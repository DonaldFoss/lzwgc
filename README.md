
LZW-GC is a variation on the popular [LZW](http://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Welch) algorithm, with intention to make it much more adaptive to large inputs and long-running streams. 

Conventional approaches to adapting LZW for streams involve resetting the dictionary, either by encoding a clear-code or by an external segmenting mechanism. The implementation-dependent decision for where to reset the dictionary becomes a source of indeterminism, which is not a desirable trait for all applications. Further, those historical patterns are valuable and we should be reluctant to forget about them.

LZW-GC does not require resets or many arbitrary decisions.

Instead, an incremental garbage collector (GC) will collect exactly one token for every one allocated. An exponential decay factor slowly removes old and unused patterns from memory to eventually be replaced by new patterns. LZW-GC has all the advantages of a sliding-window GC, but is much simpler and much less arbitrary.

High level characteristics for this implementation of LZW-GC:

* operates on the byte level
* configurable dictionary size in bits (min 9, max 24, default 12)
* reserves topmost token for client; e.g. 0xfff for 12 bits
* heavy but static memory use after initialization

The reserved token is primarily to support a subsequent Huffman or [Polar](http://www.ezcodesample.com/prefixer/prefixer_article.html) encoding. In this case, it would be used as an escape token, to either indicate end of input or perhaps reset the Huffman tree in an adaptive encoding.

## Using

Use `make` to generate program `lzwgc` and eventually a separate program for Huffman or Polar encodings.

        lzwgc (x|c) [-bN]
            x to extract, c to compress
            N in range 9..24 (default 12); dict size 2^N-1
            processes stdin → stdout

Note that compression uses a simplistic exhaustive search and gets very slow for larger numbers of bits. It's designed to test the concept, not get maximal performance.

At the moment, lzwgc outputs/inputs 16 bit tokens (bigendian) for up to 16 bits, and 24 bit tokens for up to 24 bits. These are octet aligned, which makes them relatively easy to work with, and it should be easy to compute the ideal packed size. For now, final packing is left to the Huffman encoder.

## Special Implementation Considerations

Garbage collection needs careful attention: if it isn't identical in the compressor and decompressor, the two will drift. The easiest (but not necessarily most efficient) way to achieve this is to separate the problem, i.e. such that the compressor's own match counts are computed based on its output (as though it were decompressing). 

Unless 'compressor' and 'decompressor' have identical match counts, the dictionaries will drift apart. The decompressor increments match count for every token received, incrementing the match count for that token and its dependencies. The compressor, thus, must increment match count for every token sent and its dependencies. The decompressor does GC - thus reducing match counts - just after processing an input token.

 match only the token received and its dependencies. 

The compressor generates matches just before outputting a token 

* The compressor adds match counts when receiving input

is limited by what the decompressor can predict. To ensure consistent views for both compressor and decompressor, we must GC immediately after adding a token to the dictionary, i.e. such that tokens collected does not depend on future inputs.

Plain old LZW already has a special case where a pattern like `abababa` might emit an unknown token for `aba` that is not yet known to the decompressor. This 'expected' special case happens only when the first and last characters are the same, so can be regenerated.

## (Thought): Pre-Initialized Dictionary?

An interesting possibility with LZW-GC is to start with a pre-initialized dictionary, e.g. based on compressing a known input. With the adaptive nature of LZW-GC, we wouldn't be hurt by this even if the actual input varies wildly from the expected. But the advantage of doing so could be significant in cases where we compress lots of similar smaller inputs and want to avoid the 'warmup' costs.

 Due to the garbage collection process, this will not hinder problem specific compression.


