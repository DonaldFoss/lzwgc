/* 
 * This is the header just for LZW-GC, a variation on LZW that will
 * incrementally garbage collect the dictionary with intention to
 * better support large files or streams.
 *
 * In this case, all allocations are up front. Approx:
 *   5 * dictionary size bytes for compress
 *   7 * dictionary size bytes for decompress
 * These APIs use simple assertions (assert.h) for error checking.
 */

#ifndef LZWGC_H

#include <stdbool.h>

typedef struct {
    unsigned short   size;   // size of dictionary
    unsigned short * match_count; // slice of match counts
    unsigned short * prev_token;  // previously matched
    unsigned char  * added_char;  // character matched
    unsigned short   alloc_idx;   // last allocated token
} lzwgc_dict;

typedef struct {
    // internal state
    lzwgc_dict dict;
    unsigned short matched_token;

    // output after each operation (at most one token)
    bool have_output;
    unsigned short token_output;
} lzwgc_compress;


typedef struct {
    // internal state
    lzwgc_dict dict;
    unsigned short prev_token; 
    unsigned char * srbuff;

    // output after each operation (a number of characters)
    unsigned short   output_count;
    unsigned char  * output_chars;
} lzwgc_decompress;

// here 'size' is the total dictionary size, and is the only parameter
// a typical value is 4095 (i.e. 12 bit tokens, reserving 0xfff for client)
// valid range for size is 256..32767.
void lzwgc_dict_init(lzwgc_dict*, unsigned short size);
void lzwgc_dict_fini(lzwgc_dict*);

// Incremental API; element at a time
// compress will receive bytes and output zero or one tokens
// finalization will release memory and usually emits a final output.
void lzwgc_compress_init(lzwgc_compress*, unsigned short size);
void lzwgc_compress_recv(lzwgc_compress*, unsigned char);
void lzwgc_compress_fini(lzwgc_compress*);

// decompress will receive a token and output at least one byte
// finalization will release memory, and in this case never has output.
void lzwgc_decompress_init(lzwgc_decompress*, unsigned short size);
void lzwgc_decompress_recv(lzwgc_decompress*, unsigned short tok);
void lzwgc_decompress_fini(lzwgc_decompress*);

#define LZWGC_H
#endif


