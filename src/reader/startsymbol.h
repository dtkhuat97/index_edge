/**
 * @file startsymbol.h
 * @author FR
 */

#ifndef STARTSYMBOL_H
#define STARTSYMBOL_H

#include <stdbool.h>
#include <reader.h>
#include <edge.h>
#include <eliasfano.h>
#include <k2.h>
#include <cgraph.h>

typedef struct {
	Reader r;

	K2Reader* matrix;
	EliasFanoReader* labels;

	struct {
		int n; // bits per value
		FileOff off; // offset in the reader r
	} edge_ifs;

	struct {
		EliasFanoReader* table; // offset table
		FileOff off; // offset in the reader r of the concatted data
	} ifs;

	// The following two fields are only set, if the NT table exists
	K2Reader* nt_table; // note: memory is managed by the grammar reader
	uint64_t terminals;
} StartSymbolReader;

StartSymbolReader* startsymbol_init(Reader* r);
void startsymbol_destroy(StartSymbolReader* s);

typedef struct {
	StartSymbolReader* s;
	// Storing node and expected label
	CGraphRank rank;
    CGraphEdgeLabel label;
    CGraphNode nodes[128];

    bool predicate_query;
	union {
        K2Iterator it;
        EliasFanoIterator efit;
    };
} StartSymbolNeighborhood;

// `node_dst` and `label` are optional.
void startsymbol_neighborhood(StartSymbolReader* s, bool predicate_query, CGraphRank rank, CGraphEdgeLabel label, const CGraphNode* nodes, StartSymbolNeighborhood* n);

// return value:
// 1: next element exists
// 0: no next element exists
// -1: error occured
int startsymbol_neighborhood_next(StartSymbolNeighborhood* n, StEdge* edge);
void startsymbol_neighborhood_finish(StartSymbolNeighborhood* n); // needed if not iterated to the end

#endif
