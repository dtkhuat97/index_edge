/**
 * @file fm_index_writer.c
 * @author FR
 */

#define _GNU_SOURCE // needed for qsort_r

#include "fm_index_writer.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <divsufsort64.h>
#include <bitarray.h>
#include <eliasfano_list.h>
#include <wavelet_tree_writer.h>
#include <arith.h>
#include <bitsequence.h>
#include <sort_r.h>

struct FMIndexData {
	uint64_t c[256];
	int len_c;

	BitArray rle_bits;
	BitArray rle_select_bits;

	uint64_t* sampled_table;
	size_t sampled_table_size;
	BitArray sampled_bits;

	uint8_t* bwt;
	size_t bwt_len;
};

static int cmp_select_run(const void* v_i, const void* v_j, void* rle_ptr) {
	size_t i = *((size_t*) v_i);
	size_t j = *((size_t*) v_j);
	const uint8_t* rle = rle_ptr;

	if(rle[i] != rle[j])
		return CMP(rle[i], rle[j]);
	return CMP(i, j);
}

// Warning: this functions writes the RL-encoded BWT inside the old one
static int rle_create(uint8_t* bwt, size_t n, size_t* new_len, BitArray* rle_bits, BitArray* select_bits) {
	if(bitarray_init(rle_bits, n) < 0)
		return -1;
	if(bitarray_init(select_bits, n + 1) < 0)
		// rle_bits must not be freed because it will be freed in the function `fm_index_data`
		return -1;

	size_t* run_lengths = malloc(n * sizeof(*run_lengths));
	if(!run_lengths)
		return -1;

	size_t rle_len = 0;
	size_t index_run_lengths = 0;

	size_t i;
	int last = -1; // -1 so it differs from all other values
	for(i = 0; i < n; i++) {
		uint8_t b = bwt[i];
		if(b != last) {
			bitarray_set(rle_bits, i, true);

			bwt[rle_len++] = b;
			run_lengths[index_run_lengths++] = 1;
		}
		else
			run_lengths[index_run_lengths - 1]++; // increment last value

		last = b;
	}

	size_t* indices = malloc(rle_len * sizeof(*indices));
	if(!indices) {
		free(run_lengths);
		return -1;
	}

	for(i = 0; i < rle_len; i++)
		indices[i] = i;

	sort_r(indices, rle_len, sizeof(*indices), cmp_select_run, bwt);

	i = 0;
	size_t index_bits = 0;
	while(i < rle_len & index_bits < n) {
		bitarray_set(select_bits, index_bits, true);

		index_bits += run_lengths[indices[i]];
		i++;
	}
	bitarray_set(select_bits, n, true);

	free(run_lengths);
	free(indices);

	*new_len = rle_len;

	return 0;
}

static int fm_index_data(const uint8_t* bs, size_t n, int sampling, bool rle, struct FMIndexData* data) {
	assert(bs[n - 1] == 0);

	int64_t* sa = malloc(n * sizeof(*sa));
	if(!sa)
		return -1;

	if(divsufsort64(bs, sa, n) < 0)
		goto err_0;

	uint8_t* bwt = malloc(n * sizeof(*bwt));
	if(!bwt)
		goto err_0;

	size_t i;
	for(i = 0; i < n; i++)
		bwt[i] = sa[i] == 0 ? bs[n - 1] : bs[sa[i] - 1];

	size_t n_tmp = n; // save because it may change afterwards
	BitArray rle_bits, select_bits;
	if(rle) {
		size_t new_len;
		if(rle_create(bwt, n, &new_len, &rle_bits, &select_bits) < 0)
			goto err_1;

		n = new_len;
	}

	// create table c
	size_t c[256];
	int len_c = 0;
	memset(c, 0, sizeof(c));

	for(i = 0; i < n; i++) {
		uint8_t b = bwt[i];

		c[b + 1]++;
		if(b > len_c)
			len_c = b;
	}
	len_c += 2; // add 2 to the length of 2
	for(i = 1; i < len_c; i++)
		c[i] += c[i - 1];

	// only used with sampling
	size_t sampled_table_size;
	uint64_t* sampled_table;
	BitArray sampled;

	if(sampling > 0) {
		sampled_table_size = DIVUP(n_tmp, sampling);
		sampled_table = malloc(sampled_table_size * sizeof(*sampled_table));
		if(!sampled_table)
			goto err_1;

		if(bitarray_init(&sampled, n_tmp + 1) < 0) {
			free(sampled_table);
			goto err_1;
		}

		size_t j = 0;
		for(i = 0; i < n_tmp; i++) {
			if(sa[i] % sampling == 0) {
				sampled_table[j++] = sa[i];
				bitarray_set(&sampled, i, true);
			}
		}
		bitarray_set(&sampled, n_tmp, true);
	}

	free(sa);

	memcpy(data->c, c, len_c * sizeof(*c));
	data->len_c = len_c;

	if(rle) {
		data->rle_bits = rle_bits;
		data->rle_select_bits = select_bits;
	}
	if(sampling > 0) {
		data->sampled_table = sampled_table;
		data->sampled_table_size = sampled_table_size;
		data->sampled_bits = sampled;
	}

	data->bwt = bwt;
	data->bwt_len = n;

	return 0;

err_1:
	free(bwt);
	if(rle) {
		bitarray_destroy(&rle_bits);
		bitarray_destroy(&select_bits);
	}
err_0:
	free(sa);

	return -1;
}

