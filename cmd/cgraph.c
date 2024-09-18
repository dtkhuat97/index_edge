/**
 * @file cgraph.c
 * @author FR
 */
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>
#include <inttypes.h>
#include <unistd.h>

#include <serd-0/serd/serd.h>

#include <cgraph.h>
#include <constants.h>
#include "arith.h"

// used to convert the default values of the compression to a string
#define STRINGIFY( x) #x
#define STR(x) STRINGIFY(x)

static void print_usage(bool error) {
	static const char* usage_str = \
	"Usage: cgraph-cli\n"
	"    -h,--help                       show this help\n"
	"\n"
	" * to compress a RDF graph:\n"
	"   cgraph-cli [options] [input] [output]\n"
	"                       [input]          input file of the RDF graph\n"
	"                       [output]         output file of the compressed graph\n"
	"\n"
	"   optional options:\n"
	"    -f,--format        [format]         format of the RDF graph; keep empty to auto detect the format\n"
	"                                        possible values: \"turtle\", \"ntriples\", \"nquads\", \"trig\", \"hyperedge\"\n"
	"       --overwrite                      overwrite if the output file exists\n"
	"    -v,--verbose                        print advanced information\n"
	"\n"
	"   options to influence the resulting size and the runtime to browse the graph (optional):\n"
	"       --max-rank      [rank]           maximum rank of edges, set to 0 to remove limit (default: " STR(DEFAULT_MAX_RANK) ")\n"
	"       --monograms                      enable the replacement of monograms\n"
	"       --factor        [factor]         number of blocks of a bit sequence that are grouped into a superblock (default: " STR(DEFAULT_FACTOR) ")\n"
	"       --sampling      [sampling]       sampling value of the dictionary; a value of 0 disables sampling (default: " STR(DEFAULT_SAMPLING) ")\n"
	"       --no-rle                         disable run-length encoding\n"
	"       --no-table                       do not add an extra table to speed up the decompression of the edges for an specific label\n"
#ifdef RRR
	"       --rrr                            use bitsequences based on R. Raman, V. Raman, and S. S. Rao [experimental]\n"
	"                                        --factor can also be applied to this type of bit sequences\n"
#endif
	"\n"
	" * to read a compressed RDF graph:\n"
	"   cgraph-cli [options] [input] [commands...]\n"
	"                       [input]      input file of the compressed RDF graph\n"
	"\n"
	"   optional options:\n"
	"    -f,--format        [format]         default format for the RDF graph at the command `--decompress`\n"
	"                                        possible values: \"turtle\", \"ntriples\", \"nquads\", \"trig\"\n"
	"       --overwrite                      overwrite if the output file exists, used with `--decompress`\n"
	"\n"
	"   commands to read the compressed path:\n"
	"       --decompress    [RDF graph]      decompresses the given compressed RDF graph\n"
	"       --extract-node  [node-id]        extracts the node label of the given node id\n"
	"       --extract-edge  [edge-id]        extracts the edge label of the given edge id\n"
	"       --locate-node   [text]           determines the node id of the node with the given node label\n"
	"       --locate-edge   [text]           determines the edge label id for the given text\n"
	"       --locatep-node  [text]           determines the node ids that have labels starting with the given text\n"
	"       --search-node   [text]           determines the node ids with labels containing the given text\n"
    "       --hyperedges    [rank,label]*{,node}\n"
    "                                        determines the edges with given rank. You can specify any number of nodes\n"
    "                                        that will be checked the edge is connected to. The incidence-type is given\n"
    "                                        implicitly. The label must not be set, use ? otherwise. For example:\n"
    "                                        - \"4,2,?,3,?,4\": determines all rank 4 edges with label 2 that are connected\n"
    "                                           to the node 3 with connection-type 2 and node 4 with connection-type 4.\n"
    "                                        - \"2,?,?,5\": determines all rank 2 edges any label that are connected\n"
    "                                           to the node 5 with connection-type 1. In the sense of regular edges, \n"
    "                                           this asks for all incoming edges of node 5.\n"
    "                                        Note that it is not allowed to pass no label and no nodes to this function.\n"
    "                                        Use --decompress in this case.\n"
	"       --node-count                     returns the number of nodes in the graph\n"
	"       --edge-labels                    returns the number of different edge labels in the graph\n"
	;

	FILE* os = error ? stderr : stdout;
	fprintf(os, "%s", usage_str);
}

enum opt {
	OPT_HELP = 'h',
	OPT_VERBOSE = 'v',

	OPT_CR_FORMAT = 'f',
	OPT_CR_OVERWRITE = 1000,

	OPT_C_MAX_RANK,
	OPT_C_MONOGRAMS,
	OPT_C_FACTOR,
	OPT_C_SAMPLING,
	OPT_C_NO_RLE,
	OPT_C_NO_TABLE,
#ifdef RRR
	OPT_C_RRR,
#endif

	OPT_R_DECOMPRESS,
	OPT_R_EXTRACT_NODE,
	OPT_R_EXTRACT_EDGE,
	OPT_R_LOCATE_NODE,
	OPT_R_LOCATE_EDGE,
	OPT_R_LOCATEP_NODE,
	OPT_R_SEARCH_NODE,
	OPT_R_EDGES,
    OPT_R_HYPEREDGES,
	OPT_R_NODE_COUNT,
	OPT_R_EDGE_LABELS,
	OPT_R_LOCATE_INDEX,
	OPT_R_INDEX_BETWEEN,
};

typedef enum {
	CMD_NONE = 0,
	CMD_DECOMPRESS,
	CMD_EXTRACT_NODE,
	CMD_EXTRACT_EDGE,
	CMD_LOCATE_NODE,
	CMD_LOCATE_EDGE,
	CMD_LOCATEP_NODE,
	CMD_SEARCH_NODE,
	CMD_EDGES,
    CMD_HYPEREDGES,
	CMD_NODE_COUNT,
	CMD_EDGE_LABELS,
	CMD_LOCATE_INDEX,
	CMD_INDEX_BETWEEN,
} CGraphCmd;

typedef struct {
	CGraphCmd cmd;
	union {
		char* arg_str;
		uint64_t arg_int;
	};
} CGraphCommand;

typedef struct {
	// -1 = unknown, 0 = compress, 1 = decompress
	int mode;

	bool verbose;
	char* format;
	bool overwrite;

	// options for compression
	CGraphCParams params;

	// options for reading
	int command_count;
	CGraphCommand commands[1024];
} CGraphArgs;

