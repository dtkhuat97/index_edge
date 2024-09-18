/**
 * @file fmindex.c
 * @author FR
 */

#include "fmindex.h"

#include <stdlib.h>
#include <reader.h>
#include <string.h>
#include <reader.h>
#include <panic.h>
#include <bitsequence_r.h>
#include <eliasfano.h>
#include <wavelettree.h>

#define EOF_BYTE ((uint8_t) 0) // used as the separator character

FMIndexReader* fmindex_init(Reader* r) {
	size_t nbytes;
	uint64_t n = reader_vbyte(r, &nbytes);
	FileOff off = nbytes;

	uint8_t opts = reader_readbyte(r);
	off++;

	bool sampling = (opts >> 4) != 0;
	bool with_rle = (opts & 0xf) != 0;

	FileOff lenc = reader_vbyte(r, &nbytes);
	off += nbytes;

	FileOff lensuff, lensamplebits, lenrle, lenrleselect;

	if(sampling) {
		lensuff = reader_vbyte(r, &nbytes);
		off += nbytes;

		lensamplebits = reader_vbyte(r, &nbytes);
		off += nbytes;
	}
	if(with_rle) {
		lenrle = reader_vbyte(r, &nbytes);
		off += nbytes;

		lenrleselect = reader_vbyte(r, &nbytes);
		off += nbytes;
	}

	FileOff offc = off;
	off = offc + lenc;

	FileOff offsuff, offsampleb, offrle, offrleselect;

	if(sampling) {
		offsuff = off;
		offsampleb = offsuff + lensuff;
		off += lensuff + lensamplebits;
	}
	if(with_rle) {
		offrle = off;
		offrleselect = offrle + lenrle;
		off += lenrle + lenrleselect;
	}

	Reader rt;
	reader_init(r, &rt, offc);
	EliasFanoReader* c = eliasfano_init(&rt);
	if(!c)
		return NULL;

	BitsequenceReader *sampled, *rle, *rle_select;

	uint64_t sampled_n, sampled_off;
	if(sampling) {
		reader_init(r, &rt, offsuff); // reusing the reader
		sampled_n = reader_vbyte(&rt, &nbytes);
		sampled_off = offsuff + nbytes;

		reader_init(r, &rt, offsampleb); // reusing the reader again
		sampled = bitsequence_reader_init(&rt);
		if(!sampled)
			goto err0;
	}

	if(with_rle) {
		reader_init(r, &rt, offrle);
		rle = bitsequence_reader_init(&rt);
		if(!rle)
			goto err1;

		reader_init(r, &rt, offrleselect);
		rle_select = bitsequence_reader_init(&rt);
		if(!rle_select)
			goto err2;
	}

	reader_init(r, &rt, off);
	WaveletTreeReader* bwt = wavelet_init(&rt);
	if(!bwt)
		goto err3;

	FMIndexReader* f = malloc(sizeof(*f));
	if(!f)
		goto err4;

	f->r = *r;
	f->n = n;
	f->sampling = sampling;
	f->with_rle = with_rle;
	f->c = c;
	f->sampled_n = sampled_n;
	f->sampled_off = sampled_off;
	f->sampled = sampled;
	f->rle = rle;
	f->rle_select = rle_select;
	f->bwt = bwt;

	return f;

err4:
	wavelet_destroy(bwt);
err3:
	if(with_rle)
		bitsequence_reader_destroy(rle_select);
err2:
	if(with_rle)
		bitsequence_reader_destroy(rle);
err1:
	if(sampled)
		bitsequence_reader_destroy(sampled);
err0:
	eliasfano_destroy(c);
	return NULL;
}

void fmindex_destroy(FMIndexReader* f) {
	eliasfano_destroy(f->c);
	if(f->sampling)
		bitsequence_reader_destroy(f->sampled);
	if(f->with_rle) {
		bitsequence_reader_destroy(f->rle);
		bitsequence_reader_destroy(f->rle_select);
	}
	wavelet_destroy(f->bwt);
	free(f);
}

