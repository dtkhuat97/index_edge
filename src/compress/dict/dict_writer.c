/**
 * @file dict_writer.c
 * @author FR
 */

#include "dict_writer.h"

#include <string.h>
#include <treemap.h>
#include <bitarray.h>
#include <writer.h>
#include <fm_index_writer.h>

// calculate the expected length of the concatted text minus one
static size_t dict_text_len_sum(Treemap* dict) {
	TreemapIterator it;
	treemap_iter(dict, &it);
	size_t text_len; // text length including the 0-byte

	size_t n = 1;
	while(treemap_iter_next_key(&it, &text_len) != NULL)
		n += text_len; // adding the text length with the 0-byte because every text will be concatted with the 0-byte-terminator

	return n;
}

static void dict_concat_text(Treemap* dict, uint8_t* text, BitArray* separators) {
	TreemapIterator it;
	treemap_iter(dict, &it);
	const char* t;
	size_t text_len; // text length including the 0-byte

	size_t i = 1; // the first text starts at index 1

	text[0] = 0; // the first byte is a 0-byte
	if(separators)
		bitarray_set(separators, 0, true);

	while((t = treemap_iter_next_key(&it, &text_len)) != NULL) {
		memcpy(text + i, t, text_len); // copying includes the 0-terminator
		i += text_len;

		if(separators)
			bitarray_set(separators, i - 1, true);
	}
}

int dict_write(Treemap* dict, BitArray* bv, BitArray* be, bool disjunct, int sampling, bool rle, BitWriter* w, const BitsequenceParams* p) {
	int res = -1;

	size_t size = treemap_size(dict);
	size_t n = dict_text_len_sum(dict);

	BitArray separators;
	if(sampling > 0)
		if(bitarray_init(&separators, n) < 0)
			return res;

	uint8_t* text = malloc(n * sizeof(*text));
	if(!text)
		goto exit_0;

	// Create the text and pass the separator bitarray if sampling is used
	dict_concat_text(dict, text, sampling > 0 ? &separators : NULL);

	if(bitwriter_write_vbyte(w, size) < 0)
		goto exit_1;
	if(bitwriter_write_byte(w, disjunct ? 1 : 0) < 0)
		goto exit_1;

	BitWriter w0, w1;
	bitwriter_init(&w0, NULL);

	if(bitwriter_write_bitsequence(&w0, bv, p) < 0)
		goto exit_2;
	if(bitwriter_write_vbyte(w, bitwriter_bytelen(&w0)) < 0)
		goto exit_2;

	if(!disjunct) {
		bitwriter_init(&w1, NULL);
		if(bitwriter_write_bitsequence(&w1, be, p) < 0)
			goto exit_3;
		if(bitwriter_write_vbyte(w, bitwriter_bytelen(&w1)) < 0)
			goto exit_3;
	}

	if(bitwriter_write_bitwriter(w, &w0) < 0)
		goto exit_3;
	if(!disjunct)
		if(bitwriter_write_bitwriter(w, &w1) < 0)
			goto exit_3;

	if(fm_index_write(text, n, sampling, &separators, rle, w, p) < 0)
		goto exit_3;

	res = 0;

exit_3:
	if(!disjunct)
		bitwriter_close(&w1);
exit_2:
	bitwriter_close(&w0);
exit_1:
	free(text);
exit_0:
	if(sampling > 0)
		bitarray_destroy(&separators);

	return res;
}
