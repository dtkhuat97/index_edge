/**
 * @file huffman.c
 * @author FR
 */

#include "huffman.h"

#include <string.h>

typedef struct _HuffmanNode {
	int value;
	size_t freq;
	struct _HuffmanNode* left;
	struct _HuffmanNode* right;
} HuffmanNode;

static void huffman_enqueue(HuffmanNode** es, size_t k, HuffmanNode* key) {
	while(k > 0) {
		size_t parent = (k - 1) >> 1;
		HuffmanNode* e = es[parent];

		if(key->freq >= e->freq)
			break;

		es[k] = e;
		k = parent;
	}
	es[k] = key;
}

static void huffman_shiftdown(size_t k, HuffmanNode* key, HuffmanNode** es, size_t n) {
	size_t half = n >> 1;
	while(k < half) {
		size_t child = (k << 1) + 1; // assume left child is least
		HuffmanNode* c = es[child];
		size_t right = child + 1;

		if(right < n && c->freq > es[right]->freq)
			c = es[child = right];
		if(key->freq <= c->freq)
			break;

		es[k] = c;
		k = child;
	}
	es[k] = key;
}

static HuffmanNode* huffman_dequeue(HuffmanNode** es, size_t size) {
	HuffmanNode* result;

	if((result = es[0]) != NULL) {
		size_t n = size - 1;
		HuffmanNode* x = es[n];
		es[n] = NULL;
		if(n > 0)
			huffman_shiftdown(0, x, es, n);
	}
	return result;
}

static int huffman_code(HuffmanNode* node, BitArray* code, BitArray coding[BYTE_COUNT]) {
	if(node->value >= 0) { // node is leaf
		coding[node->value] = *code;
		return 0;
	}

	BitArray code_right; // only the right code cloned because `code` can be used for the left subtree
	if(bitarray_clone(&code_right, code) < 0)
		return -1;

	if(bitarray_append(code, false) < 0)
		goto err_1;
	if(bitarray_append(&code_right, true) < 0)
		goto err_1;

	if(huffman_code(node->left, code, coding) < 0)
		return -1;
	if(huffman_code(node->right, &code_right, coding) < 0)
		return -1;

	return 0;

err_1:
	bitarray_destroy(&code_right);
err_0:
	bitarray_destroy(code);
	return -1;
}

static void huffman_tree_destroy(HuffmanNode* node) {
	if(node->left)
		huffman_tree_destroy(node->left);
	if(node->right)
		huffman_tree_destroy(node->right);
	free(node);
}

int huffman_create_coding(const void* data, size_t len, BitArray coding[BYTE_COUNT]) {
	size_t freq[BYTE_COUNT];
	memset(freq, 0, sizeof(freq));

	size_t i;
	for(i = 0; i < len; i++)
		freq[((uint8_t*) data)[i]]++;

	size_t node_count = 0;
	HuffmanNode** nodes = malloc((2 * BYTE_COUNT) * sizeof(HuffmanNode*)); // capacity of 2*256
	if(!nodes)
		return -1;

	HuffmanNode* tmp;
	for(int c = 0; c < BYTE_COUNT; c++) {
		if(freq[c] > 0) {
			tmp = malloc(sizeof(*tmp));
			if(!tmp)
				goto err_0;

			tmp->value = c;
			tmp->freq = freq[c];
			tmp->left = NULL;
			tmp->right = NULL;

			huffman_enqueue(nodes, node_count++, tmp);
		}
	}

	while(node_count > 1) {
		HuffmanNode* n1 = huffman_dequeue(nodes, node_count--);
		HuffmanNode* n2 = huffman_dequeue(nodes, node_count--);

		tmp = malloc(sizeof(*tmp));
		if(!tmp) {
			huffman_tree_destroy(n1);
			huffman_tree_destroy(n2);
			goto err_0;
		}

		tmp->value = -1;
		tmp->freq = n1->freq + n2->freq;
		tmp->left = n1;
		tmp->right = n2;

		huffman_enqueue(nodes, node_count++, tmp);
	}

	HuffmanNode* node = huffman_dequeue(nodes, node_count);
	free(nodes);

	BitArray start;
	bitarray_init(&start, 0);

	int res = huffman_code(node, &start, coding);
	huffman_tree_destroy(node);

	return res;

err_0:
	for(size_t i = 0; i < node_count; i++)
		huffman_tree_destroy(nodes[i]);
	free(nodes);
	return -1;
}