static bool fmi_locate_reg(FMIndexReader* f, const uint8_t* p, size_t n, uint64_t* sp0, uint64_t* ep0) {
	size_t i = n - 1;
	uint8_t c = p[i];

	int64_t sp = eliasfano_get(f->c, c);
	int64_t ep = eliasfano_get(f->c, (int) c + 1) - 1; // cast to prevent overflow
	uint64_t c0;

	while(sp <= ep && i >= 1) {
		c = p[--i];

		c0 = eliasfano_get(f->c, c);
		sp = c0 + wavelet_rank(f->bwt, c, sp - 1);
		ep = c0 + wavelet_rank(f->bwt, c, ep) - 1;
	}

	*sp0 = sp;
	*ep0 = ep;
	return sp <= ep;
}

static bool fmi_locate_rle(FMIndexReader* f, const uint8_t* p, size_t n, uint64_t* sp0, uint64_t* ep0) {
	size_t i = n - 1;
	uint8_t c = p[i];

	int64_t sp = bitsequence_reader_select1(f->rle_select, eliasfano_get(f->c, c) + 1);
	int64_t ep = bitsequence_reader_select1(f->rle_select, eliasfano_get(f->c, (int) c + 1) + 1) - 1; // cast to prevent overflow
	uint64_t c0;

	uint64_t rank;
	while(sp <= ep && i >= 1) {
		c = p[--i];

		c0 = eliasfano_get(f->c, c);

		rank = bitsequence_reader_rank1(f->rle, sp) - 1;
		if(wavelet_access(f->bwt, rank, NULL) == c)
			sp = sp - bitsequence_reader_selectprev1(f->rle, sp);
		else
			sp = 0;
		sp += bitsequence_reader_select1(f->rle_select, c0 + 1 + wavelet_rank(f->bwt, c, rank - 1));

		rank = bitsequence_reader_rank1(f->rle, ep) - 1;
		if(wavelet_access(f->bwt, rank, NULL) == c)
			ep = ep - bitsequence_reader_selectprev1(f->rle, ep);
		else
			ep = -1;
		ep += bitsequence_reader_select1(f->rle_select, c0 + 1 + wavelet_rank(f->bwt, c, rank - 1));
	}

	*sp0 = sp;
	*ep0 = ep;
	return sp <= ep;
}

bool fmindex_locate(FMIndexReader* f, const uint8_t* p, size_t n, uint64_t* sp, uint64_t* ep) {
	if(!f->with_rle)
		return fmi_locate_reg(f, p, n, sp, ep);
	else
		return fmi_locate_rle(f, p, n, sp, ep);
}

static inline uint64_t fmi_sampled_get(FMIndexReader* f, uint64_t i) {
	uint64_t bitoff = 8 * f->sampled_off + f->sampled_n * i;
	reader_bitpos(&f->r, bitoff);

	return reader_readint(&f->r, f->sampled_n);
}

static inline bool fmi_sampled(FMIndexReader* f, uint64_t i) {
	if(!f->sampling)
		return false;
	return bitsequence_reader_access(f->sampled, i);
}

static uint64_t fmi_locate_match_reg(FMIndexReader* f, uint64_t i) {
	uint8_t c = 0xff;
	uint64_t rank;

	while(!fmi_sampled(f, i)) {
		c = wavelet_access(f->bwt, i, &rank);
		if(c == EOF_BYTE)
			break;

		i = eliasfano_get(f->c, c) + rank - 1;
	}

	if(fmi_sampled(f, i))
		i = fmi_sampled_get(f, bitsequence_reader_rank1(f->sampled, i) - 1);
	else // c == $
		i = wavelet_rank(f->bwt, c, i) - 2;

	return i;
}

