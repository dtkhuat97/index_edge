/**
 * @file dict.h
 * @author FR
 */

#ifndef DICTIONARY_H
#define DICTIONARY_H

#include <reader.h>
#include <bitsequence_r.h>
#include <fmindex.h>
#include <intset.h>

typedef struct {
	uint64_t n;
	BitsequenceReader* bitsnode;
	BitsequenceReader* bitsedge;
	FMIndexReader* fmi;
} DictionaryReader;

DictionaryReader* dictionary_init(Reader* r);
void dictionary_destroy(DictionaryReader* d);

char* dictionary_extract(DictionaryReader* d, uint64_t n, size_t* l);
int64_t dictionary_locate(DictionaryReader* d, const char* p);
bool dictionary_locate_prefix(DictionaryReader* d, const char* p, uint64_t* s, uint64_t* e);

typedef struct {
	FMIndexReader* fmi;
	bool has_next;
	uint64_t next;
	uint64_t limit;
} DictIterator;

/**
 * Normally, it would be better if the `DictIterator` would already sort out duplicate occurrences.
 * However, these occurrences would have to be stored in an `Intset`, so that node and edge labels are stored together.
 * But usually only one type of labels is needed.
 * So iterator will return all found values; no matter whether it is a (duplicate) node or edge label.
 * Filtering takes place in the graph reader for performance reasons.
 */
void dictionary_locate_substr(DictionaryReader* d, const char* p, DictIterator* it);

// return value:
// 1: next element exists
// 0: no next element exists
// -1: error occured
int dictionary_substr_next(DictIterator* it, uint64_t* i);
void dictionary_substr_finish(DictIterator* it); // needed if not iterated to the end

#endif
