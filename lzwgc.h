/* 
 * This is the header just for LZW-GC, a variation on LZW that will
 * incrementally garbage collect the dictionary with intention to
 * better support large files or streams.
 *
 * These APIs use simple assertions (assert.h) for error checking.
 */

#ifndef LZWGC_H

#include <stdbool.h>
#include <stdint.h>

typedef uint32_t token_t; // documentation some integers as tokens

typedef struct {
    uint32_t        size;        // size of dictionary
    uint32_t      * match_count; // slice of match counts
    token_t       * prev_token;  // previously matched
    unsigned char * added_char;  // character matched
    uint32_t        alloc_next;  // the current 'free' token index
} lzwgc_dict;

typedef struct {
    // internal state
    lzwgc_dict      dict;
    token_t         matched_token;

    // output after each operation (at most one token)
    bool            have_output;
    token_t         token_output;
} lzwgc_compress;


typedef struct {
    // internal state
    lzwgc_dict      dict;
    token_t         prev_token; 
    unsigned char * srbuff;

    // output after each operation (a number of characters)
    uint32_t        output_count;
    unsigned char * output_chars;
} lzwgc_decompress;

// here 'size' is the total dictionary size, and is the only parameter
// typical size is 2^N-1 reserving topmost; e.g. 4095 reserving 0xfff
// valid range is 2^8 to 2^24. Though, this implementation uses a simple
// linear searches so becomes very inefficient once the search doesn't
// trivially fit cache. 
void lzwgc_dict_init(lzwgc_dict*, uint32_t size);
void lzwgc_dict_fini(lzwgc_dict*);

// fetch at most count elements (reversed)
uint32_t lzwgc_dict_readrev(lzwgc_dict*, token_t, unsigned char*, uint32_t count); 

// Incremental API; element at a time
// compress will receive bytes and output zero or one tokens
// finalization will release memory and usually emits a final output.
void lzwgc_compress_init(lzwgc_compress*, uint32_t size);
void lzwgc_compress_recv(lzwgc_compress*, unsigned char);
void lzwgc_compress_fini(lzwgc_compress*);

// decompress will receive a token and output at least one byte
// finalization will release memory, and in this case never has output.
void lzwgc_decompress_init(lzwgc_decompress*, uint32_t size);
void lzwgc_decompress_recv(lzwgc_decompress*, token_t  tok);
void lzwgc_decompress_fini(lzwgc_decompress*);

#define LZWGC_H
#endif


