
LZW-GC is a variation on the popular [LZW](http://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Welch) algorithm, with intention to make it much more adaptive to large inputs and long-running streams. 

Conventional approaches to adapting LZW for streams involve resetting the dictionary, either by encoding a clear-code or by an external segmenting mechanism. The implementation-dependent decision for where to reset the dictionary becomes a source of indeterminism, which is not a desirable trait for all applications. Further, those historical patterns are valuable and we should be reluctant to forget about them.

LZW-GC does not require resets or many arbitrary decisions.

Instead, an incremental garbage collector (GC) will collect exactly one token for every one allocated. An exponential decay factor slowly removes old and unused patterns from memory to eventually be replaced by new patterns. LZW-GC has all the advantages of a sliding-window GC, but is much simpler and much less arbitrary.

High level characteristics for this implementation of LZW-GC:

* operates on the byte level 
* fixed dictionary size of 4095 (0..0xffe)
* token 0xfff is escape for client use
* encoding is 12 bits per token, bigendian

The escape token is primarily to support a subsequent Huffman or [Polar](http://www.ezcodesample.com/prefixer/prefixer_article.html) encoding. For example, we might escape then follow with the 1 bit to indicate we're finished, or the 0 bit to reset the Huffman tree and continue. 

## Memory Usage

Encoding requires about 20kB of memory, and decoding a bit more. 


## Using

This project is simple and small scale: just a few C files, headers, and a Makefile. 