#define check_mode(mode_compress, mode_read, compress_expected) \
do { \
	if(compress_expected) { \
		if(mode_read) { \
			fprintf(stderr, "option '--%s' not allowed when compressing\n", options[longid].name); \
			return -1; \
		} \
		mode_compress = true; \
	} \
	else { \
		if(mode_compress) { \
			fprintf(stderr, "option '--%s' not allowed when reading the compressed graph\n", options[longid].name); \
			return -1; \
		} \
		mode_read = true; \
	} \
} while(0)

#define add_command_str(argd, c) \
do { \
	if((argd)->command_count == (sizeof((argd)->commands) / sizeof(*(argd)->commands))) { \
		fprintf(stderr, "exceeded the maximum number of commands\n"); \
		return -1; \
	} \
	(argd)->commands[(argd)->command_count].cmd = (c); \
	(argd)->commands[(argd)->command_count].arg_str = optarg; \
	(argd)->command_count++; \
} while(0)

#define add_command_int(argd, c) \
do { \
	if((argd)->command_count == (sizeof((argd)->commands) / sizeof(*(argd)->commands))) { \
		fprintf(stderr, "exceeded the maximum number of commands\n"); \
		return -1; \
	} \
	if(parse_optarg_int(&v) < 0) \
		return -1; \
	(argd)->commands[(argd)->command_count].cmd = (c); \
	(argd)->commands[(argd)->command_count].arg_int = v; \
	(argd)->command_count++; \
} while(0)

#define add_command_none(argd, c) \
do { \
	if((argd)->command_count == (sizeof((argd)->commands) / sizeof(*(argd)->commands))) { \
		fprintf(stderr, "exceeded the maximum number of commands\n"); \
		return -1; \
	} \
	(argd)->commands[(argd)->command_count].cmd = (c); \
	(argd)->command_count++; \
} while(0)

static const char* parse_int(const char* s, uint64_t* value) {
	if(!s || *s == '-') // handle negative values because `strtoull` ignores them
		return NULL;

	char* temp;
	uint64_t val = strtoull(s, &temp, 0);
	if(temp == s || ((val == LONG_MIN || val == LONG_MAX) && errno == ERANGE))
		return NULL;

	*value = val;
	return temp;
}

static inline int parse_optarg_int(uint64_t* value) {
	const char* temp = parse_int(optarg, value);
	if(!temp || *temp != '\0')
		return -1;
	return 0;
}

static int parse_args(int argc, char** argv, CGraphArgs* argd) {
	static struct option options[] = {
		// basic options
		{"help", no_argument, 0, OPT_HELP},
		{"verbose", no_argument, 0, OPT_VERBOSE},

		// options for compression or reading
		{"format",  required_argument, 0, OPT_CR_FORMAT},
		{"overwrite", no_argument, 0, OPT_CR_OVERWRITE},

		// options used for compression
		{"max-rank", required_argument, 0, OPT_C_MAX_RANK},
		{"monograms", no_argument, 0, OPT_C_MONOGRAMS},
		{"factor", required_argument, 0, OPT_C_FACTOR},
		{"sampling", required_argument, 0, OPT_C_SAMPLING},
		{"no-rle", no_argument, 0, OPT_C_NO_RLE},
		{"no-table", no_argument, 0, OPT_C_NO_TABLE},
#ifdef RRR
		{"rrr", no_argument, 0, OPT_C_RRR},
#endif

		// options used for browsing
		{"decompress", required_argument, 0, OPT_R_DECOMPRESS},
		{"extract-node", required_argument, 0, OPT_R_EXTRACT_NODE},
		{"extract-edge", required_argument, 0, OPT_R_EXTRACT_EDGE},
		{"locate-node", required_argument, 0, OPT_R_LOCATE_NODE},
		{"locate-edge", required_argument, 0, OPT_R_LOCATE_EDGE},
		{"locatep-node", required_argument, 0, OPT_R_LOCATEP_NODE},
		{"search-node", required_argument, 0, OPT_R_SEARCH_NODE},
		{"edges", required_argument, 0, OPT_R_EDGES},
        {"hyperedges", required_argument, 0, OPT_R_HYPEREDGES},
		{"node-count", no_argument, 0, OPT_R_NODE_COUNT},
		{"edge-labels", no_argument, 0, OPT_R_EDGE_LABELS},
		{"locate-index", required_argument, 0, OPT_R_LOCATE_INDEX},
		{"index-between", required_argument, 0, OPT_R_INDEX_BETWEEN},
		{0, 0, 0, 0}
	};

	bool mode_compress = false;
	bool mode_read = false;

	argd->mode = -1;
	argd->verbose = false;
	argd->format = NULL;
	argd->overwrite = false;
	argd->params.max_rank = DEFAULT_MAX_RANK;
	argd->params.monograms = DEFAULT_MONOGRAMS;
	argd->params.factor = DEFAULT_FACTOR;
	argd->params.sampling = DEFAULT_SAMPLING;
	argd->params.rle = DEFAULT_RLE;
	argd->params.nt_table = DEFAULT_NT_TABLE;
#ifdef RRR
	argd->params.rrr = DEFAULT_RRR;
#endif
	argd->command_count = 0;

	uint64_t v;
	int opt, longid;
	while((opt = getopt_long(argc, argv, "hvf:", options, &longid)) != -1) {
		switch(opt) {
		case OPT_HELP:
			print_usage(false);
			exit(0);
			break;
		case OPT_VERBOSE:
			// TODO: only when compressing a graph
			argd->verbose = 1;
			break;
		case OPT_CR_FORMAT:
			argd->format = optarg;
			break;
		case OPT_CR_OVERWRITE:
			argd->overwrite = true;
			break;
		case OPT_C_MAX_RANK:
			check_mode(mode_compress, mode_read, true);
			if(parse_optarg_int(&v) < 0) {
				fprintf(stderr, "max-rank: expected integer\n");
				return -1;
			}

			argd->params.max_rank = v;
			break;
		case OPT_C_MONOGRAMS:
			check_mode(mode_compress, mode_read, true);
			argd->params.monograms = true;
			break;
		case OPT_C_FACTOR:
			check_mode(mode_compress, mode_read, true);
			if(parse_optarg_int(&v) < 0) {
				fprintf(stderr, "factor: expected integer\n");
				return -1;
			}

			argd->params.factor = v;
			break;
		case OPT_C_SAMPLING:
			check_mode(mode_compress, mode_read, true);
			if(parse_optarg_int(&v) < 0) {
				fprintf(stderr, "sampling: expected integer\n");
				return -1;
			}

			argd->params.sampling = v;
			break;
		case OPT_C_NO_RLE:
			check_mode(mode_compress, mode_read, true);
			argd->params.rle = false;
			break;
		case OPT_C_NO_TABLE:
			check_mode(mode_compress, mode_read, true);
			argd->params.nt_table = false;
			break;
#ifdef RRR
		case OPT_C_RRR:
			check_mode(mode_compress, mode_read, true);
			argd->params.rrr = true;
			break;
#endif
		case OPT_R_DECOMPRESS:
			check_mode(mode_compress, mode_read, false);
			add_command_str(argd, CMD_DECOMPRESS);
			break;
		case OPT_R_EXTRACT_NODE:
			check_mode(mode_compress, mode_read, false);
			add_command_int(argd, CMD_EXTRACT_NODE);
			break;
		case OPT_R_EXTRACT_EDGE:
			check_mode(mode_compress, mode_read, false);
			add_command_int(argd, CMD_EXTRACT_EDGE);
			break;
		case OPT_R_LOCATE_NODE:
			check_mode(mode_compress, mode_read, false);
			add_command_str(argd, CMD_LOCATE_NODE);
			break;
		case OPT_R_LOCATE_EDGE:
			check_mode(mode_compress, mode_read, false);
			add_command_str(argd, CMD_LOCATE_EDGE);
			break;
		case OPT_R_LOCATEP_NODE:
			check_mode(mode_compress, mode_read, false);
			add_command_str(argd, CMD_LOCATEP_NODE);
			break;
		case OPT_R_SEARCH_NODE:
			check_mode(mode_compress, mode_read, false);
			add_command_str(argd, CMD_SEARCH_NODE);
			break;
		case OPT_R_EDGES:
			check_mode(mode_compress, mode_read, false);
			add_command_str(argd, CMD_EDGES);
			break;
        case OPT_R_HYPEREDGES:
            check_mode(mode_compress, mode_read, false);
            add_command_str(argd, CMD_HYPEREDGES);
            break;
		case OPT_R_NODE_COUNT:
			check_mode(mode_compress, mode_read, false);
			add_command_none(argd, CMD_NODE_COUNT);
			break;
		case OPT_R_EDGE_LABELS:
			check_mode(mode_compress, mode_read, false);
			add_command_none(argd, CMD_EDGE_LABELS);
			break;
		case OPT_R_LOCATE_INDEX:
			check_mode(mode_compress, mode_read, false);
			add_command_str(argd, CMD_LOCATE_INDEX);
			break;
		case OPT_R_INDEX_BETWEEN:
			check_mode(mode_compress, mode_read, false);
			add_command_str(argd, CMD_INDEX_BETWEEN);
			break;
		case '?':
		case ':':
		default:
			return -1;
		}
	}

	if(mode_compress)
		argd->mode = 0;
	else if(mode_read)
		argd->mode = 1;

	return optind;
}

