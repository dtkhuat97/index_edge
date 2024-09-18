/**
 * @file cgraph.c
 * @author FR
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cgraph.h>
#include <constants.h>
#include <reader.h>
#include <grammar.h>
#include <dict.h>
#include <bitsequence_r.h>
#include "panic.h"

// Internal struct for the handler of libcgraph.
// This contains the file readers and the readers for the grammar and the dictionary.
typedef struct {
	FileReader* r;
	GrammarReader* gr;
	DictionaryReader* dr;
} GraphReaderImpl;

CGraphR* cgraphr_init(const char* path) {
	// check if graph file is readable
	if(access(path, F_OK | R_OK) != 0) {
		perror(path);
		return NULL;
	}

	// open the bit reader for the graph file
	FileReader* fr = filereader_init(path);
	if(!fr)
		return NULL;

	// initialize the grammar reader with an subreader
	Reader r;
	reader_initf(fr, &r, 0);

	const uint8_t* magic = reader_read(&r, MAGIC_GRAPH_LEN);
	if(memcmp(magic, MAGIC_GRAPH, MAGIC_GRAPH_LEN) != 0)
		goto err0;

	size_t nbytes;
	FileOff lengrammar = reader_vbyte(&r, &nbytes);

	FileOff offgrammar = MAGIC_GRAPH_LEN + nbytes;
	FileOff offdict = offgrammar + lengrammar;

	reader_initf(fr, &r, offgrammar);
	GrammarReader* gr = grammar_init(&r);
	if(!gr)
		goto err0;

	// initialize the dict reader with an subreader
	reader_initf(fr, &r, offdict);
	DictionaryReader* dr = dictionary_init(&r);
	if(!dr)
		goto err1;

	GraphReaderImpl* g = malloc(sizeof(*g));
	if(!g)
		goto err2;

	g->r = fr;
	g->gr = gr;
	g->dr = dr;

	return (CGraphR*) g;

err2:
	dictionary_destroy(dr);
err1:
	grammar_destroy(gr);
err0:
	filereader_close(fr);
	return NULL;
}

void cgraphr_destroy(CGraphR* g) {
	GraphReaderImpl* gi = (GraphReaderImpl*) g;

	filereader_close(gi->r);
	grammar_destroy(gi->gr);
	dictionary_destroy(gi->dr);
	free(gi);
}

size_t cgraphr_node_count(CGraphR* g) {
	return ((GraphReaderImpl*) g)->gr->node_count;
}

size_t cgraphr_edge_count(CGraphR* g) {
	return ((GraphReaderImpl*) g)->gr->nt_table->width;
}

size_t cgraphr_edge_label_count(CGraphR* g) {
	return ((GraphReaderImpl*) g)->gr->rules->first_nt;
}

char* cgraphr_extract_node(CGraphR* g, CGraphNode n, size_t* l) {
	GraphReaderImpl* gi = (GraphReaderImpl*) g;

	// check if n is in the corrent interval
	uint64_t ones = bitsequence_reader_ones(gi->dr->bitsnode);
	if(n < 0 || n >= ones) {
		if(l)
			*l = 0;
		return NULL;
	}

	// determine the id in the whole dictionary
	n = bitsequence_reader_select1(gi->dr->bitsnode, n + 1);

	size_t len;
	char* res = dictionary_extract(gi->dr, n, &len); // extract operation
	if(!res) {
		if(l)
			*l = 0;
		return NULL;
	}

	if(l)
		*l = len;
	return res;
}

char* cgraphr_extract_edge_label(CGraphR* g, CGraphEdgeLabel e, size_t* l) {
	GraphReaderImpl* gi = (GraphReaderImpl*) g;

	// check if e is in the corrent interval
	uint64_t ones;
	if(gi->dr->bitsedge)
		ones = bitsequence_reader_ones(gi->dr->bitsedge);
	else
		ones = bitsequence_reader_len(gi->dr->bitsnode) - bitsequence_reader_ones(gi->dr->bitsnode);

	if(e < 0 || e >= ones) {
		if(l)
			*l = 0;
		return NULL;
	}

	// determine the id in the whole dictionary
	if(gi->dr->bitsedge)
		e = bitsequence_reader_select1(gi->dr->bitsedge, e + 1);
	else
		e = bitsequence_reader_select0(gi->dr->bitsnode, e + 1);

	size_t len;
	char* res = dictionary_extract(gi->dr, e, &len); // extract operation
	if(!res) {
		if(l)
			*l = 0;
		return NULL;
	}

	if(l)
		*l = len;
	return res;
}

CGraphNode cgraphr_locate_node(CGraphR* g, const char* p) {
	if(!p)
		return -1;

	GraphReaderImpl* gi = (GraphReaderImpl*) g;

	// Performing locate for the nodes from the whole dict like described in the bachelor thesis
	int64_t i = dictionary_locate(gi->dr, p);
	if(i < 0)
		return -1;
	if(!bitsequence_reader_access(gi->dr->bitsnode, i))
		return -1;

	return bitsequence_reader_rank1(gi->dr->bitsnode, i) - 1;
}

CGraphEdgeLabel cgraphr_locate_edge_label(CGraphR* g, const char* p) {
	if(!p)
		return -1;

	GraphReaderImpl* gi = (GraphReaderImpl*) g;

	// Performing locate for the nodes from the whole dict like described in the bachelor thesis
	int64_t i = dictionary_locate(gi->dr, p);
	if(i < 0)
		return -1;

	// check if the extracted id exists as a edge label
	if(gi->dr->bitsedge) { // bits for the dictionary of the edges exists
		if(!bitsequence_reader_access(gi->dr->bitsedge, i))
			return -1;

		return bitsequence_reader_rank1(gi->dr->bitsedge, i) - 1;
	}
	else { // no bits for the dictionary of the edges exists
		if(bitsequence_reader_access(gi->dr->bitsnode, i))
			return -1;

		return bitsequence_reader_rank0(gi->dr->bitsnode, i) - 1;
	}
}

typedef struct {
	bool prefix; // prefix search
	BitsequenceReader* bitsnode; // copy, do not free

	union {
		struct { // for prefix search
			uint64_t next;
			uint64_t limit;
		};
		struct {
			DictIterator it; // for substr search
			Intset set; // to filter duplicate nodes (see comment at `dictionary_locate_substr`)
		};
	};
} NodeIterator;

CGraphNodeIterator* cgraphr_locate_node_prefix(CGraphR* g, const char* p) {
	GraphReaderImpl* gi = (GraphReaderImpl*) g;

	NodeIterator* it = malloc(sizeof(*it));
	if(!it)
		return NULL;

	it->prefix = true;
	it->bitsnode = gi->dr->bitsnode;

	uint64_t s, e;
	if(dictionary_locate_prefix(gi->dr, p, &s, &e)) { // locate prefix
		it->next = s;
		it->limit = e;
	}
	else { // not found; creating an empty iterator
		it->next = 0;
		it->limit = -1;
	}

	return (CGraphNodeIterator*) it;
}

bool cgraphr_node_next(CGraphNodeIterator* it, CGraphNode* n) {
	NodeIterator* i = (NodeIterator*) it;

	uint64_t v;
	for(;;) {
		if(i->prefix) {
			if(i->next <= i->limit)
				v = i->next++;
			else {
				cgraphr_node_finish(it);
				return false;
			}

			if(bitsequence_reader_access(i->bitsnode, v)) { // match is node?
				*n = bitsequence_reader_rank1(i->bitsnode, v) - 1;
				return true;
			}
		}
		else {
			if(dictionary_substr_next(&i->it, &v) != 1) {
				cgraphr_node_finish(it);
				return false;
			}

			if(bitsequence_reader_access(i->bitsnode, v)) { // match is node?
				uint64_t match = bitsequence_reader_rank1(i->bitsnode, v) - 1;

				if(!intset_contains(&i->set, match)) {
					if(intset_add(&i->set, match) < 0)
						return -1;

					*n = match;
					return true;
				}
			}
		}
	}
}

void cgraphr_node_finish(CGraphNodeIterator* it) {
	NodeIterator* i = (NodeIterator*) it;

	if(!i->prefix) {
		intset_destroy(&i->set);
		dictionary_substr_finish(&i->it);
	}

	free(i);
}

CGraphNodeIterator* cgraphr_search_node(CGraphR* g, const char* p) {
	GraphReaderImpl* gi = (GraphReaderImpl*) g;

	NodeIterator* it = malloc(sizeof(*it));
	if(!it)
		return NULL;

	it->prefix = false;
	it->bitsnode = gi->dr->bitsnode;
	dictionary_locate_substr(gi->dr, p, &it->it);
	intset_init(&it->set);

	return (CGraphNodeIterator*) it;
}

bool cgraphr_edges_next(CGraphEdgeIterator* it, CGraphEdge* e) {
	CGraphEdge t;
    CGraphNode nodes[128];
    t.nodes = nodes;
	switch(grammar_neighborhood_next((GrammarNeighborhood*) it, &t)) {
	case 1:
		if(e) {
			e->label = t.label;
            e->rank = t.rank;
            e->nodes = malloc(t.rank * sizeof (CGraphNode));
            if (!e->nodes)  //TODO: Introduce error case for this.
            {
                cgraphr_edges_finish(it);
                return false;
            }
            memcpy(e->nodes, t.nodes, t.rank * sizeof (CGraphNode));
		}

		return true;
	default:
		cgraphr_edges_finish(it);
		return false;
	}
}

void cgraphr_edges_finish(CGraphEdgeIterator* it) {
	grammar_neighborhood_finish((GrammarNeighborhood*) it);
	free(it);
}

bool cgraphr_edge_exists(CGraphR* g, CGraphRank rank, CGraphEdgeLabel label, const CGraphNode* nodes) {
	GraphReaderImpl* gi = (GraphReaderImpl*) g;

    for (int i = 0; i < rank; i++)
    {
        if (nodes[i] != CGRAPH_NODES_ALL && (nodes[i] < 0 || nodes[i] >= gi->gr->node_count))
            return false; // node[i] does not exists
    }
	if(label < 0 || label >= gi->gr->rules->first_nt) // label does not exist
		return false;

	GrammarNeighborhood nb;
	grammar_neighborhood(gi->gr, false, rank, label, nodes, &nb);

	if(grammar_neighborhood_next(&nb, NULL)) {
		grammar_neighborhood_finish(&nb);
		return true;
	}

	// not `grammar_neighborhood_finish` needed because the iterator is freed
	// because `grammar_neighborhood_finish` returned false.
	return false;
}

CGraphEdgeIterator* cgraphr_edges(CGraphR* g, CGraphRank rank, CGraphEdgeLabel label, const CGraphNode* nodes) {
    GraphReaderImpl* gi = (GraphReaderImpl*) g;

    for (int i=0; i < rank; i++)
    {
        if(nodes[i] != CGRAPH_NODES_ALL && (nodes[i] < 0 || nodes[i] >= gi->gr->node_count)) // node does not exist nor is wildcard.

			return NULL;
    }

	
    GrammarNeighborhood* nb = malloc(sizeof(*nb));
    if(!nb)	
        return NULL;

    grammar_neighborhood(gi->gr, false, rank, label, nodes, nb);
	
    return (CGraphEdgeIterator*) nb;
}

CGraphEdgeIterator* cgraphr_edges_by_predicate(CGraphR* g, CGraphEdgeLabel label) {
    GraphReaderImpl* gi = (GraphReaderImpl*) g;

    if (label < 0 || label >= gi->gr->rules->first_nt) // check for terminal symbol.
        return NULL;


    GrammarNeighborhood* nb = malloc(sizeof(*nb));
    if(!nb)
        return NULL;

    grammar_neighborhood(gi->gr, true, CGRAPH_LABELS_ALL, label, NULL, nb);

    return (CGraphEdgeIterator*) nb;
}


CGraphEdgeIterator* cgraphr_edges_connecting(CGraphR* g, CGraphRank rank, const CGraphNode* nodes) {
	GraphReaderImpl* gi = (GraphReaderImpl*) g;

    for (int i = 0; i < rank; i++)
    {
        if (nodes[i] != CGRAPH_NODES_ALL && (nodes[i] < 0 || nodes[i] >= gi->gr->node_count))
            return false; // node[i] does not exists
    }

	GrammarNeighborhood* nb = malloc(sizeof(*nb));
	if(!nb)
		return NULL;

	grammar_neighborhood(gi->gr, false, rank, CGRAPH_LABELS_ALL, nodes, nb);

	return (CGraphEdgeIterator*) nb;
}

bool cgraphr_nodes_connected(CGraphR* g, CGraphRank rank, const CGraphNode* nodes) {
	CGraphEdgeIterator* it = cgraphr_edges_connecting(g, rank, nodes);
	if(!it)
		return false;

	if(cgraphr_edges_next(it, NULL)) {
		cgraphr_edges_finish(it);
		return true;
	}

	// not `cgraphr_edges_finish` needed because the iterator is freed because `graphr_edges_next` returned false.
	return false;
}
