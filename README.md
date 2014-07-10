
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
* predictable time-space performance, no allocations after initialization

The reserved token can be used as an escape or stop word for packaging or a layered (e.g. Huffman) encoding.

### Using

Use `make` to generate program `lzwgc` and eventually a separate program for Huffman or Polar encodings.

        lzwgc (x|c) [-bN]
            x to extract, c to compress
            N in range 9..24 (default 12)   
            dict size 2^N-1 (reserving top code)
            processes stdin → stdout

At the moment, lzwgc outputs/inputs 16 bit tokens (bigendian) for up to 16 bits, and 24 bit tokens for up to 24 bits. These are octet aligned, which makes them relatively easy to work with, and it should be easy to compute the ideal packed size. 

Todo: improve compression performance by supporting a fast reverse dictionary lookup, e.g. a hashtable that uses token+character and provides a possible token, and that may have elements removed from it too.

## Compression Quality:

The compression quality for LZW-GC depends only on dictionary size and the input file. In this case, I'm using sizes 2^N - 1 (thus reserving one token, common for stop codes and similar). The figures reported below are for packed tokens. But since I don't actually pack the tokens yet, I simply multiply the effective size by the appropriate factor (e.g. 12/16 for the 12 bit tokens). 

From the [Canterbury Corpus](http://corpus.canterbury.ac.nz/details/cantrbry/RatioByRatio.html). Values here are bits per character (so 4 is a 50% compression ratio, and 2 is 75%; lower is better):

        FILE\BITS               10      12      14      16      18  
        alice29.txt  text      4.06    3.48    3.33    3.69    4.15     
        asyoulik.txt play      4.31    3.75    3.61    4.01    4.52
        cp.html      html      4.20    3.76    4.25    4.86    5.47
        fields.c     Csrc      3.60    3.81    4.44    5.08    5.71
        grammar.lsp  list      4.45    5.19    6.06    6.92    7.79
        kennedy.xls  Excl      1.82    2.01    2.24    2.54    2.74  
        lcet10.txt   tech      4.11    3.49    3.14    3.17    3.55
        plrabn12.txt poem      4.26    3.66    3.37    3.39    3.77
        ptt5         fax       0.98    0.96    0.98    1.10    1.23
        sum          SPRC      3.95    4.08    4.57    5.22    5.87
        xargs.1      man       4.51    5.09    5.94    6.78    7.63

        cantrbry.tar tar       2.72    2.53    2.50    2.63    2.81

Even a casual look will reveal that the compression here isn't very good by modern standards, especially not for smaller files (grammar.lsp, xargs.1). In several cases, the file was smaller than the dictionary. The last file is simply the result of tarballing the files without compression. I expect the compression could in most cases be improved by a subsequent Huffman encoding or similar, but I have not yet written the code for that. 

Sadly, Canterbury corpus does not include LZW for comparison. OTOH, Matt Mahoney's large text challenge (encoding Wikipedia) does include tg


## (Thought): Pre-Initialized Dictionary?

An interesting possibility with LZW-GC is to start with a pre-initialized dictionary, e.g. based on compressing a known input. With the adaptive nature of LZW-GC, we wouldn't be hurt by this even if the actual input varies wildly from the expected. But the advantage of doing so could be significant in cases where we compress lots of small, similar inputs and want to avoid repeated 'warmup' costs.

## 




