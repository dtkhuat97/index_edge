/**
 * @file wavelet_tree_writer.h
 * @author FR
 */

#ifndef WAVELET_TREE_WRITER_H
#define WAVELET_TREE_WRITER_H

#include <writer.h>

int wavelet_tree_write(const uint8_t* data, size_t len, BitWriter* w, const BitsequenceParams* p);

#endif
