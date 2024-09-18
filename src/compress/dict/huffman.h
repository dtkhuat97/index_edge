/**
 * @file huffman.h
 * @author FR
 */

#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stddef.h>
#include <bitarray.h>

#define BYTE_COUNT (256) // number of possible bytes

int huffman_create_coding(const void* data, size_t len, BitArray coding[BYTE_COUNT]);

#endif
