
#include "lzwgc.h"
#include <stdlib.h>
#include <assert.h>

typedef unsigned short i16;

i16 token(i16 ix)  { return 256 + ix;  }
i16 index(i16 tok) { return tok - 256; }
bool valid_token(unsigned short tok, lzwgc_dict* dict) {
    return (tok < dict->size) 
        && ((tok < 256) || 
            (tok != dict->prev_token[index(tok)]));
}

void lzwgc_dict_init(lzwgc_dict* dict, unsigned short size) {
    assert((size >= 256) && (size <= 0x7fff));
    unsigned short const dyn_size = index(size);
    dict->match_count = malloc(dyn_size * sizeof(unsigned short));
    dict->prev_token  = malloc(dyn_size * sizeof(unsigned short));
    dict->added_char  = malloc(dyn_size * sizeof(unsigned char));
    dict->alloc_idx   = dyn_size - 2; // invalid first alloc @ end of array
    dict->size        = size;
    assert(0 != dict->match_count);
    assert(0 != dict->prev_token);
    assert(0 != dict->added_char);
    for(i16 ii = 0; ii < dyn_size; ++ii) {
        dict->match_count[ii] = 0;
        dict->prev_token[ii]  = token(ii); // init cyclic
        dict->added_char[ii]  = 0;
    }
}

// allocate a token-index. 
i16 lzwgc_alloc(lzwgc_dict* dict) {
    i16 const idx_max = index(dict->size);
    i16 ii = dict->alloc_idx; // at last allocation
    while(true) {
        ii = (ii + 1) % idx_max;
        if(0 == dict->match_count[ii]) 
            break;
        dict->match_count[ii] /= 2;
    }
    dict->alloc_idx = ii;
    return ii;
}

void lzwgc_dict_fini(lzwgc_dict* dict) {
    free(dict->match_count); dict->match_count = 0;
    free(dict->prev_token);  dict->prev_token  = 0;
    free(dict->added_char);  dict->added_char  = 0;
}

void lzwgc_compress_init(lzwgc_compress* st, unsigned short size) {
    lzwgc_dict_init(&(st->dict), size);
    st->matched_token = size; // invalid token to start

    st->have_output   = false;
    st->token_output  = size;
}

void lzwgc_compress_recv(lzwgc_compress* st, unsigned char c) {
    i16 const s = st->matched_token;
    i16 const ii_max = index(st->dict.size);

    for(i16 ii = 0; ii < ii_max; ++ii) {
        if((s == st->dict.prev_token[ii]) && (c == st->dict.added_char[ii])) {
            st->dict.match_count[ii] += 1;
            st->matched_token = token(ii);
            st->have_output = false;
            return; // input 'c' will be compressed
        }
    }

    // update dictionary
    i16 const new_dict_ent = lzwgc_alloc(&(st->dict));
    st->dict.prev_token[new_dict_ent] = s;
    st->dict.added_char[new_dict_ent] = c;

    // track tokens not yet output
    st->matched_token = (unsigned short) c;

    // compute output
    st->have_output  = (s < st->dict.size);
    st->token_output = s;
}

// unless input was empty, we should have a final output.
void lzwgc_compress_fini(lzwgc_compress* st) {
    st->have_output  = (st->matched_token < st->dict.size);
    st->token_output = st->matched_token;
    lzwgc_dict_fini(&(st->dict));
}


void lzwgc_decompress_init(lzwgc_decompress* st, unsigned short size) {
    lzwgc_dict_init(&(st->dict), size);
    st->prev_token = size; // invalid token to start

    size_t const szBuff = (size - 255) * sizeof(unsigned char);
    st->srbuff = malloc(szBuff);
    assert(0 != st->srbuff);

    st->output_count = 0;
    st->output_chars = malloc(szBuff);
    assert(0 != st->output_chars);
}


// decompress will go ahead and validate the input token to protect against
// security issues. It will abort in case of 
void lzwgc_decompress_recv(lzwgc_decompress* st, unsigned short rcvTok) {
    // validate token
    bool const input_token_is_valid = valid_token(rcvTok,&(st->dict));
    assert(input_token_is_valid);
    if(!input_token_is_valid) {
        // assertions are disabled? silently ignore bad input...
        st->output_count = 0;
        return;
    }

    // decode the token
    unsigned char * sr = st->srbuff; // reversed string
    i16 ct = 0; // count of characters
    i16 tok = rcvTok;

    while(tok >= 256) {
        i16 const ix = index(tok);
        sr[ct++] = st->dict.added_char[ix];
        st->dict.match_count[ix] += 1;
        tok = st->dict.prev_token[ix];
    }
    unsigned char c = (unsigned char) tok; // final token is just a character
    sr[ct++] = c; 

    // add entry to dictionary for previous input + c.
    i16 const new_dict_ent = lzwgc_alloc(&(st->dict));
    st->dict.prev_token[new_dict_ent] = st->prev_token;
    st->dict.added_char[new_dict_ent] = c;
    st->prev_token = rcvTok;

    // compute output (simply reverse 'sr')
    i16 ix = 0;
    while(ct > 0) { st->output_chars[ix++] = sr[--ct]; }
    st->output_count = ix;
}

void lzwgc_decompress_fini(lzwgc_decompress* st) {
    lzwgc_dict_fini(&(st->dict));
    st->output_count = 0;
    free(st->srbuff); st->srbuff = 0;
    free(st->output_chars); st->output_chars = 0;
}

