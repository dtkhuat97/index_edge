/**
 * @file wavelet_tree_writer.c
 * @author FR
 */

#include "wavelet_tree_writer.h"

#include <string.h>
#include <assert.h>
#include <huffman.h>
#include <bitarray.h>

typedef struct _WaveletNode {
	int value;
	BitArray bits;
	struct _WaveletNode* left;
	struct _WaveletNode* right;
} WaveletNode;

#define wavelet_node_is_leaf(node) ((node)->left == NULL && (node)->right == NULL)

void wavelet_node_destroy(WaveletNode* node) {
	if(!wavelet_node_is_leaf(node)) { // inner node
		bitarray_destroy(&node->bits);
		if(node->left)
			wavelet_node_destroy(node->left);
		if(node->right)
			wavelet_node_destroy(node->right);
	}

	free(node);
}

static WaveletNode* wavelet_node_new_leaf(uint8_t v) {
	WaveletNode* node = malloc(sizeof(*node));
	if(!node)
		return NULL;

	node->value = v;
	node->left = NULL;
	node->right = NULL;

	return node;
}

static WaveletNode* wavelet_tree_build(const uint8_t* data, size_t n, int d, BitArray coding[BYTE_COUNT]) {
	BitArray bitmap;
	if(bitarray_init(&bitmap, n) < 0) // create the bitsequence of current node
		return NULL;

	uint8_t v;
	size_t i;
	for(i = 0; i < n; i++) {
		v = data[i];
		BitArray* c = &coding[v];

		if(bitarray_get(c, d))
			bitarray_set(&bitmap, i, true);
	}

	size_t len_left = bitarray_count(&bitmap, 0, n, false); // number of 1 bits in bitsequence of current node
	size_t len_right = n - len_left; // number of 0 bits in the bitsequence of the current node

	// Prealloc for better performance
	uint8_t* left = malloc(len_left * sizeof(*left));
	if(!left)
		goto err_0;

	uint8_t* right = malloc(len_right * sizeof(*right));
	if(!right)
		goto err_1;

	len_left = 0;
	len_right = 0;

	bool leaf_left = true;
	bool leaf_right = true;

	for(i = 0; i < n; i++) {
		v = data[i];
		if(!bitarray_get(&bitmap, i)) {
			left[len_left++] = v;

			if(len_left > 1 && left[len_left - 1] != left[len_left - 2])
				leaf_left = false;
		}
		else {
			right[len_right++] = v;

			if(len_right > 1 && right[len_right - 1] != right[len_right - 2])
				leaf_right = false;
		}
	}

	WaveletNode* left_child = NULL;
	WaveletNode* right_child = NULL;

	if(len_left > 0) {
		if(leaf_left)
			left_child = wavelet_node_new_leaf(left[0]);
		else
			left_child = wavelet_tree_build(left, len_left, d + 1, coding);
		if(!left_child)
			goto err_2;
	}

	if(len_right > 0) {
		if(leaf_right)
			right_child = wavelet_node_new_leaf(right[0]);
		else
			right_child = wavelet_tree_build(right, len_right, d + 1, coding);
		if(!right_child)
			goto err_3;
	}

	free(left);
	free(right);

	WaveletNode* tree = malloc(sizeof(*tree));
	if(!tree)
		goto err_4;

	tree->value = -1;
	tree->bits = bitmap;
	tree->left = left_child;
	tree->right = right_child;

	return tree;

err_4:
	wavelet_node_destroy(right_child);
err_3:
	wavelet_node_destroy(left_child);
err_2:
	free(right);
err_1:
	free(left);
err_0:
	bitarray_destroy(&bitmap);
	return NULL;
}

static int wavelet_tree_encode_nodes(WaveletNode* node, BitWriter* w, BitArray* bits) {
	if(wavelet_node_is_leaf(node)) {
		if(bitwriter_write_bit(w, 1) < 0)
			return -1;
		if(bitwriter_write_byte(w, node->value) < 0)
			return -1;
	}
	else {
		if(bitwriter_write_bit(w, 0) < 0)
			return -1;
		if(bitarray_append_bitarray(bits, &node->bits) < 0)
			return -1;

		if(wavelet_tree_encode_nodes(node->left, w, bits) < 0)
			return -1;
		if(wavelet_tree_encode_nodes(node->right, w, bits) < 0)
			return -1;
	}

	return 0;
}

int wavelet_tree_write(const uint8_t* data, size_t len, BitWriter* w, const BitsequenceParams* p) {
	int res = -1;

	BitArray coding[BYTE_COUNT];
	memset(coding, 0, sizeof(coding));

	if(huffman_create_coding(data, len, coding) < 0)
		goto exit_0;

	WaveletNode* tree = wavelet_tree_build(data, len, 0, coding);

	BitWriter nodes;
	bitwriter_init(&nodes, NULL);

	BitArray bits;
	bitarray_init(&bits, 0);

	if(wavelet_tree_encode_nodes(tree, &nodes, &bits) < 0)
		goto exit_1;
	if(bitwriter_flush(&nodes) < 0)
		goto exit_1;

	if(bitwriter_write_vbyte(w, len) < 0)
		goto exit_1;
	if(bitwriter_write_vbyte(w, bitwriter_bytelen(&nodes)) < 0)
		goto exit_1;
	if(bitwriter_write_bitwriter(w, &nodes) < 0)
		goto exit_1;
	if(bitwriter_write_bitsequence(w, &bits, p) < 0)
		goto exit_1;
	if(bitwriter_flush(w) < 0)
		goto exit_1;

	res = 0;

exit_1:
	bitwriter_close(&nodes);
	bitarray_destroy(&bits);
	wavelet_node_destroy(tree);
exit_0:
	for(size_t i = 0; i < BYTE_COUNT; i++)
		bitarray_destroy(&coding[i]);

	return res;
}
