/**
 * @file cgraphw.c
 * @author FR
 */

#include <cgraph.h>

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include <hgraph.h>
#include <treemap.h>
#include <hashmap.h>
#include <hashset.h>
#include <bitarray.h>
#include <constants.h>
#include <slhr_grammar.h>
#include <repair.h>
#include <bitsequence.h>
#include <writer.h>
#include <slhr_grammar_writer.h>
#include <dict_writer.h>

typedef struct {
	bool compressed;
	CGraphCParams params;

	
	Treemap* dict_ve; // dictionary which contains nodes and edges
	bool dict_disjunct;

	size_t nodes;
	size_t terminals;

	union {
		// Only needed before compression:
		struct {
			Hashmap* dict_rev; // hashmap because it is faster than the treemap and the order is not important
			Hashset* edges; // The edges are stored in a set because duplicate edges are not allowed
		};
		// Only needed after compression:
		struct {
			BitArray bv;
			BitArray be;
			SLHRGrammar* grammar;
		};
	};
} GraphWriterImpl;

static int cmp_size_cb(const void* k1, size_t l1, const void* k2, size_t l2) {
	assert(l1 == sizeof(size_t) && l2 == sizeof(size_t));

	const size_t* s1 = k1;
	const size_t* s2 = k2;
	return CMP(*s1, *s2);
}

static Hash hash_size_cb(const void* k, size_t l) {
	assert(l == sizeof(size_t));

	const size_t* s = k;
	return HASH(*s);
}

static int cmp_edge_sort(const void* v1, const void* v2) {
	const HEdge** e1 = (const HEdge**) v1;
	const HEdge** e2 = (const HEdge**) v2;
	return hedge_cmp(*e1, *e2);
}

static int cmp_edge_cb(const void* k1, size_t l1, const void* k2, size_t l2) {
	const HEdge* e1 = k1;
	const HEdge* e2 = k2;

	assert(l1 == hedge_sizeof(e1->rank) && l2 == hedge_sizeof(e2->rank));

	return hedge_cmp(e1, e2);
}

static Hash hash_edge_cb(const void* k, size_t l) {
	const HEdge* e = k;

	assert(l == hedge_sizeof(e->rank));

	Hash h = HASH(e->label);
	for(size_t i = 0; i < e->rank; i++)
		HASH_COMBINE(h, HASH(e->nodes[i]));
	return h;
}

CGraphW* cgraphw_init() {
	Treemap* dict_ve = treemap_init(NULL); // using default comparator
	if(!dict_ve)
		return NULL;

	Hashmap* dict_rev = hashmap_init(cmp_size_cb, hash_size_cb);
	if(!dict_rev)
		goto err_0;

	Hashset* edges = hashset_init(cmp_edge_cb, hash_edge_cb);
	if(!edges)
		goto err_1;

	GraphWriterImpl* g = malloc(sizeof(*g));
	if(!g)
		goto err_2;

	// Only init attributes needed before compression
	g->compressed = false;

	g->params.max_rank = DEFAULT_MAX_RANK;
	g->params.monograms = DEFAULT_MONOGRAMS;
	g->params.factor = DEFAULT_FACTOR;
	g->params.sampling = DEFAULT_SAMPLING;
	g->params.rle = DEFAULT_RLE;
	g->params.nt_table = DEFAULT_NT_TABLE;
#ifdef RRR
	g->params.rrr = DEFAULT_RRR;
#endif

	g->dict_ve = dict_ve;
	g->dict_disjunct = true;
	g->nodes = 0;
	g->terminals = 0;

	g->dict_rev = dict_rev;
	g->edges = edges;

	return (CGraphW*) g;

err_2:
	hashset_destroy(edges);
err_1:
	hashmap_destroy(dict_rev);
err_0:
	treemap_destroy(dict_ve);
	return NULL;
}

