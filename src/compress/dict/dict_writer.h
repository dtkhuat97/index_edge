/**
 * @file dict_writer.h
 * @author FR
 */

#ifndef DICT_WRITER_H
#define DICT_WRITER_H

#include <stdbool.h>
#include <treemap.h>
#include <writer.h>

int dict_write(Treemap* dict, BitArray* bv, BitArray* be, bool disjunct, int sampling, bool rle, BitWriter* w, const BitsequenceParams* p);

#endif