typedef struct {
	SerdSyntax syntax;
	const char* name;
	const char* extension;
} Syntax;

static const Syntax syntaxes[] = {
	{SERD_TURTLE, "turtle", ".ttl"},
	{SERD_NTRIPLES, "ntriples", ".nt"},
	{SERD_NQUADS, "nquads", ".nq"},
	{SERD_TRIG, "trig", ".trig"},
    {(SerdSyntax) 5, "hyperedge", ".hyperedge"},
	{(SerdSyntax) 0, NULL, NULL}
};

static SerdSyntax get_format(const char* format) {
	for(const Syntax* s = syntaxes; s->name; s++) {
		if(!strncasecmp(s->name, format, strlen(format))) {
			return s->syntax;
		}
	}

	return (SerdSyntax) 0;
}
static SerdSyntax guess_format(const char* filename) {
	const char* ext = strrchr(filename, '.');
	if(ext) {
		for(const Syntax* s = syntaxes; s->name; s++) {
			if(!strncasecmp(s->extension, ext, strlen(ext))) {
				return s->syntax;
			}
		}
	}

	return (SerdSyntax) 0;
}

typedef struct {
	SerdEnv* env;
	CGraphW* handler;
} CGraphParserContext;

static void free_handle(void* v) {
	(void) v;
}

static SerdStatus base_sink(void* handle, const SerdNode* uri) {
	CGraphParserContext* ctx = handle;
	SerdEnv* env = ctx->env;
	return serd_env_set_base_uri(env, uri);
}

static SerdStatus prefix_sink(void* handle, const SerdNode* name, const SerdNode* uri) {
	CGraphParserContext* ctx = handle;
	SerdEnv* env = ctx->env;
	return serd_env_set_prefix(env, name, uri);
}

//int index_storage = 999999999;
//u_int64_t index_storage = 1;
size_t edge_index= 0;
//char edge_index[20];
int MAXLINE = 11;
static SerdStatus statement_sink(void* handle, SerdStatementFlags flags, const SerdNode* graph, const SerdNode* subject, const SerdNode* predicate, const SerdNode* object, const SerdNode* object_datatype, const SerdNode* object_lang) {
	CGraphParserContext* ctx = handle;
	SerdEnv* env = ctx->env;
	CGraphW* handler = ctx->handler;


	const char* s = (const char*) subject->buf;
	const char* p = (const char*) predicate->buf;
	const char* o = (const char*) object->buf;
	//char* edge_index = (char *) malloc(MAXLINE);
	//sprintf(edge_index, "%i", index_storage);
	//sprintf(edge_index, "%lu", index_storage);
	
	const int edge_rank = 3;
	//const char* so[3] = {s, o, edge_index};
	const char* so[2] = {s, o};
	if(cgraphw_add_edge(handler, edge_rank, p, so, edge_index) < 0)
	{
		return SERD_FAILURE;
	}
	edge_index=edge_index+1;
	//free(edge_index);
	return SERD_SUCCESS;
}

static SerdStatus end_sink(void* handle, const SerdNode* node) {
	return SERD_SUCCESS;
}

