/**
 * @file wavelettree.c
 * @author FR
 */

#include "wavelettree.h"

#include <stdlib.h>
#include <string.h>
#include <reader.h>
#include <bitarray.h>
#include <panic.h>
#include <bitsequence_r.h>

// calculate `bitoff` and rank1(bitoff - 1) for each inner node
static void tree_data(WaveletTreeReader* w, int i, uint64_t len, FileOff* bitoff) {
	WaveletRNode* n = w->tree + i;

	if(!n->leaf) {
		n->bitoff = *bitoff;
		*bitoff += len;

		n->bitoff_rank1 = n->bitoff > 0 ? bitsequence_reader_rank1(w->bits, n->bitoff - 1) : 0;

		size_t len_right = bitsequence_reader_rank1(w->bits, n->bitoff + len - 1) - n->bitoff_rank1;

		tree_data(w, n->left, len - len_right, bitoff);
		tree_data(w, n->right, len_right, bitoff);
	}
}

static void read_tree(Reader* r, WaveletRNode* tree, int* i) {
	WaveletRNode* n = tree + (*i)++;

	if(*i >= MAX_NODES)
		panic("the number of nodes (%d) reaches or exceeds the maximum of %d", *i, MAX_NODES);

	bool leaf = reader_readbit(r);
	n->leaf = leaf;

	if(leaf)
		n->value = reader_readbyte(r);
	else {
		n->left = *i;
		read_tree(r, tree, i);

		n->right = *i;
		read_tree(r, tree, i);
	}
}

// shortcut for cloning and append a bitarray to keep the code in "build_coding" more clean
static inline int bitarray_clone_append(BitArray* dst, const BitArray* src, bool v) {
	if(bitarray_clone(dst, src) < 0)
		return -1;

	return bitarray_append(dst, v);
}

static int build_coding(WaveletRNode* tree, BitArray coding[256], int node_i, BitArray* path) {
	WaveletRNode* node = tree + node_i;

	if(node->leaf)
		// Copying path is possible because after this call,
		// the data of path is not freed
		coding[node->value] = *path;
	else {
		BitArray pathl, pathr;

		int ret = bitarray_clone_append(&pathl, path, false);
		if(ret < 0) {
			bitarray_destroy(path);
			return -1;
		}

		ret = bitarray_clone_append(&pathr, path, true);
		bitarray_destroy(path);

		if(ret < 0) {
			bitarray_destroy(&pathl);
			return -1;
		}

		if(build_coding(tree, coding, node->left, &pathl) < 0) {
			bitarray_destroy(&pathr);
			return -1;
		}
		if(build_coding(tree, coding, node->right, &pathr) < 0)
			return -1;
	}

	return 0;
}

WaveletTreeReader* wavelet_init(Reader* r) {
	size_t nbytes;
	FileOff len = reader_vbyte(r, &nbytes);
	FileOff off = nbytes;

	FileOff lentree = reader_vbyte(r, &nbytes);
	off += nbytes;

	FileOff offbits = off + lentree;

	Reader rt;
	reader_init(r, &rt, offbits);
	BitsequenceReader* b = bitsequence_reader_init(&rt);
	if(!b)
		return NULL;

	WaveletTreeReader* w = malloc(sizeof(*w));
	if(!w)
		goto err0;

	w->bits = b;
	memset(w->tree, 0, sizeof(w->tree));

	int i = 0;
	reader_bytepos(r, off);
	read_tree(r, w->tree, &i);

	// calculate bitoff and rank1 of each inner node after the full tree is read because the tree is read sequentially but
	// by calculating the bitoff and rank1 of the inner nodes, the bit position is moved away
	FileOff bitoff = 0;
	tree_data(w, 0, len, &bitoff);

	memset(w->coding, 0, sizeof(w->coding)); // setting all values to 0

	BitArray bs; // starting at the root with an empty bitarray
	bitarray_init(&bs, 0);

	if(build_coding(w->tree, w->coding, 0, &bs) < 0) // starting with node 0 and the empty bitarray
		goto err1;

	return w;

err1:
	for(int i = 0; i < (sizeof(w->coding) / sizeof(w->coding[0])); i++)
		bitarray_destroy(&w->coding[i]);
	free(w);
err0:
	bitsequence_reader_destroy(b);
	return NULL;
}

void wavelet_destroy(WaveletTreeReader* w) {
	bitsequence_reader_destroy(w->bits);
	for(int i = 0; i < (sizeof(w->coding) / sizeof(w->coding[0])); i++)
		bitarray_destroy(&w->coding[i]);
	free(w);
}

static inline bool w_bits_access(WaveletTreeReader* w, WaveletRNode* n, uint64_t i) {
	return bitsequence_reader_access(w->bits, n->bitoff + i);
}

static inline uint64_t w_bits_rank0(WaveletTreeReader* w, WaveletRNode* n, uint64_t i) {
	return bitsequence_reader_rank0(w->bits, n->bitoff + i) - (n->bitoff - n->bitoff_rank1);
}

static inline uint64_t w_bits_rank1(WaveletTreeReader* w, WaveletRNode* n, uint64_t i) {
	return bitsequence_reader_rank1(w->bits, n->bitoff + i) - n->bitoff_rank1;
}

uint8_t wavelet_access(WaveletTreeReader* w, uint64_t i, uint64_t* rank) {
	WaveletRNode* n = w->tree; // first node, equivalent with &w->tree[0]

	while(!n->leaf) {
		if(!w_bits_access(w, n, i)) {
			i = w_bits_rank0(w, n, i) - 1;
			n = w->tree + n->left; // equivalent with &w->tree[n->left]
		} else {
			i = w_bits_rank1(w, n, i) - 1;
			n = w->tree + n->right; // equivalent with &w->tree[n->right]
		}
	}

	if(rank)
		*rank = i + 1;
	return n->value;
}

uint64_t wavelet_rank(WaveletTreeReader* w, uint8_t c, uint64_t i) {
	BitArray* code = &w->coding[c];
	if(!code->len)
		return 0;

	WaveletRNode* n = w->tree; // first node, equivalent with &w->tree[0]
	int level = 0;

	while(!n->leaf) {
		if(!bitarray_get(code, level++)) { // postincrementing level
			i = w_bits_rank0(w, n, i) - 1;
			n = w->tree + n->left; // equivalent with &w->tree[n->left]
		} else {
			i = w_bits_rank1(w, n, i) - 1;
			n = w->tree + n->right; // equivalent with &w->tree[n->right]
		}
	}
	// do not destroy code

	if(n->value != c)
		return 0;
	return i + 1;
}