void cgraphw_destroy(CGraphW* g) {
	GraphWriterImpl* gi = (GraphWriterImpl*) g;

	treemap_destroy(gi->dict_ve);

	if(!gi->compressed) {
		hashmap_destroy(gi->dict_rev);
		hashset_destroy(gi->edges);
	}
	else {
		bitarray_destroy(&gi->bv);
		if(!gi->dict_disjunct)
			bitarray_destroy(&gi->be);

		slhr_grammar_destroy(gi->grammar);
	}

	free(gi);
}

typedef enum {
	OCC_NODE,
	OCC_EDGE,
	OCC_BOTH
} ElementOccurence;

typedef struct {
	size_t value;
	ElementOccurence occ;
} GraphDictElement;

static size_t dict_put_text(GraphWriterImpl* g, const char* s, bool node) {
	GraphDictElement e, *val;
	MapItem item;

	size_t len = strlen(s) + 1;
	if(!treemap_contains_key(g->dict_ve, s, len)) {
		size_t id = treemap_size(g->dict_ve);

		e.value = id;
		e.occ = node ? OCC_NODE : OCC_EDGE;

		if(treemap_put(g->dict_ve, s, len, &e, sizeof(e)) < 0)
			return -1;

		if(!treemap_item(g->dict_ve, s, len, &item)) {
			// should never happen
			treemap_remove(g->dict_ve, s, len);
			return -1;
		}

		// reverse-dict stores the pointer to the key in the other dict
		if(hashmap_put(g->dict_rev, &id, sizeof(id), &item.key, sizeof(&item.key)) < 0)
			return -1;

		return e.value;
	}

	val = treemap_get(g->dict_ve, s, len, NULL);

	if((node && val->occ != OCC_NODE) || (!node && val->occ != OCC_EDGE)) {
		val->occ = OCC_BOTH;
		g->dict_disjunct = false;
	}

	return val->value;
}

int cgraphw_add_edge(CGraphW* g, const CGraphRank rank, const char* label, const char** nodes, size_t edge_index) {
	GraphWriterImpl* gi = (GraphWriterImpl*) g;

	// cannot add new edges if the graph is compressed
	if(gi->compressed)
		return -1;

    // The memory of the edge is allocated via `alloca` because `hashmap` copies the data internally
    // and then manages the memory itself.
    // Furthermore, a short call to `alloca` is more efficient than `malloc` + `free`.
    HEdge* edge = alloca(hedge_sizeof(rank));
    if(!edge)
        return -1;
	
    edge->rank = rank;
	
    if(label == NULL || (edge->label = dict_put_text(gi, label, false)) == -1) // comparing unsigned value
        return -1;
	

    for (size_t i = 0; i < rank-1; i++)
    {


        if (nodes[i] == NULL || (edge->nodes[i] = dict_put_text(gi, nodes[i], true)) == -1)
            return -1;
    }
	
	edge->nodes[rank-1] = edge_index;
	
	if(hashset_add(gi->edges, edge, hedge_sizeof(rank)) < 0)
		return -1;

	return 0;
}

int cgraphw_add_node(CGraphW* g, const char* n) {
	GraphWriterImpl* gi = (GraphWriterImpl*) g;

	// cannot add new edges if the graph is compressed
	if(gi->compressed)
		return -1;
	if(n == NULL)
		return -1;

	size_t v_n;

	if((v_n = dict_put_text(gi, n, true)) == -1) // comparing unsigned value
		return -1;

	return 0;
}

void cgraphw_set_params(CGraphW* g, const CGraphCParams* p) {
	if(!p)
		return;

	GraphWriterImpl* gi = (GraphWriterImpl*) g;

	if(p->max_rank > 0)
		gi->params.max_rank = p->max_rank;
	if(gi->params.max_rank > LIMIT_MAX_RANK)
		gi->params.max_rank = LIMIT_MAX_RANK;
	gi->params.monograms = p->monograms;
	if(p->factor > 0)
		gi->params.factor = p->factor;
	if(p->sampling > 0)
		gi->params.sampling = p->sampling;
	gi->params.rle = p->rle;
	gi->params.nt_table = p->nt_table;
#ifdef RRR
	gi->params.rrr = p->rrr;
#endif
}