static SerdStatus error_sink(void* handle, const SerdError* error) {
	fprintf(stderr, "error: %s:%u:%u: %s", error->filename, error->col, error->line, error->fmt);
	return SERD_SUCCESS;
}

static int rdf_parse(const uint8_t* filename, SerdSyntax syntax, CGraphW* g) {
	int res = -1;

	SerdURI base_uri = SERD_URI_NULL;
	SerdNode base = SERD_NODE_NULL;

	SerdEnv* env = serd_env_new(&base);
	if(!env)
		return -1;

	CGraphParserContext ctx;
	ctx.env = env;
	ctx.handler = g;

	base = serd_node_new_file_uri(filename, NULL, &base_uri, true);

	SerdReader* reader = serd_reader_new(syntax, &ctx, free_handle, base_sink, prefix_sink, statement_sink, end_sink);
	if(!reader)
		goto exit_0;

	serd_reader_set_strict(reader, true);

	bool err = false;
	serd_reader_set_error_sink(reader, error_sink, &err);

	FILE* in_fd = fopen((const char*) filename, "r");
	if(!in_fd)
		goto exit_1;

	//FALSE
	SerdStatus status = serd_reader_start_stream(reader, in_fd, filename, false);
	while(status == SERD_SUCCESS)
		status = serd_reader_read_chunk(reader);

	fclose(in_fd);

	if(!err)
		res = 0;

exit_1:
	serd_reader_end_stream(reader);
	serd_reader_free(reader);
exit_0:
	serd_env_free(env);
	serd_node_free(&base);

	return res;
}

#define MAX_LINE_LENGTH 1024
static int hyperedge_parse(const uint8_t* filename, SerdSyntax syntax, CGraphW* g) {
    int res = -1;

    bool err = false;

    FILE* in_fd = fopen((const char*) filename, "r");
    if(!in_fd)
        return res;

    char line[MAX_LINE_LENGTH];
    char* n[128];
    size_t cn = 0;
    while (fgets(line, sizeof(line), in_fd) && !err) {
        cn = 0;
        // Process each line of the hyperedge file
        // Split the line into individual items using strtok
        char* token = strtok(line, " \t\n"); //Use empty space, tab, and newline as delimiter.
        while (token != NULL) {
            n[cn++] = token;
            if (cn == 128)
                return -1; //Allowed number of parameters are exceeded.
            token = strtok(NULL, " \t\n");
        }
        if (cgraphw_add_edge(g, cn-1, n[0], (const char **) (n + 1), -1) < 0) {
            err = true;
        }
    }

    fclose(in_fd);

    if(!err)
        res = 0;

    return res;
}

static int do_compress(const char* input, const char* output, const CGraphArgs* argd) {
	if(!argd->overwrite) {
		if(access(output, F_OK) == 0) {
			fprintf(stderr, "Output file \"%s\" already exists.\n", output);
			return -1;
		}
	}

	SerdSyntax syntax;
	if(argd->format)
		syntax = get_format(argd->format);
	else
		syntax = guess_format(input);
	if(!syntax)
		syntax = SERD_TURTLE;

	if(argd->verbose) {
		printf("Compression parameters:\n");
		printf("- max-rank: %d\n", argd->params.max_rank);
		printf("- monograms: %s\n", argd->params.monograms ? "true" : "false");
		printf("- factor: %d\n", argd->params.factor);
		printf("- sampling: %d\n", argd->params.sampling);
		printf("- rle: %s\n", argd->params.rle ? "true" : "false");
		printf("- nt-table: %s\n", argd->params.nt_table ? "true" : "false");
#ifdef RRR
		printf("- rrr: %s\n", argd->params.rrr ? "true" : "false");
#endif
	}

	CGraphW* g = cgraphw_init();

	if(!g)
		return -1;

	cgraphw_set_params(g, &argd->params);

	int res = -1;

    if (syntax != 5)
    {
        if(argd->verbose)
            printf("Parsing RDF file %s\n", input);
        if(rdf_parse((const uint8_t*) input, syntax, g) < 0) {
            fprintf(stderr, "Failed to read file \"%s\".\n", input);
            goto exit_0;
        }
    }
    else
    {
        if(argd->verbose)
            printf("Parsing Hyperedge file %s\n", input);
        if(hyperedge_parse((const uint8_t*) input, syntax, g) < 0) {
            fprintf(stderr, "Failed to read file \"%s\".\n", input);
            goto exit_0;
        }
    }


	if(argd->verbose)
		printf("Applying repair compression\n");
	
	//only for checking file data
	

	clock_t start, end;
	double cpu_time_used;
	start = clock();
	if(cgraphw_compress(g, edge_index) < 0) {
		fprintf(stderr, "failed to compress graph\n");
		goto exit_0;
	}
	end = clock();
	cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
	FILE *fpt;
	//fpt = fopen("/home/dtkhuat/results/random_graphs_compressed/IndexedEdges_compressed/var_num_edge_labels/timings03_06_2024.csv", "a");
	//fpt = fopen("/home/dtkhuat/results/times/11_06_2024.csv", "a");
	fpt = fopen("/home/dtkhuat/results/random_graphs_compressed/IndexedEdges_compressed/var_num_edge_labels3/timings_23_07_2024.csv", "a");
	fprintf(fpt, "%f", cpu_time_used);
	fclose(fpt);
	
	if(argd->verbose)
		printf("Writing compressed graph to %s\n", output);

	if(cgraphw_write(g, output, argd->verbose) < 0) {
		fprintf(stderr, "failed to write compressed graph\n");
		goto exit_0;
	}

	res = 0;

exit_0:
	cgraphw_destroy(g);
	return res;
}

static char* rdf_node(CGraphR* g, uint64_t v, bool is_node, SerdNode* node) {
	char* s;
	if(is_node)
		s = cgraphr_extract_node(g, v, NULL);
	else
		s = cgraphr_extract_edge_label(g, v, NULL);
	if(!s)
		return NULL;

    if (node != NULL)
    {
        if (!*s) // empty
            *node = serd_node_from_string(SERD_BLANK, (const uint8_t *) s);
        else if (!strncmp("http://", s, strlen("http://")) || !strncmp("http://", s, strlen("http://"))) // is url
            *node = serd_node_from_string(SERD_URI, (const uint8_t *) s);
        else // literal
            *node = serd_node_from_string(SERD_LITERAL, (const uint8_t *) s);
    }

	return s;
}

