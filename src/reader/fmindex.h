/**
 * @file fmindex.h
 * @author FR
 */

#ifndef FMINDEX_H
#define FMINDEX_H

#include <stdbool.h>
#include <reader.h>
#include <eliasfano.h>
#include <bitsequence_r.h>
#include <wavelettree.h>

typedef struct {
	Reader r;

	uint64_t n;
	bool sampling;
	bool with_rle;
	EliasFanoReader* c;

	// Only used with sampling:
	uint64_t sampled_n;
	uint64_t sampled_off;
	BitsequenceReader* sampled;

	// Only used with RLE:
	BitsequenceReader* rle;
	BitsequenceReader* rle_select;

	WaveletTreeReader* bwt;
} FMIndexReader;

FMIndexReader* fmindex_init(Reader* r);
void fmindex_destroy(FMIndexReader* f);

// Other functions not used by the Python webserver
bool fmindex_locate(FMIndexReader* f, const uint8_t* p, size_t n, uint64_t* sp, uint64_t* ep);
uint64_t fmindex_locate_match(FMIndexReader* f, uint64_t i);
uint8_t* fmindex_extract(FMIndexReader* f, uint64_t i, size_t* l);

#endif
