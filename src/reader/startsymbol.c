/**
 * @file startsymbol.c
 * @author FR
 */

#include "startsymbol.h"

#include <stdlib.h>
#include <string.h>
#include <reader.h>
#include <cgraph.h>
#include <panic.h>
#include <eliasfano.h>
#include <k2.h>

StartSymbolReader* startsymbol_init(Reader* r) {
	size_t nbytes;
	FileOff lenmatrix = reader_vbyte(r, &nbytes);
	FileOff off = nbytes;

	FileOff lenlabels = reader_vbyte(r, &nbytes);
	off += nbytes;

	FileOff lenifsedge = reader_vbyte(r, &nbytes);
	off += nbytes;

	FileOff offlabels = off + lenmatrix;
	FileOff offifsedge = offlabels + lenlabels;
	FileOff offifs = offifsedge + lenifsedge;

	Reader rt;
	reader_init(r, &rt, off);
	K2Reader* matrix = k2_init(&rt);
	if(!matrix)
		return NULL;

	reader_init(r, &rt, offlabels); // reusing the reader
	EliasFanoReader* labels = eliasfano_init(&rt);
	if(!labels)
		goto err0;

	reader_bytepos(r, offifsedge);

	int edge_ifs_n = reader_vbyte(r, &nbytes);
	FileOff edge_ifs_off = offifsedge + nbytes;

	reader_bytepos(r, offifs);

	FileOff tmp = reader_vbyte(r, &nbytes);
	FileOff offtable = offifs + nbytes;
	FileOff offdata = offtable + tmp;

	reader_init(r, &rt, offtable); // reusing the reader
	EliasFanoReader* table = eliasfano_init(&rt);
	if(!table)
		goto err1;

	StartSymbolReader* s = malloc(sizeof(*s));
	if(!s)
		goto err2;

	s->r = *r;
	s->matrix = matrix;
	s->labels = labels;
	s->edge_ifs.n = edge_ifs_n;
	s->edge_ifs.off = 8 * edge_ifs_off;
	s->ifs.table = table;
	s->ifs.off = 8 * offdata;
	s->nt_table = NULL;
	s->terminals = 0;

	return s;

err2:
	eliasfano_destroy(table);
err1:
	eliasfano_destroy(labels);
err0:
	k2_destroy(matrix);
	return NULL;
}

void startsymbol_destroy(StartSymbolReader* s) {
	k2_destroy(s->matrix);
	eliasfano_destroy(s->labels);
	eliasfano_destroy(s->ifs.table);
	free(s);
}

void startsymbol_neighborhood(StartSymbolReader* s, bool predicate_query, CGraphRank rank, CGraphEdgeLabel label, const CGraphNode* nodes, StartSymbolNeighborhood* n) {
	n->s = s;
//	if(node_src == node_dst)
//		// If `node_src == node_dst` we do not have to check for edges, adjacent to the destination node,
//		// because all resulting edges are automatically adjacent to the destination node.
//		n->node_dst = -1;
//	else
//		n->node_dst = node_dst;
    int next = 0;
	//BSP: -1,-1,1939
	// n->nodes[0]=1939 
    if (nodes != NULL) {
        for (int i = 0; i < rank; i++) {
            if (nodes[i] != CGRAPH_NODES_ALL) {
                n->nodes[next++] = nodes[i];
            }
        }
		//nicht hier rein
        for (int i = 0; i < next; i++) {
            if (n->nodes[i] != CGRAPH_NODES_ALL) {
                for (int j = i + 1; j < next; j++) {
                    if (n->nodes[i] == n->nodes[j]) {
                        n->nodes[j] = CGRAPH_NODES_ALL;
                    }
                }
            }
        }
		//FÜR BSP n->rank = 1
        n->rank = next;
		//n->rank = rank;
    }
    else
    {
        n->rank = CGRAPH_NODES_ALL;
    }
    n->predicate_query = predicate_query;
    n->label = label;
    if (predicate_query)
    {
        eliasfano_iter(s->labels, label, s->terminals + 1, &n->efit);
    }
	else
    {
        k2_iter_init_row(s->matrix, n->nodes[0], &n->it);
    }
}