static int do_decompress(CGraphR* g, const char* output, const char* format, bool overwrite) {
	int res = -1;

	if(!overwrite) {
		if(access(output, F_OK) == 0) {
			fprintf(stderr, "Output file \"%s\" already exists.\n", output);
			return -1;
		}
	}

	SerdSyntax syntax;
	if(format)
		syntax = get_format(format);
	else
		syntax = guess_format(output);
	if(!syntax)
		syntax = SERD_TURTLE;


    FILE* out_fd = fopen((const char*) output, "w+");
    if(!out_fd) {
        fprintf(stderr, "Failed to write to file \"%s\".\n", output);
        goto exit_0;
    }

    if (syntax == 5) // Syntax is hyperedge file
    {
        char *label, *txt;
        size_t label_count = cgraphr_edge_label_count(g);
        for (CGraphEdgeLabel l = 0; l < label_count; l++)
        {
            label = rdf_node(g, l, false, NULL);
            CGraphEdgeIterator *it = cgraphr_edges_by_predicate(g, l);
            if (!it) {
                free(label);
                goto exit_0;
            }

            CGraphEdge n;
            while (cgraphr_edges_next(it, &n)) {
                //write label here.
                if (fprintf(out_fd, "%s", label) == EOF)
                {
                    free(label);
                    cgraphr_edges_finish(it);
                    goto exit_0;
                }
                for (CGraphRank i = 0; i < n.rank; i++)
                {
                    txt = rdf_node(g, n.nodes[i], true, NULL);
                    //Write empty space plus txt here;
                    int suc = fprintf(out_fd, " %s", txt);
                    if (suc  == EOF) {
                        free(label);
                        free(txt);
                        cgraphr_edges_finish(it);
                        goto exit_0;
                    }
                    free(txt);
                }
                if (fprintf(out_fd, "\n") == EOF)
                {
                    free(label);
                    cgraphr_edges_finish(it);
                    goto exit_0;
                }
            }
            free(label);
        }
        res = 0;
    }
    else {

		/*
        SerdURI base_uri = SERD_URI_NULL;
        SerdNode base = SERD_NODE_NULL;

        SerdEnv *env = serd_env_new(&base);
        if (!env)
            return -1;

        base = serd_node_new_file_uri((const uint8_t *) output, NULL, &base_uri, true);

        SerdStyle output_style = SERD_STYLE_ABBREVIATED | SERD_STYLE_ASCII | SERD_STYLE_BULK;

        SerdWriter *writer = serd_writer_new(syntax, output_style, env, &base_uri, serd_file_sink, out_fd);
        if (!writer)
            goto exit_1;

        bool err = false;
        serd_writer_set_error_sink(writer, error_sink, &err);

        char *txt_s, *txt_p, *txt_o;
        SerdNode s, p, o;


		//For schleifen auf 0 setzen
        size_t node_count = cgraphr_node_count(g);
        for (size_t v = 1; v < node_count; v++) {
            
			
			//CGraphNode nodes[] = {v, -1};
            CGraphNode nodes[] = {-1, -1, v};
			//HIER HINZUGEFÜGT
			CGraphEdgeIterator *it = cgraphr_edges(g, 3, CGRAPH_LABELS_ALL, nodes);
            //CGraphEdgeIterator *it = cgraphr_edges(g, 2, CGRAPH_LABELS_ALL, nodes);
            if (!it) {
                free(txt_s);
                goto exit_2;
            }

            CGraphEdge n;
            while (cgraphr_edges_next(it, &n)) {
				txt_s = rdf_node(g, n.nodes[0], true, &s);
                txt_p = rdf_node(g, n.label, false, &p);
                txt_o = rdf_node(g, n.nodes[1], true, &o);

                int res = serd_writer_write_statement(writer, 0, &base, &s, &p, &o, NULL, NULL);

                if (res != SERD_SUCCESS || err) {
                    free(txt_s);
                    cgraphr_edges_finish(it);
                    goto exit_2;
                }
				free(txt_s);
				free(txt_p);
                free(txt_o);
				
            }

            
        }

        res = 0;

        exit_2:
        serd_writer_finish(writer);
        serd_writer_free(writer);
        exit_1:
        serd_env_free(env);
        serd_node_free(&base);
	*/

/*
		char *label, *txt;
        size_t label_count = cgraphr_edge_label_count(g);
        for (CGraphEdgeLabel l = 0; l < label_count; l++)
        {
            label = rdf_node(g, l, false, NULL);
            CGraphEdgeIterator *it = cgraphr_edges_by_predicate(g, l);
            if (!it) {
                free(label);
                goto exit_0;
            }

            CGraphEdge n;
            while (cgraphr_edges_next(it, &n)) {
                //write label here.
                if (fprintf(out_fd, "%s", label) == EOF)
                {
                    free(label);
                    cgraphr_edges_finish(it);
                    goto exit_0;
                }
                for (CGraphRank i = 0; i < n.rank-1; i++)
                {
                    txt = rdf_node(g, n.nodes[i], true, NULL);
                    //Write empty space plus txt here;
                    int suc = fprintf(out_fd, " %s", txt);
                    if (suc  == EOF) {
                        free(label);
                        free(txt);
                        cgraphr_edges_finish(it);
                        goto exit_0;
                    }
                    free(txt);
                }
                if (fprintf(out_fd, "\n") == EOF)
                {
                    free(label);
                    cgraphr_edges_finish(it);
                    goto exit_0;
                }
            }
			
            free(label);

		
        }
        res = 0;
	*/
		char *label, *txt;
        char *txt_s, *txt_p, *txt_o;
        SerdNode s, p, o;


		//For schleifen auf 0 setzen
        size_t node_count = cgraphr_node_count(g);
        for (size_t v = 0; v < node_count; v++) {
            
			
			//CGraphNode nodes[] = {v, -1};
            CGraphNode nodes[] = {-1, -1, v};
			//HIER HINZUGEFÜGT
			CGraphEdgeIterator *it = cgraphr_edges(g, 3, CGRAPH_LABELS_ALL, nodes);
            //CGraphEdgeIterator *it = cgraphr_edges(g, 2, CGRAPH_LABELS_ALL, nodes);
            if (!it) {
                free(txt_s);
            }

            CGraphEdge n;
            while (cgraphr_edges_next(it, &n)) {
                //write label here.
				label = rdf_node(g, n.label, false, NULL);
                if (fprintf(out_fd, "%s", label) == EOF)
                {
                    free(label);
                    cgraphr_edges_finish(it);
                    goto exit_0;
                }
                for (CGraphRank i = 0; i < n.rank-1; i++)
                {
                    txt = rdf_node(g, n.nodes[i], true, NULL);
                    //Write empty space plus txt here;
                    int suc = fprintf(out_fd, " %s", txt);
                    if (suc  == EOF) {
                        free(label);
                        free(txt);
                        cgraphr_edges_finish(it);
                        goto exit_0;
                    }
                    free(txt);
                }
                if (fprintf(out_fd, "\n") == EOF)
                {
                    free(label);
                    cgraphr_edges_finish(it);
                    goto exit_0;
                }
			}
            
        }

        res = 0;
    }
exit_0:
    fclose(out_fd);
	return res;
}

