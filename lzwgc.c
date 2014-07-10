
#include "lzwgc.h"
#include <stdlib.h>
#include <assert.h>

token_t token(uint32_t ix)  { return 256 + ix;  }
uint32_t index(token_t tok) { return tok - 256; }
bool valid_token(lzwgc_dict* dict, token_t tok) {
    return (tok < dict->size) &&
           ((tok < 256) || 
            (tok != dict->prev_token[index(tok)]));
}
uint32_t hash_sc(token_t t, unsigned char c) {
    return ((c << 23) + (t << 11) + (c << 7) + t) * 16180319;
}


void lzwgc_dict_init(lzwgc_dict* dict, uint32_t size) {
    assert(((1<<8) <= size) && (size <= (1<<24)));
    uint32_t const dyn_size = index(size);
    dict->match_count = malloc(dyn_size * sizeof(uint32_t));
    dict->prev_token  = malloc(dyn_size * sizeof(token_t));
    dict->added_char  = malloc(dyn_size * sizeof(unsigned char));
    dict->alloc_idx   = (dyn_size - 1);  // begin allocating at 0, 
    dict->hist_token  = size; // special case: no history yet
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

void lzwgc_dict_update(lzwgc_dict* dict, token_t tok) {
    token_t const tok_received = tok;

    // special case: this is first token; invalid history
    bool const this_is_first_token = (dict->hist_token == dict->size);
    if(this_is_first_token) {
        dict->hist_token = tok_received;
        return;
    }

    // increment match counts
    while(tok >= 256) {
        uint32_t const ix = index(tok);
        dict->match_count[ix] += 1;
        tok = dict->prev_token[ix];
    }
    unsigned char const first_char_of_tok_received = tok;

    // collect an entry for allocation
    uint32_t const ii_max = index(dict->size);
    uint32_t ii = dict->alloc_idx; 
    while(1) {
        ii = (ii + 1) % ii_max;
        if(0 == dict->match_count[ii])
            break;
        dict->match_count[ii] /= 2;
    }
    // remember the GC'd element (to support compression)
    dict->alloc_idx = ii;
    dict->gc_tok  = dict->prev_token[ii];
    dict->gc_char = dict->added_char[ii];

    // add new element to dictionary
    dict->prev_token[ii] = dict->hist_token;
    dict->added_char[ii] = first_char_of_tok_received;
    dict->hist_token = tok_received;
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

void lzwgc_dict_fini(lzwgc_dict* dict) {
    free(dict->match_count); dict->match_count = 0;
    free(dict->prev_token);  dict->prev_token  = 0;
    free(dict->added_char);  dict->added_char  = 0;
}

void lzwgc_compress_init(lzwgc_compress* st, uint32_t size) {
    lzwgc_dict_init(&(st->dict), size);
    st->matched_token = size; // invalid token to start

    // support fast reverse dictionary lookup
    st->ht_size = 2 * size; // so we have about 50% fill for full dict
    size_t const ht_buff_size = sizeof(token_t) * st->ht_size;
    st->ht_content = malloc(ht_buff_size);
    assert(0 != st->ht_content);
    for(uint32_t ii = 0; ii < st->ht_size; ++ii)
        st->ht_content[ii] = 0;
    st->ht_saturation = 0;

    // no output at start
    st->have_output   = false;
    st->token_output  = size;
}

// declare string s+c to be stored as token `loc`
void lzwgc_hashtable_add(lzwgc_compress* st, token_t s, unsigned char c, token_t loc) {
    assert(loc >= 256);
    uint32_t const ht_size = st->ht_size;
    uint32_t ix = hash_sc(s,c) % ht_size;
    while(1) {
        token_t const tok = st->ht_content[ix];
        bool const bSpaceFound = ((tok == loc) || (tok == 1) || (tok == 0));
        if(bSpaceFound) break;
        ix = (ix + 1) % ht_size; // collision
    }
    st->ht_content[ix] = loc; // search for s+c at loc
    // saturation increases when we modify a zero entry
    if(0 == st->ht_content[ix]) {
        st->ht_saturation += 1;
    }
}

// update hashtable whenever we update dictionary
void lzwgc_hashtable_update(lzwgc_compress* st) {
    // remove prior entry, if it exists:
    token_t const tokenTgt = token(st->dict.alloc_idx);
    uint32_t const ht_size = st->ht_size;
    uint32_t ix = hash_sc(st->dict.gc_tok, st->dict.gc_char) % ht_size;
    while(0 != st->ht_content[ix]) {
        if(tokenTgt == st->ht_content[ix]) {
            st->ht_content[ix] = 1; // clear entry (but allow collisions)
            break;
        }
        ix = (ix + 1) % ht_size;
    }

    // add the new entry
    token_t const s = st->dict.prev_token[st->dict.alloc_idx];
    unsigned char c = st->dict.added_char[st->dict.alloc_idx];
    lzwgc_hashtable_add(st,s,c,tokenTgt);    

    // rebuild from dictionary whenever saturation > 80%.
    // since ht_size is over twice dynamic dictionary size, and
    // `1` entries may be rewritten, this won't happen often. 
    bool const bSaturated = (5 * st->ht_saturation) > (4 * st->ht_size);
    if(bSaturated) {
        // clear hashtable (no realloc)
        for(uint32_t ii = 0; ii < st->ht_size; ++ii) {
            st->ht_content[ii] = 0;
        }
        // rebuild from dictionary
        uint32_t const max_dict_index = index(st->dict.size);
        for(uint32_t ii = 0; ii < max_dict_index; ++ii) {
            lzwgc_hashtable_add(st,st->dict.prev_token[ii]
                                  ,st->dict.added_char[ii]
                                  ,token(ii));
        }
    }
}


void lzwgc_compress_recv(lzwgc_compress* st, unsigned char c) {
    token_t const s = st->matched_token;

    // Use hashtable for reverse dictionary lookup.
    uint32_t const ht_size = st->ht_size;
    uint32_t ix = hash_sc(s,c) % ht_size;
    while(0 != st->ht_content[ix]) {
        token_t const tok = st->ht_content[ix];
        ix = (ix + 1) % ht_size;
        if(tok < 256) { continue; } // a deleted entry
        uint32_t const tx = index(tok);
        if((st->dict.added_char[tx] == c) && (st->dict.prev_token[tx] == s)) {
            st->matched_token = tok; // compress s+c into tok
            st->have_output = false; // no output this step
            return;
        }
    }

    // emit completed token (not including added character 'c')
    st->have_output  = (s < st->dict.size);
    st->token_output = s;

    // 'c' will become part of NEXT output
    st->matched_token = (token_t) c;

    // update the dictionary after output
    if(st->have_output) {
        lzwgc_dict_update(&(st->dict),st->token_output);
        lzwgc_hashtable_update(st);
    }
}

// unless input was empty, we should have a final output.
void lzwgc_compress_fini(lzwgc_compress* st) {
    st->have_output  = (st->matched_token < st->dict.size);
    st->token_output = st->matched_token;
    lzwgc_dict_fini(&(st->dict));
}


void lzwgc_decompress_init(lzwgc_decompress* st, uint32_t size) {
    lzwgc_dict_init(&(st->dict), size);

    size_t const szBuff = size * sizeof(unsigned char);
    st->srbuff = malloc(szBuff);
    assert(0 != st->srbuff);

    st->output_count = 0;
    st->output_chars = malloc(szBuff);
    assert(0 != st->output_chars);
}

// decompress will validate the input token for security reasons. 
void lzwgc_decompress_recv(lzwgc_decompress* st, token_t tok) {
    // validate input for safety
    bool const valid_input_token = valid_token(&(st->dict),tok);
    assert(valid_input_token);
    if(!valid_input_token) return;

    // decode the token and compute output
    unsigned char * const sr = st->srbuff; // reversed string
    uint32_t ct = lzwgc_dict_readrev(&(st->dict),tok,sr,st->dict.size);
    uint32_t ix = 0;
    while(ct > 0) { st->output_chars[ix++] = sr[--ct]; }
    st->output_count = ix;

    // add token to dictionary
    lzwgc_dict_update(&(st->dict),tok);
}

void lzwgc_decompress_fini(lzwgc_decompress* st) {
    lzwgc_dict_fini(&(st->dict));
    st->output_count = 0;
    free(st->srbuff); st->srbuff = 0;
    free(st->output_chars); st->output_chars = 0;
}

