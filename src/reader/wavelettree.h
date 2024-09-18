/**
 * @file wavelettree.h
 * @author FR
 */

#ifndef WAVELET_TREE_H
#define WAVELET_TREE_H

#include <stdbool.h>
#include <bitarray.h>
#include <reader.h>
#include <bitsequence_r.h>

#define MAX_LEAFS 256
#define MAX_NODES (2 * MAX_LEAFS)

typedef struct {
	bool leaf;
	union { // a node as either ...
		uint8_t value; // ... a leaf value (1 byte)
		struct { // ... or a left and right child and an offset to the bit sequence
			int left;
			int right;
			FileOff bitoff;
			FileOff bitoff_rank1; // cache for rank1(bitoff - 1)
		};
	};
} WaveletRNode; // wavelet reader node

typedef struct {
	BitsequenceReader* bits;
	WaveletRNode tree[MAX_NODES]; // 512 nodes possible
	BitArray coding[MAX_LEAFS]; // only 256 values possible
} WaveletTreeReader;

WaveletTreeReader* wavelet_init(Reader* r);
void wavelet_destroy(WaveletTreeReader* w);

uint8_t wavelet_access(WaveletTreeReader* w, uint64_t i, uint64_t* rank);
uint64_t wavelet_rank(WaveletTreeReader* w, uint8_t c, uint64_t i);

#endif