// ******* Helper functions *******

typedef struct {
	uint64_t node_src;
	uint64_t node_dst;
	uint64_t label;
} EdgeArg;

int parse_edge_arg(const char* s, EdgeArg* arg) {
	uint64_t node_src, node_dst, label;

	s = parse_int(s, &node_src);
	if(!s)
		return -1;

	switch(*s) {
	case '\0':
		node_dst = -1;
		label = -1;
		goto exit;
	case ',':
		break;
	default:
		return -1;
	}

	if(*(++s) == '?') {
		s++;
		node_dst = -1;
	}
	else {
		s = parse_int(s, &node_dst);
		if(!s)
			return -1;
	}

	switch(*s) {
	case '\0':
		label = -1;
		goto exit;
	case ',':
		break;
	default:
		return -1;
	}

	s = parse_int(s + 1, &label);
	if(!s || *s != '\0')
		return -1;

exit:
	arg->node_src = node_src;
	arg->node_dst = node_dst;
	arg->label = label;
	return 0;
}

typedef struct {

    /* The rank of the edge. */
    CGraphRank rank;

    /* The edge label of the edge. */
    CGraphEdgeLabel label;

    /* The nodes of the edge. */
    CGraphNode nodes[128];
} HyperedgeArg;

int parse_hyperedge_arg(const char* s, HyperedgeArg* arg, bool* exists_query, bool* predicate_query) {
    uint64_t rank, label;
    *exists_query = true;
    *predicate_query = true;

    s = parse_int(s, &rank);
    if(!s)
        return -1;

    for (int j=0; j < rank; j++) {
        arg->nodes[j] = -1;
    }

    switch(*s) {
        case '\0':
            rank = -1;
            label = -1;
            goto exit;
        case ',':
            break;
        default:
            return -1;
    }

    if(*(++s) == '?') {
        s++;
        label = -1;
        *exists_query = false;
    }
    else {
        s = parse_int(s, &label);
        if(!s)
            return -1;
    }

    for (int npc = 0; npc < rank && * s == ','; npc++) {
        if(*(++s) == '?') {
            s++;
            *exists_query = false;
            // Already initialized all values with -1.
        }
        else {
            s = parse_int(s, (uint64_t *) &arg->nodes[npc]);
            *predicate_query = false;
            if (!s)
                return -1;
        }
    }

    exit:
    arg->rank = rank;
    arg->label = label;
    return 0;
}
//parse_index_between_arg
int parse_index_arg(const char* s, HyperedgeArg* arg) {
	arg->rank = 3;
	arg->label = -1;
	//
    for (int j=0; j < arg->rank; j++) {
        arg->nodes[j] = -1;
    }
	s = parse_int(s, (uint64_t *) &arg->nodes[2]);
	if(!s)
        return -1;
	
	switch(*s) {
        case '\0':
			return 1;
        default:
            return -1;
    }
}

int parse_index_between_arg(const char* s, HyperedgeArg* arg, int pos1, int pos2) {
    arg->rank = 3;
	arg->label = -1;

    for (int j=0; j < arg->rank; j++) {
        arg->nodes[j] = -1;
    }

	s = parse_int(s, &arg->nodes[pos1]);
    if(!s)
        return -1;

    switch(*s) {
        case ',':
            break;
        default:
            return -1;
    }
	s++;
	s = parse_int(s, (uint64_t *) &arg->nodes[pos2]);

	switch(*s) {
        case '\0':
			return 1;
        default:
            return -1;
    }

}

// comparing two nodes
static int cmp_node(const void* e1, const void* e2)  {
	CGraphNode* v1 = (CGraphNode*) e1;
	CGraphNode* v2 = (CGraphNode*) e2;

	int64_t cmpn = *v1 - *v2;
	if(cmpn > 0) return  1;
	if(cmpn < 0) return -1;
	return 0;
}

typedef struct {
	size_t len;
	size_t cap;
	CGraphNode* data;
} NodeList;

// low level list with capacity
void node_append(NodeList* l, CGraphNode n) {
	size_t cap = l->cap;
	size_t len = l->len;

	if(cap == len) {
		cap = !cap ? 8 : (cap + (cap >> 1));
		CGraphNode* data = realloc(l->data, cap * sizeof(*data));
		if(!data)
			exit(1);

		l->cap = cap;
		l->data = data;
	}

	l->data[l->len++] = n;
}

// comparing two edges to sort it
//  1. by label (higher label first)
//  2. by node (higher node first) and
//  3. by rank (higher rank first)
static int cmp_edge(const void* e1, const void* e2)  {

	CGraphEdge* v1 = (CGraphEdge*) e1;
	CGraphEdge* v2 = (CGraphEdge*) e2;

    int64_t cmp = v1->label - v2->label;
	if(cmp > 0) return  1;
	if(cmp < 0) return -1;
    for (int j=0; j < MIN(v1->rank, v2->rank); j++)
    {
        cmp = v1->nodes[j] - v2->nodes[j];
        if(cmp > 0) return  1;
        if(cmp < 0) return -1;
    }
    cmp = v1->rank - v2->rank;
    if(cmp > 0) return  1;
    if(cmp < 0) return -1;
	return 0;
}

typedef struct {
	size_t len;
	size_t cap;
	CGraphEdge* data;
} EdgeList;