static uint64_t fmi_locate_match_rle(FMIndexReader* f, uint64_t i) {
	uint8_t c = 0xff;
	uint64_t rank;

	while(!fmi_sampled(f, i)) {
		rank = bitsequence_reader_rank1(f->rle, i) - 1;

		c = wavelet_access(f->bwt, rank, NULL);
		if(c == EOF_BYTE)
			break;

		i = bitsequence_reader_select1(f->rle_select, eliasfano_get(f->c, c) + 1 + wavelet_rank(f->bwt, c, rank - 1)) + i - bitsequence_reader_selectprev1(f->rle, i);
	}

	if(fmi_sampled(f, i))
		i = fmi_sampled_get(f, bitsequence_reader_rank1(f->sampled, i) - 1);
	else { // c == $
		rank = bitsequence_reader_rank1(f->rle, i) - 1;
		uint64_t c0 = eliasfano_get(f->c, c); // c0 has the value 0 because c == $
		uint64_t first_run = bitsequence_reader_select1(f->rle_select, c0 + 1 + wavelet_rank(f->bwt, c, rank) - 1);
		uint64_t index = i - bitsequence_reader_selectprev1(f->rle, i);
		uint64_t first_ch = bitsequence_reader_select1(f->rle_select, c0 + 1);

		i = first_run + index + first_ch - 1;
	}

	return i;
}

uint64_t fmindex_locate_match(FMIndexReader* f, uint64_t i) {
	if(!f->with_rle)
		return fmi_locate_match_reg(f, i);
	else
		return fmi_locate_match_rle(f, i);
}

// reversing the list of bytes
static inline void bytes_reverse(uint8_t* b, size_t len) {
	uint8_t* i = b;
	uint8_t* j = b + (len - 1);
	uint8_t tmp;

	for(; i < j; i++, j--) {
		tmp = *i;
		*i = *j;
		*j = tmp;
	}
}

#define DEFAULT_CAPACITY 16

static uint8_t* fmi_extract_reg(FMIndexReader* f, uint64_t i, size_t* l) {
	uint8_t* res = NULL;
	size_t len = 0;
	size_t cap = 0;

	uint8_t c;
	uint64_t rank;

	for(;;) {
		c = wavelet_access(f->bwt, i, &rank);
		if(c == EOF_BYTE)
			break;

		if(len == cap) { // increase capacity
			cap = !cap ? DEFAULT_CAPACITY : (cap + (cap >> 1));
			res = realloc(res, cap * sizeof(uint8_t));
			if(!res)
				return NULL;
		}
		res[len++] = c; // append c to the end because we reverse after this loop

		i = eliasfano_get(f->c, c) + rank - 1;
	}

	bytes_reverse(res, len);

	*l = len;
	return res;
}

static uint8_t* fmi_extract_rle(FMIndexReader* f, uint64_t i, size_t* l) {
	uint8_t* res = NULL;
	size_t len = 0;
	size_t cap = 0;

	uint8_t c;
	uint64_t rank;

	for(;;) {
		c = wavelet_access(f->bwt, bitsequence_reader_rank1(f->rle, i) - 1, NULL);
		if(c == EOF_BYTE)
			break;

		if(len == cap) { // increase capacity
			cap = !cap ? DEFAULT_CAPACITY : 2 * cap;
			res = realloc(res, cap * sizeof(*res));
			if(!res)
				return NULL;
		}
		res[len++] = c; // append c to the end because we reverse after this loop

		rank = bitsequence_reader_rank1(f->rle, i) - 1;
		if(wavelet_access(f->bwt, rank, NULL) == c)
			i = i - bitsequence_reader_selectprev1(f->rle, i);
		else
			i = 0;
		i += bitsequence_reader_select1(f->rle_select, eliasfano_get(f->c, c) + wavelet_rank(f->bwt, c, rank - 1) + 1);
	}

	bytes_reverse(res, len);

	*l = len;
	return res;
}

uint8_t* fmindex_extract(FMIndexReader* f, uint64_t i, size_t* l) {
	if(!f->with_rle)
		return fmi_extract_reg(f, i, l);
	else
		return fmi_extract_rle(f, i, l);
}