// return the id if the index function of a edge
static inline int edge_ifs_get(StartSymbolReader* s, uint64_t edge) {
	FileOff line_off = s->edge_ifs.off + s->edge_ifs.n * edge;
	reader_bitpos(&s->r, line_off);

	return reader_readint(&s->r, s->edge_ifs.n);
}

// determine the index function of a edge
// the memory where the index function is written to is given as a parameter
// the number of elements is returned as the return type
static inline int if_get(StartSymbolReader* s, int i, int* indf) {
	FileOff off = eliasfano_get(s->ifs.table, i);
	reader_bitpos(&s->r, s->ifs.off + off);

	int n = reader_eliasdelta(&s->r);
	if(n > RANK_MAX)
		panic("index function %d with a rank of %d exceeds the maximum rank of %d", i, n, RANK_MAX);

	for(int k = 0; k < n; k++)
		indf[k] = reader_eliasdelta(&s->r);

	return n;
}

// return value:
// 1: edge should be considered
// 0: edge can be ignored
// -1: error occured
static inline int get_edge(StartSymbolNeighborhood* n, uint64_t e, StEdge* edge) {
	CGraphEdgeLabel expected_label;
	StartSymbolReader* s = n->s;

	// determining the label of the edge
	uint64_t label = eliasfano_get(s->labels, e);

	if((expected_label = n->label) != CGRAPH_LABELS_ALL) {
		uint64_t terminals = s->terminals;

		// determine by the edge label, if the edge should be extracted
		if(label < terminals) { // edge is a terminal edge
			if(label != expected_label)
				return 0; // return 0, because the edge label does not match with the expected label
		}
		else {
			K2Reader* nt_table;
			if((nt_table = s->nt_table) && !k2_get(nt_table, label - terminals, expected_label))
				return 0; // return 0, because the nt edge does not produces a edge with the expected label
		}
	}

	// Check if the current edge is adjacent to all destination nodes.
    for (int i = 0; i < n->rank; i++) {
        // edge is not adjacent to the destination node
        if (n->nodes[i] != CGRAPH_NODES_ALL && !k2_get(s->matrix, n->nodes[i], e))
            return 0;
    }

	size_t c_len; // Number of nodes of the edge
	uint64_t* nodes = k2_column(s->matrix, e, &c_len); // Should we check for NULL?
	if(!nodes)
		return -1;

	int ix = edge_ifs_get(s, e); // Index of the index function

	int indx[RANK_MAX]; // The index function
	int i_len = if_get(s, ix, indx); // length of the index function

	uint64_t nodes_order[RANK_MAX]; // We prealloc on the stack because our rank is limited to RANK_MAX
	for(int j = 0; j < i_len; j++)
		nodes_order[j] = nodes[indx[j]];
	free(nodes);

	edge->label = label;
	edge->rank = i_len;
	memcpy(edge->nodes, nodes_order, i_len * sizeof(uint64_t)); // copy the nodes of "nodes_order" to the edge via memcpy
	return 1;
}

int startsymbol_neighborhood_next(StartSymbolNeighborhood* n, StEdge* edge) {
	uint64_t neigh;
	for(;;) {
		switch(n->predicate_query ? eliasfano_iter_next(&n->efit, &neigh) : k2_iter_next(&n->it, &neigh)) {
		case 0:
			return 0;
		case 1: {
			switch(get_edge(n, neigh, edge)) {
			case 0:
				continue;
			case 1:
				return 1;
			default:
				return -1;
			}
		}
		default:
			return -1;
		}
	}
}

void startsymbol_neighborhood_finish(StartSymbolNeighborhood* n) {
    if (n->predicate_query)
    {
        eliasfano_iter_finish(&n->efit);
    }
    else
    {
        k2_iter_finish(&n->it);
    }
}