// low level list with capacity
void edge_append(EdgeList* l, CGraphEdge* e) {
	size_t cap = l->cap;
	size_t len = l->len;

	if(cap == len) {
		cap = !cap ? 8 : (cap + (cap >> 1));
		CGraphEdge* data = realloc(l->data, cap * sizeof(*data));
		if(!data)
			exit(1);

		l->cap = cap;
		l->data = data;
	}

	l->data[l->len++] = *e;
}

static int do_read(const char* input, const CGraphArgs* argd) {
	CGraphR* g = cgraphr_init(input);
	if(!g) {
		fprintf(stderr, "failed to read compressed graph %s\n", input);
		return -1;
	}

	if(argd->command_count == 0) {
		fprintf(stderr, "no commands given\n");
		return -1;
	}

	int res = -1;

	for(int i = 0; i < argd->command_count; i++) {
		const CGraphCommand* cmd = &argd->commands[i];

		switch(cmd->cmd) {
		case CMD_DECOMPRESS:
			if(do_decompress(g, cmd->arg_str, argd->format, argd->overwrite) < 0)
				goto exit; // terminate the reading of the graph

			res = 0;
			break;
		case CMD_EXTRACT_NODE: {
			char* v = cgraphr_extract_node(g, cmd->arg_int, NULL);
			if(v) {
				printf("%s\n", v);
				free(v);
				res = 0;
			}
			else
				fprintf(stderr, "no node found for id %" PRIu64 "\n", cmd->arg_int);
			break;
		}
		case CMD_EXTRACT_EDGE: {
			char* v = cgraphr_extract_edge_label(g, cmd->arg_int, NULL);
			if(v) {
				printf("%s\n", v);
				free(v);
				res = 0;
			}
			else
				fprintf(stderr, "no edge found for id %" PRIu64 "\n", cmd->arg_int);
			break;
		}
		case CMD_LOCATE_NODE: {
			CGraphNode n = cgraphr_locate_node(g, cmd->arg_str);
			if(n >= 0) {
				printf("%" PRId64 "\n", n);
				res = 0;
			}
			else
				fprintf(stderr, "node \"%s\" does not exists\n", cmd->arg_str);
			break;
		}
		case CMD_LOCATE_EDGE: {
			CGraphNode n = cgraphr_locate_edge_label(g, cmd->arg_str);
			if(n >= 0) {
				printf("%" PRId64 "\n", n);
				res = 0;
			}
			else
				fprintf(stderr, "edge label \"%s\" does not exists\n", cmd->arg_str);
			break;
		}
		case CMD_LOCATEP_NODE:
			// fallthrough
		case CMD_SEARCH_NODE: {
			CGraphNodeIterator* it = (cmd->cmd == CMD_LOCATEP_NODE) ?
				cgraphr_locate_node_prefix(g, cmd->arg_str) :
				cgraphr_search_node(g, cmd->arg_str);
			if(!it)
				break;

			CGraphNode n;
			NodeList ls = {0}; // list is empty
			while(cgraphr_node_next(it, &n))
				node_append(&ls, n);

			// sort the nodes
			qsort(ls.data, ls.len, sizeof(CGraphNode), cmp_node);

			for(size_t i = 0; i < ls.len; i++)
				printf("%" PRId64 "\n", ls.data[i]);

			if(ls.data)
				free(ls.data);

			res = 0;
			break;
		}
		case CMD_EDGES:
//        {
//			EdgeArg arg;
//			if(parse_edge_arg(cmd->arg_str, &arg) < 0) {
//				fprintf(stderr, "failed to parse edge argument \"%s\"\n", cmd->arg_str);
//				break;
//			}
//
//			if(arg.label != CGRAPH_LABELS_ALL && arg.node_dst != -1) {
//				bool exists = cgraphr_edge_exists(g, arg.node_src, arg.node_dst, arg.label);
//				printf("%d\n", exists ? 1 : 0);
//				res = 0;
//				break;
//			}
//
//			CGraphEdgeIterator* it = (arg.node_dst == -1) ?
//				cgraphr_edges(g, arg.node_src, arg.label) :
//				cgraphr_edges_connecting(g, arg.node_src, arg.node_dst);
//			if(!it)
//				break;
//
//			CGraphEdge n;
//			EdgeList ls = {0}; // list is empty
//			while(cgraphr_edges_next(it, &n))
//				edge_append(&ls, &n);
//
//			// sort the edges
//			qsort(ls.data, ls.len, sizeof(CGraphEdge), cmp_edge);
//
//			for(size_t i = 0; i < ls.len; i++)
//				printf("%" PRId64 "\t%" PRId64 "\t%" PRId64 "\n", ls.data[i].node1, ls.data[i].node2, ls.data[i].label);
//
//			if(ls.data)
//				free(ls.data);
//
//			res = 0;
//			break;
//		}
        case CMD_HYPEREDGES: {
            HyperedgeArg arg;
            bool exist_query = false;
            bool predicate_query = false;
            if(parse_hyperedge_arg(cmd->arg_str, &arg, &exist_query, &predicate_query) < 0) {
                fprintf(stderr, "failed to parse edge argument \"%s\"\n", cmd->arg_str);
                break;
            }

            if (exist_query) {
                bool exists = cgraphr_edge_exists(g, arg.rank, arg.label, arg.nodes);
                printf("%d\n", exists ? 1 : 0);
                res = 0;
                break;
            }
            CGraphEdgeIterator* it;
            if (predicate_query)
            {
                it = cgraphr_edges_by_predicate(g, arg.label);
            }
            else
            {
                it = cgraphr_edges(g, arg.rank, arg.label, arg.nodes) ;
            }

            if(!it)
			{
                break;
			}

            CGraphEdge n;
            EdgeList ls = {0}; // list is empty
            while(cgraphr_edges_next(it, &n))
			{
                edge_append(&ls, &n);
			}
            // sort the edges
            //qsort(ls.data, ls.len, sizeof(CGraphEdge), cmp_edge);

			
			int bla_counter=0;
            for(size_t i = 0; i < ls.len; i++) {
                printf("(%" PRId64, ls.data[i].label);
                for (CGraphRank j = 0; j < ls.data[i].rank; j++) {

					printf(",\t%" PRId64, ls.data[i].nodes[j]);
                }
                printf(")\n");
			
            }
			
			printf("Gezählte Hyperedge %li\n",ls.len);
            for (int i = 0; i < ls.len; i++)
            {
                free(ls.data[i].nodes);
            }
            if(ls.data)
                free(ls.data);

            res = 0;
            break;
        }
		case CMD_NODE_COUNT:
		/*
			char node_num[12];
			int result = (int)cgraphr_node_count(g);
			printf("%zu\n", cgraphr_node_count(g));
			for(int i=999999999; i < 999999999+(int)cgraphr_node_count(g); i++)
			{
				sprintf(node_num, "%i", i);
				if(cgraphr_locate_node(g, node_num)>0)
				{
					result = result -1;
				}

			}
			printf("%i \n", result);
			res = 0;
			break;
		*/
			
			printf("%zu\n", cgraphr_node_count(g));
			res = 0;
			break;
		case CMD_EDGE_LABELS:
			printf("%zu\n", cgraphr_edge_label_count(g));
			res = 0;
			break;
		case CMD_LOCATE_INDEX:{
			HyperedgeArg arg;
			//search for node id, given a node label. Node label = argument of cmd 
			CGraphNode node_id = cgraphr_locate_node(g, cmd->arg_str);
			char node_id_str[12];
			sprintf(node_id_str, "%ld", node_id);
			
			//if(parse_index_arg(node_id_str, &arg) < 0) {
            if(parse_index_arg(cmd->arg_str, &arg) < 0) {
                fprintf(stderr, "failed to parse edge argument \"%s\"\n", cmd->arg_str);
                break;
 			}

            CGraphEdgeIterator* it;


			it = cgraphr_edges(g, arg.rank, arg.label, arg.nodes) ;

			
            CGraphEdge n;
            EdgeList ls = {0}; // list is empty
            while(cgraphr_edges_next(it, &n))
                edge_append(&ls, &n);

            // sort the edges
            qsort(ls.data, ls.len, sizeof(CGraphEdge), cmp_edge);

            for(size_t i = 0; i < ls.len; i++) {
                printf("(%" PRId64, ls.data[i].label);
				for (CGraphRank j = 0; j < ls.data[i].rank; j++) {
                //for (CGraphRank j = 0; j < ls.data[i].rank; j++) {
						printf(",\t%" PRId64, ls.data[i].nodes[j]);
					
                }
                printf(")\n");

            }
			printf("Gezählte Hyperedge %i\n", (int)ls.len);
            for (int i = 0; i < ls.len; i++)
            {
                free(ls.data[i].nodes);
            }
            if(ls.data)
                free(ls.data);

            res = 0;
			break;
		}
		case CMD_INDEX_BETWEEN:{
			HyperedgeArg arg;
			
			int len =0;
			//check for node1 -- node2
            if(parse_index_between_arg(cmd->arg_str, &arg, 0, 1) < 0) {
                fprintf(stderr, "failed to parse edge argument \"%s\"\n", cmd->arg_str);
                break;
 			}

            CGraphEdgeIterator* it;
			it = cgraphr_edges(g, arg.rank, arg.label, arg.nodes) ;

            CGraphEdge n;
            EdgeList ls = {0}; // list is empty
            while(cgraphr_edges_next(it, &n))
                edge_append(&ls, &n);

            // sort the edges
            qsort(ls.data, ls.len, sizeof(CGraphEdge), cmp_edge);

            for(size_t i = 0; i < ls.len; i++) {
                printf("(%" PRId64, ls.data[i].label);
				for (CGraphRank j = 0; j < ls.data[i].rank; j++) {
                //for (CGraphRank j = 0; j < ls.data[i].rank; j++) {
						printf(",\t%" PRId64, ls.data[i].nodes[j]);
					
                }
                printf(")\n");

            }
			len = len + (int)ls.len;

            for (int i = 0; i < ls.len; i++)
            {
                free(ls.data[i].nodes);
            }
            if(ls.data)
                free(ls.data);

			//DO Same for node2 -- node1
            if(parse_index_between_arg(cmd->arg_str, &arg, 1, 0) < 0) {
                fprintf(stderr, "failed to parse edge argument \"%s\"\n", cmd->arg_str);
                break;
 			}
 			CGraphEdgeIterator* it2;
			it2= cgraphr_edges(g, arg.rank, arg.label, arg.nodes) ;

			
            CGraphEdge n2;
            EdgeList ls2 = {0}; // list is empty
            while(cgraphr_edges_next(it2, &n2))
                edge_append(&ls2, &n2);

            // sort the edges
            qsort(ls2.data, ls2.len, sizeof(CGraphEdge), cmp_edge);

            for(size_t i = 0; i < ls2.len; i++) {
                printf("(%" PRId64, ls2.data[i].label);
				for (CGraphRank j = 0; j < ls2.data[i].rank; j++) {
                //for (CGraphRank j = 0; j < ls.data[i].rank; j++) {
						printf(",\t%" PRId64, ls2.data[i].nodes[j]);
					
                }
                printf(")\n");

            }
			len = len + (int)ls2.len;
			printf("Gezählte Hyperedge %i\n", len);
            for (int i = 0; i < ls2.len; i++)
            {
                free(ls2.data[i].nodes);
            }
            if(ls2.data)
                free(ls2.data);


            res = 0;
			break;
		}
		case CMD_NONE:
			goto exit;
		}	
	}

exit:
	cgraphr_destroy(g);
	return res;
}


int main(int argc, char** argv) {
	if(argc <= 1) {
		print_usage(true);
		return EXIT_FAILURE;
	}

	CGraphArgs argd;
	int arg_indx = parse_args(argc, argv, &argd); // ignore first program arg
	if(arg_indx < 0)
		return EXIT_FAILURE;

	char** cmd_argv = argv + arg_indx;
	int cmd_argc = argc - arg_indx;

	if(argd.mode == -1) // unknown mode, determine by the number of arguments
		argd.mode = cmd_argc == 2 ? 0 : 1;
	if(argd.mode == 0) {
		if(cmd_argc != 2) {
			fprintf(stderr, "expected 2 parameters when compressing RDF graphs\n");
			return EXIT_FAILURE;
		}

		if(do_compress(cmd_argv[0], cmd_argv[1], &argd) < 0)
			return EXIT_FAILURE;

	}
	else {
		if(cmd_argc != 1) {
			fprintf(stderr, "expected 1 parameter when reading compressed RDF graphs\n");
			return EXIT_FAILURE;
		}

		if(do_read(cmd_argv[0], &argd) < 0)
			return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