static int cgraph_set_bitsequences(GraphWriterImpl* g, BitArray* bv, BitArray* be) {
	size_t dict_len = treemap_size(g->dict_ve);

	if(bitarray_init(bv, dict_len) < 0)
		return -1;
	if(!g->dict_disjunct) {
		if(bitarray_init(be, dict_len) < 0) {
			// destruction of `bv` needed because it is not freed in `cgraphw_compress`
			bitarray_destroy(bv);
			return -1;
		}
	}

	TreemapIterator it;
	treemap_iter(g->dict_ve, &it);

	MapItem item;
	ElementOccurence occ;

	for(size_t i = 0; treemap_iter_next(&it, &item); i++) {
		occ = ((GraphDictElement*) item.val)->occ;

		if(occ == OCC_NODE || occ == OCC_BOTH) {
			bitarray_set(bv, i, true);
			g->nodes++;
		}
		if(occ == OCC_EDGE || occ == OCC_BOTH) {
			if(!g->dict_disjunct)
				bitarray_set(be, i, true);
			g->terminals++;
		}
	}

	return 0;
}

static Hashmap* modify_dict_rev(GraphWriterImpl* g) {
	Hashmap* id_mapping = hashmap_init(cmp_size_cb, hash_size_cb);
	if(!id_mapping)
		return NULL;

	HashmapIterator it;
	hashmap_iter(g->dict_rev, &it);
	MapItem item;

	while(hashmap_iter_next(&it, &item)) {
		char** text = item.val;

		size_t len = strlen(*text) + 1;
		ssize_t index = treemap_index_of(g->dict_ve, *text, len);
		if(index < 0)
			goto err;

		if(hashmap_put(id_mapping, item.key, item.len_key, &index, sizeof(index)) < 0)
			goto err;
	}

	return id_mapping;

err:
	hashmap_destroy(id_mapping);
	return NULL;
}

static inline size_t dict_index(Hashmap* new_ids, size_t val) {
	size_t* index = hashmap_get(new_ids, &val, sizeof(val), NULL);
	if(!index)
		return -1;

	return *index;
}

static HGraph* cgraphw_modify_ids(GraphWriterImpl* g , BitArray* bv, BitArray* be, Hashmap* new_ids) {
	bool disjunct = g->dict_disjunct;
	Bitsequence bs_v, bs_e;

	if(bitsequence_build(&bs_v, bv, 0) < 0)
		return NULL;
	if(!disjunct && bitsequence_build(&bs_e, be, 0) < 0)
		goto err_0;

	HGraph* gr = hgraph_init(RANK_NONE);
	if(!gr)
		goto err_1;

	HashsetIterator it;
	hashset_iter(g->edges, &it);
	const HEdge* edge;
	size_t tmp;

	while((edge = hashset_iter_next(&it, NULL)) != NULL) {
		HEdge* e = malloc(hedge_sizeof(edge->rank));
		if(!e)
			goto err_2;

		if((tmp = dict_index(new_ids, edge->label)) == -1) // determine the index in the whole dict, comparing unsigned value
			goto err_2;

		// determine the value in the edge label dict
		e->label = (!disjunct ? bitsequence_rank1(&bs_e, tmp) : bitsequence_rank0(&bs_v, tmp)) - 1;
		e->rank = edge->rank;

		//CHANGED
		for(size_t j = 0; j < edge->rank-1; j++) {
			if((tmp = dict_index(new_ids, edge->nodes[j])) == -1) // determine the index in the whole dict, comparing unsigned value
				goto err_2;

			e->nodes[j] = bitsequence_rank1(&bs_v, tmp) - 1; // determine the value in the node label dict
		}
		
		
		e->nodes[e->rank-1] = edge->nodes[edge->rank-1];

		if(hgraph_add_edge(gr, e) < 0) {
			free(e);
			goto err_2;
		}
	}

	// sorting the edges enhances the compression
	qsort(gr->edges, gr->len, sizeof(HEdge*), cmp_edge_sort);

	bitsequence_destroy(&bs_v);
	if(!disjunct)
		bitsequence_destroy(&bs_e);

	return gr;

err_2:
	hgraph_destroy(gr);
err_1:
	if(!disjunct)
		bitsequence_destroy(&bs_e);
err_0:
	bitsequence_destroy(&bs_v);
	return NULL;
}

