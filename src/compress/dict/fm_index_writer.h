/**
 * @file fm_index_writer.h
 * @author FR
 */

#ifndef FM_INDEX_WRITER_H
#define FM_INDEX_WRITER_H

#include <stdbool.h>
#include <bitarray.h>
#include <writer.h>

int fm_index_write(uint8_t* text, size_t n, int sampling, BitArray* separators, bool rle, BitWriter* w, const BitsequenceParams* p);

#endif
