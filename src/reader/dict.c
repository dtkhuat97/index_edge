/**
 * @file dict.c
 * @author FR
 */

#include "dict.h"

#include <string.h>
#include <stdlib.h>
#include <reader.h>
#include <bitsequence_r.h>
#include <fmindex.h>
#include <arith.h>
#include <intset.h>

DictionaryReader* dictionary_init(Reader* r) {
	size_t nbytes;
	uint64_t n = reader_vbyte(r, &nbytes);
	FileOff off = nbytes;

	bool disjunct = reader_readbyte(r) != 0;
	off++;

	FileOff lenbitsnode = reader_vbyte(r, &nbytes);
	off += nbytes;

	FileOff lenbitsedge, offbitsedge, offfmi;
	if(!disjunct) {
		lenbitsedge = reader_vbyte(r, &nbytes);
		off += nbytes;

		offbitsedge = off + lenbitsnode;
		offfmi = offbitsedge + lenbitsedge;
	}
	else
		offfmi = off + lenbitsnode;

	Reader rt;
	reader_init(r, &rt, off); // init this reader on the stack
	BitsequenceReader* bn = bitsequence_reader_init(&rt);
	if(!bn)
		return NULL;

	BitsequenceReader* be;
	if(!disjunct) { // bitsequence exists, because the node and edge labels are not disjunct
		reader_init(r, &rt, offbitsedge); // reuse this reader because it is not needed anymore by the bitsequence
		be = bitsequence_reader_init(&rt);
		if(!be)
			goto err0;
	}
	else
		be = NULL;

	reader_init(r, &rt, offfmi); // reuse again
	FMIndexReader* fmi = fmindex_init(&rt);
	if(!fmi)
		goto err1;

	DictionaryReader* d = malloc(sizeof(*d));
	if(!d)
		goto err2;

	d->n = n;
	d->bitsnode = bn;
	d->bitsedge = be;
	d->fmi = fmi;

	return d;

err2:
	fmindex_destroy(fmi);
err1:
	if(be)
		bitsequence_reader_destroy(be);
err0:
	bitsequence_reader_destroy(bn);
	return NULL;
}

void dictionary_destroy(DictionaryReader* d) {
	bitsequence_reader_destroy(d->bitsnode);
	bitsequence_reader_destroy(d->bitsedge);
	fmindex_destroy(d->fmi);
	free(d);
}

char* dictionary_extract(DictionaryReader* d, uint64_t i, size_t* l) {
	if(i < 0 || i >= d->n) {
		*l = 0;
		return NULL;
	}

	i = i == (d->n - 1) ? 0 : i + 2;

	size_t len;
	uint8_t* res = fmindex_extract(d->fmi, i, &len);
	if(!res) {
		*l = 0;
		return NULL;
	}

	// realloc to store the terminating null byte
	res = realloc(res, (len + 1) * sizeof(*res));
	if(!res) {
		*l = 0;
		return NULL;
	}
	res[len] = '\0';

	*l = len;
	return (char*) res;
}

int64_t dictionary_locate(DictionaryReader* d, const char* p) {
	size_t len = strlen(p);

	uint8_t* b = malloc(len + 2);
	b[0] = '\0';
	memcpy(b + 1, p, len);
	b[len + 1] = '\0';

	uint64_t sp, ep;
	bool f = fmindex_locate(d->fmi, b, len + 2, &sp, &ep);

	free(b);
	return f ? sp - 1 : -1;
}

bool dictionary_locate_prefix(DictionaryReader* d, const char* p, uint64_t* s, uint64_t* e) {
	if(!p || *p == '\0') // empty strings not allowed in prefix search
		return false;

	size_t len = strlen(p);

	uint8_t* b = malloc(len + 1);
	b[0] = '\0';
	memcpy(b + 1, p, len);

	uint64_t sp, ep;
	bool f = fmindex_locate(d->fmi, b, len + 1, &sp, &ep);

	free(b);
	if(!f)
		return false;

	*s = sp - 1;
	*e = ep - 1;
	return true;
}

void dictionary_locate_substr(DictionaryReader* d, const char* p, DictIterator* it) {
	uint64_t sp, ep;
	if(!p || *p == '\0' || !fmindex_locate(d->fmi, (const uint8_t*) p, strlen(p), &sp, &ep)) {
		it->has_next = false;
		it->next = 0;
		it->limit = 0;
	}
	else {
		it->fmi = d->fmi;
		it->has_next = true;
		it->next = sp;
		it->limit = ep;
	}
}

int dictionary_substr_next(DictIterator* it, uint64_t* i) {
	if(!it->has_next)
		return 0;

	if(it->next <= it->limit) {
		uint64_t match = fmindex_locate_match(it->fmi, it->next++);

		*i = match;
		return 1;
	}

	dictionary_substr_finish(it);
	return 0;
}

void dictionary_substr_finish(DictIterator* it) {
	if(it->has_next)
		it->has_next = false;
}