static int fm_index_write_sampled_table(uint64_t* sampled, size_t n, BitWriter* w) {
	size_t i;
	uint64_t max_sampled = 0;
	for(i = 0; i < n; i++)
		if(sampled[i] > max_sampled)
			max_sampled = sampled[i];

	int bits_needed = BITS_NEEDED(max_sampled);

	if(bitwriter_write_vbyte(w, bits_needed) < 0)
		return -1;

	for(i = 0; i < n; i++)
		if(bitwriter_write_bits(w, sampled[i], bits_needed) < 0)
			return -1;

	if(bitwriter_flush(w) < 0)
		return -1;

	return 0;
}

int fm_index_write(uint8_t* text, size_t n, int sampling, BitArray* separators, bool rle, BitWriter* w, const BitsequenceParams* p) {
	struct FMIndexData data;

	if(fm_index_data(text, n, sampling, rle, &data) < 0)
		return -1;

	if(sampling > 0) {
		Bitsequence sep;
		if(bitsequence_build(&sep, separators, 0) < 0)
			goto exit_0;

		for(size_t i = 0; i < data.sampled_table_size; i++)
			data.sampled_table[i] = bitsequence_rank1(&sep, data.sampled_table[i]) - 1;

		bitsequence_destroy(&sep);
	}

	int res = -1;
	BitWriter 
		w0, // table c
		w1, // sampled suffix table
		w2, // sampled bits
		w3, // rle bits
		w4; // rle select bits

	bitwriter_init(&w0, NULL); // table c
	if(eliasfano_write(data.c, data.len_c, &w0, p) < 0)
		goto exit_0;

	if(sampling > 0) {
		bitwriter_init(&w1, NULL);
		bitwriter_init(&w2, NULL);

		if(fm_index_write_sampled_table(data.sampled_table, data.sampled_table_size, &w1) < 0) // sampled suffix table
			goto exit_1;
		if(bitwriter_write_bitsequence(&w2, &data.sampled_bits, p) < 0) // sampled bits
			goto exit_1;
	}

	if(rle) {
		bitwriter_init(&w3, NULL);
		bitwriter_init(&w4, NULL);

		if(bitwriter_write_bitsequence(&w3, &data.rle_bits, p) < 0) // rle bits
			goto exit_2;
		if(bitwriter_write_bitsequence(&w4, &data.rle_select_bits, p) < 0) // rle select bits
			goto exit_2;
	}

	if(bitwriter_write_vbyte(w, n) < 0)
		goto exit_2;

	uint8_t opts = ((sampling > 0 ? 1 : 0) << 4) | (rle ? 1 : 0);
	if(bitwriter_write_byte(w, opts) < 0) // with sampling / rle used
		goto exit_2;
	if(bitwriter_write_vbyte(w, bitwriter_bytelen(&w0)) < 0)
		goto exit_2;

	if(sampling > 0) {
		if(bitwriter_write_vbyte(w, bitwriter_bytelen(&w1)) < 0)
			goto exit_2;
		if(bitwriter_write_vbyte(w, bitwriter_bytelen(&w2)) < 0)
			goto exit_2;
	}
	if(rle > 0) {
		if(bitwriter_write_vbyte(w, bitwriter_bytelen(&w3)) < 0)
			goto exit_2;
		if(bitwriter_write_vbyte(w, bitwriter_bytelen(&w4)) < 0)
			goto exit_2;
	}

	if(bitwriter_write_bitwriter(w, &w0) < 0)
		goto exit_2;

	if(sampling > 0) {
		if(bitwriter_write_bitwriter(w, &w1) < 0)
			goto exit_2;
		if(bitwriter_write_bitwriter(w, &w2) < 0)
			goto exit_2;
	}

	if(rle) {
		if(bitwriter_write_bitwriter(w, &w3) < 0)
			goto exit_2;
		if(bitwriter_write_bitwriter(w, &w4) < 0)
			goto exit_2;
	}

	if(wavelet_tree_write(data.bwt, data.bwt_len, w, p) < 0)
		goto exit_2;

	res = 0;

exit_2:
	if(rle) {
		bitwriter_close(&w3);
		bitwriter_close(&w4);
	}
exit_1:
	if(sampling > 0) {
		bitwriter_close(&w1);
		bitwriter_close(&w2);
	}
exit_0:
	bitwriter_close(&w0);

	if(sampling > 0) {
		free(data.sampled_table);
		bitarray_destroy(&data.sampled_bits);
	}
	if(rle) {
		bitarray_destroy(&data.rle_bits);
		bitarray_destroy(&data.rle_select_bits);
	}
	free(data.bwt);

	return res;
}
