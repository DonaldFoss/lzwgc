
## Overview

LZW-GC is a variation on the popular [LZW](http://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Welch) algorithm, with intention to make it much more adaptive to large inputs and long-running streams. 

LZW is very optimistic about learning patterns, and tends to add a lot of useless patterns to the dictionary. Then, it stops adding anything. Incremental garbage-collection (GC) of the more useless patterns will at least allow us to make much more efficient use of a fixed-size dictionary, and also allow us to adapt to changing patterns common to large archives.

        typedef int token_t;

        typedef struct {
            int     match_count;    // for GC purposes
            token_t prev_token;     // previous token in string
            char    char_added;     // added to end of string 
        } dict_ent;
        
        typedef struct {
            int size;               // total size (including 256 alphabet tokens)
            dict_ent * entries;     // does not include implicit 256 base tokens
            token_t    prev_token;  // last token observed
            int        alloc_index; // last token allocated
        } dict;

        void update_dict(dict* d, token_t tok) {
            token_t const token_received = tok;

            // update match counts & find first character
            while(tok >= 256) {
                int const tokIndex = (tok - 256);
                d->entries[tokIndex].match_count ++;
                tok = d->entries[tokIndex].prev_token;
            } 
            char const c = (char) tok; // first character
            
            // garbage collect a token
            int const ii_max = d->size - 256;
            int ii = d->alloc_index;
            while(1) {
                ii = (ii + 1) % ii_max; // round robin GC
                if(0 == d->entries[ii].match_count)
                    break;
                d->entries[ii].match_count /= 2;
            }
            d->alloc_index = ii;
            
            // add to dictionary in LZW-like style
            d->entries[ii].prev_token = d->prev_token;
            d->entries[ii].char_added = c;
            d->prev_token  = token_received;
        }

Standard LZW uses a slightly different algorithms to update the dictionary at the compressor and decompressor. Consequently, it must handle a special case for strings like `abababa`. However, it is much more difficult to recognize this special case with a garbage-collecting dictionary. LZW-GC instead opts to keep it simple by using the same algorithm at the compressor and decompressor. Consequently, *the LZW-GC dictionary is a deterministic function of the token stream*. 

Note that garbage collection here is round-robin and uses an *exponential decay* factor (dividing match counts by half), such that old patterns that fall out of favor are quickly deprecated in just a handful of generations. Meanwhile, round-robin collection gives each new pattern a fair opportunity to see reuse.

## LZW-GC vs. Reset the Dictionary

The more typical approach to adaptive dictionary compression is to reserve a token as a 'clear code' then simply reset the dictionary whenever the compression ratio is bad. This approach is used by the `compress` Unix utility, for example. However, this approach has several major disadvantages: First, the decision to throw out the current dictionary is highly heuristic. Second, dictionaries have a long 'warm up' time to become useful, and so repeatedly throwing out the old dictionary means we're repeatedly warming up.

LZW's simple, implementation-independent determinism is a useful feature for some applications, and LZW-GC preserves this and even goes a bit simpler by avoiding LZW's special case. Further, the incremental GC gives us the benefits of throwing out useless patterns without losing the warm-up benefits. 

## This Implementation

High level characteristics for this implementation of LZW-GC:

* operates on the byte level
* dictionary size configured in bit width (9..24)
* reserves topmost token for client; e.g. 0xfff for 12 bits
* SLOW encoding for large dictionary sizes (linear search)
* predictable time-space performance, but not especially efficient

The reserved token can be used as an escape or stop word for packaging or a layered (e.g. Huffman) encoding.

### Using

Use `make` to generate program `lzwgc` and eventually a separate program for Huffman or Polar encodings.

        lzwgc (x|c) [-bN]
            x to extract, c to compress
            N in range 9..24 (default 12)   
            dict size 2^N-1 (reserving top code)
            processes stdin → stdout

Note that compression uses a simplistic exhaustive search and gets very slow for larger numbers of bits. It's designed to test the concept, not get maximal performance. I could introduce some fixed-width reverse lookups (e.g. a hashtable of page-ranges to search). 

At the moment, lzwgc outputs/inputs 16 bit tokens (bigendian) for up to 16 bits, and 24 bit tokens for up to 24 bits. These are octet aligned, which makes them relatively easy to work with, and it should be easy to compute the ideal packed size. 

## (Thought): Pre-Initialized Dictionary?

An interesting possibility with LZW-GC is to start with a pre-initialized dictionary, e.g. based on compressing a known input. With the adaptive nature of LZW-GC, we wouldn't be hurt by this even if the actual input varies wildly from the expected. But the advantage of doing so could be significant in cases where we compress lots of small, similar inputs and want to avoid repeated 'warmup' costs.