int cgraphw_compress(CGraphW* g, size_t edge_index) {
	GraphWriterImpl* gi = (GraphWriterImpl*) g;

	if(gi->compressed)
		return -1;
	if(hashset_size(gi->edges) == 0) // empty graph is not supported
		return -1;

	BitArray bv, be;
	if(cgraph_set_bitsequences(gi, &bv, &be) < 0)
		return -1;
	//NOTLÖSUNG, weil macht anzahl an knoten kaputt
	//set gi-> nodes to real number of elements in dict or number of edges
	//printf("%i", edge_index);
	//	gi->nodes = gi->nodes + edge_index;
	// create a new temporary mapping to increase the speed of `cgraphw_modify_ids`
	if(edge_index > gi->nodes)
		gi->nodes = edge_index;
	
	Hashmap* id_mapping = modify_dict_rev(gi);
	if(!id_mapping)
		goto err_0;

	HGraph* start_symbol = cgraphw_modify_ids(gi, &bv, &be, id_mapping);
	hashmap_destroy(id_mapping); // destroy instantly
	if(!start_symbol)
		goto err_0;
	
	SLHRGrammar* gr = repair(start_symbol, gi->nodes, gi->terminals, gi->params.max_rank, gi->params.monograms);
	if(!gr)
		goto err_0;
	// destroying `start_symbol` not needed because the memory is managed by `repair` or the grammar `gr`

	// destroy the data if the repair-compression fully succeeded
	hashmap_destroy(gi->dict_rev);
	hashset_destroy(gi->edges);

	gi->compressed = true;
	gi->bv = bv;
	gi->be = be;
	gi->grammar = gr;

	return 0;

err_0:
	bitarray_destroy(&bv);
	if(!gi->dict_disjunct)
		bitarray_destroy(&be);
	return -1;
}

int cgraphw_write(CGraphW* g, const char* path, bool verbose) {
	GraphWriterImpl* gi = (GraphWriterImpl*) g;

	if(!gi->compressed)
		return -1;

	BitWriter w;
	if(bitwriter_init(&w, path) < 0)
		return -1;

	BitWriter w0;
	bitwriter_init(&w0, NULL);

	BitsequenceParams p;
	p.factor = gi->params.factor;
#ifdef RRR
	p.rrr = gi->params.rrr;
#endif

	if(slhr_grammar_write(gi->grammar, gi->nodes, gi->terminals, gi->params.nt_table, &w0, &p) < 0)
		goto err_0;
    if (verbose)
        printf("  Writing magic\n");
	if(bitwriter_write_bytes(&w, MAGIC_GRAPH, strlen(MAGIC_GRAPH) + 1) < 0)
		goto err_1;
    if (verbose)
        printf("  Writing meta\n");
	if(bitwriter_write_vbyte(&w, bitwriter_bytelen(&w0)) < 0)
		goto err_1;
    if (verbose)
        printf("  Writing grammar\n");
	if(bitwriter_write_bitwriter(&w, &w0) < 0)
		goto err_1;
	if(bitwriter_close(&w0) < 0)
		goto err_0;
    if (verbose) {
        printf("  Grammar Size is %lu byte\n", bitwriter_bytelen(&w0));
        printf("  Writing dictionary\n");
    }
	if(dict_write(gi->dict_ve, &gi->bv, &gi->be, gi->dict_disjunct, gi->params.sampling, gi->params.rle, &w, &p) < 0)
		goto err_0;
	if(bitwriter_close(&w) < 0)
		return -1;
    if (verbose)
        printf("  Writing finished\n");
		/*
	FILE *fpt;
	fpt = fopen("/home/dtkhuat/results/sizes/17_04_24.csv", "a");
	fprintf(fpt, "%ld\n", bitwriter_bytelen(&w0));
	fclose(fpt);
	*/
	return 0;

err_1:
	bitwriter_close(&w0);
err_0:
	bitwriter_close(&w);
	return -1;
}
