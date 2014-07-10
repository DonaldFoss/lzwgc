
#include "lzwgc.h"
#include <stdlib.h>
#include <assert.h>

token_t token(uint32_t ix)  { return 256 + ix;  }
uint32_t index(token_t tok) { return tok - 256; }

void lzwgc_dict_init(lzwgc_dict* dict, uint32_t size) {
    assert(((1<<8) <= size) && (size <= (1<<24)));
    uint32_t const dyn_size = index(size);
    dict->match_count = malloc(dyn_size * sizeof(uint32_t));
    dict->prev_token  = malloc(dyn_size * sizeof(token_t));
    dict->added_char  = malloc(dyn_size * sizeof(unsigned char));
    dict->alloc_next  = dyn_size - 1; // first real alloc at 0x100
    dict->size        = size;
    assert(0 != dict->match_count);
    assert(0 != dict->prev_token);
    assert(0 != dict->added_char);
    for(uint32_t ii = 0; ii < dyn_size; ++ii) {
        dict->match_count[ii] = 0;
        dict->prev_token[ii]  = token(ii); // init to invalid token
        dict->added_char[ii]  = 0;
    }
}

uint32_t lzwgc_dict_readrev(lzwgc_dict* dict, token_t tok, unsigned char* sr, uint32_t size) {
    if(tok > dict->size) return 0;

    uint32_t ct = 0;
    while((tok >= 256) && (ct < size)) {
        uint32_t const ix = index(tok);
        sr[ct++] = dict->added_char[ix];
        tok = dict->prev_token[ix];
    }
    if(ct < size) 
        sr[ct++] = (unsigned char) tok;
    return ct;
}

// allocate the next token-index, with knowledge on token emitted
void lzwgc_alloc(lzwgc_dict* dict, token_t tok) {

    if(tok < dict->size) {
        while(tok >= 256) {
            uint32_t const ix = index(tok);
            dict->match_count[ix]++;
            tok = dict->prev_token[ix];
        }
    }

    uint32_t const idx_max = index(dict->size);
    uint32_t ii = dict->alloc_next; // at last allocation
    while(true) {
        ii = (ii + 1) % idx_max;
        if(0 == dict->match_count[ii]) 
            break;
        dict->match_count[ii] /= 2;
    }
    dict->alloc_next = ii;
}

void lzwgc_dict_fini(lzwgc_dict* dict) {
    free(dict->match_count); dict->match_count = 0;
    free(dict->prev_token);  dict->prev_token  = 0;
    free(dict->added_char);  dict->added_char  = 0;
}

void lzwgc_compress_init(lzwgc_compress* st, uint32_t size) {
    lzwgc_dict_init(&(st->dict), size);
    st->matched_token = size; // invalid token to start

    st->have_output   = false;
    st->token_output  = size;
}

void lzwgc_compress_recv(lzwgc_compress* st, unsigned char c) {
    assert ((token_t)c < 256); // assuming 8 bit characters

    token_t const s = st->matched_token;
    uint32_t const ii_max = index(st->dict.size);

    for(uint32_t ii = 0; ii < ii_max; ++ii) {
        if((c == st->dict.added_char[ii]) && (s == st->dict.prev_token[ii]) // match s+c
            && (ii != st->dict.alloc_next))      // do not match the free token!
        {
            st->matched_token = token(ii);
            st->have_output = false;
            return; // input 'c' will be compressed
        }
    }


    // emit the encoded string (if it isn't the empty string)
    st->have_output  = (s < st->dict.size);
    st->token_output = s;

    // add s+c to the dictionary
    uint32_t const new_dict_ent = st->dict.alloc_next;
    st->dict.prev_token[new_dict_ent] = s;
    st->dict.added_char[new_dict_ent] = c;
    st->matched_token = (token_t) c;
    lzwgc_alloc(&(st->dict),st->token_output);

    // begin searching for matches starting with c
}

// unless input was empty, we should have a final output.
void lzwgc_compress_fini(lzwgc_compress* st) {
    st->have_output  = (st->matched_token < st->dict.size);
    st->token_output = st->matched_token;
    lzwgc_dict_fini(&(st->dict));
}


void lzwgc_decompress_init(lzwgc_decompress* st, uint32_t size) {
    lzwgc_dict_init(&(st->dict), size);
    st->prev_token = size; // invalid token to start
    lzwgc_alloc(&(st->dict), st->prev_token); // prealloc

    size_t const szBuff = size * sizeof(unsigned char);
    st->srbuff = malloc(szBuff);
    assert(0 != st->srbuff);

    st->output_count = 0;
    st->output_chars = malloc(szBuff);
    assert(0 != st->output_chars);
}

void lzwgc_decompress_recv_next(lzwgc_decompress* st);

// decompress will validate the input token for security reasons. 
void lzwgc_decompress_recv(lzwgc_decompress* st, token_t rcvTok) {

    // special case for decompression; e.g. for `abababa`
    if(index(rcvTok) == st->dict.alloc_next) {
        lzwgc_decompress_recv_next(st);
        return;
    }

    // validate a normal token:
    bool const valid_input_token = (rcvTok < st->dict.size) &&
            ((rcvTok < 256) || (rcvTok != st->dict.prev_token[index(rcvTok)]));

    assert(valid_input_token);
    if(!valid_input_token) return;

    // decode the token
    unsigned char * const sr = st->srbuff; // reversed string
    uint32_t ct = lzwgc_dict_readrev(&(st->dict),rcvTok,sr,st->dict.size);

    // add entry to dictionary for previous input + c.
    uint32_t const new_dict_ent = st->dict.alloc_next;
    st->dict.prev_token[new_dict_ent] = st->prev_token; // previous input
    st->dict.added_char[new_dict_ent] = sr[ct-1]; // first character from rcvTok
    st->prev_token = rcvTok;
    lzwgc_alloc(&(st->dict),st->prev_token);

    // compute output (simply reverse 'sr')
    uint32_t ix = 0;
    while(ct > 0) { st->output_chars[ix++] = sr[--ct]; }
    st->output_count = ix;
}


// a known special case for LZW is when the next string starts and
// ends with the same character, and for which the initial substring
// is known. E.g. the string `abababa` should invoke this:
void lzwgc_decompress_recv_next(lzwgc_decompress* st) {

    // validate prior token for security
    bool const valid_prior_token = (st->prev_token < st->dict.size);
    assert(valid_prior_token);
    if(!valid_prior_token) return;

    // decode the prior token:
    unsigned char * const sr = st->srbuff; // reversed string
    uint32_t ct = lzwgc_dict_readrev(&(st->dict),st->prev_token,sr+1,st->dict.size);
    sr[0] = sr[ct++];

    // add entry to dictionary for previous input + c.
    uint32_t const new_dict_ent = st->dict.alloc_next;
    st->dict.prev_token[new_dict_ent]  = st->prev_token;
    st->dict.added_char[new_dict_ent]  = sr[0];
    st->prev_token = token(new_dict_ent);
    lzwgc_alloc(&(st->dict),st->prev_token);

    // compute output (simply reverse 'sr')
    uint32_t ix = 0;
    while(ct > 0) { st->output_chars[ix++] = sr[--ct]; }
    st->output_count = ix;
}

void lzwgc_decompress_fini(lzwgc_decompress* st) {
    lzwgc_dict_fini(&(st->dict));
    st->output_count = 0;
    free(st->srbuff); st->srbuff = 0;
    free(st->output_chars); st->output_chars = 0;
}

