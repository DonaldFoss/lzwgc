/* 
 * This is the header just for LZW-GC, a variation on LZW that will
 * incrementally garbage collect the dictionary with intention to
 * better support large files or streams.
 *
 * LZWGC does not provide any real parameters. The dictionary has a
 * fixed size, and tokens are 12 bits. The last token, 0xfff, is for
 * client escapes and integration with Huffman encoding.
 *
 */

#ifndef LZWGC_H

#include <stdio.h>

#define LZWGC_DICT_SIZE 4095
#define LZWGC_DYN_SIZE (LZWGC_DICT_SIZE - 256)

typedef struct {
    unsigned short match_count [LZWGC_DYN_SIZE]; // slice of match counts
    unsigned short prev_token  [LZWGC_DYN_SIZE]; // previously matched
    unsigned char  added_char  [LZWGC_DYN_SIZE]; // character matched
    unsigned short next_token; // supports round-robin incremental GC
} lzwgc_dict_state;

typedef struct {
    lzwgc_dict_state dict;
} lzwgc_compress_state;

typedef struct {
    lzwgc_dict_state dict;
    unsigned char  matched_chars [LZWGC_DYN_SIZE + 1]; // in reverse order! 
    unsigned short matched_pos; // 
} lzwgc_decompress_state;

void lzwgc_dict_init(lzwgc_dict_state*);
void lzwgc_compress_state_init(lzwgc_compress_state*);
void lzwgc_decompress_state_init(lzwgc_decompress_state*);

// FILE based IO is currently primary
void lzwgc_compress(FILE* fRead, FILE* fWrite);
void lzwgc_decompress(FILE* fRead, FILE* fWrite);

#define LZWGC_H
#endif


