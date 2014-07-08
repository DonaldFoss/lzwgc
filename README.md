
LZW-GC is a variation on the popular [LZW](http://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Welch) algorithm, with intention to make it much more adaptive to large inputs and long-running streams. 

Conventional approaches to adapting LZW for streams involve resetting the dictionary, either by encoding a clear-code or by an external segmenting mechanism. The implementation-dependent decision for where to reset the dictionary becomes a source of indeterminism, which is not a desirable trait for all applications. Further, those historical patterns are valuable and we should be reluctant to forget about them.

LZW-GC does not require resets or many arbitrary decisions.

Instead, an incremental garbage collector (GC) will collect exactly one token for every one allocated. An exponential decay factor slowly removes old and unused patterns from memory to eventually be replaced by new patterns. LZW-GC has all the advantages of a sliding-window GC, but is much simpler and much less arbitrary.

High level characteristics for this implementation of LZW-GC:

* operates on the byte level
* configurable dictionary size in bits (min 9, max 15, default 12)
* reserves topmost token for client; e.g. 0xfff for 12 bits
* suitable for hard realtime, embedded systems

The reserved token is primarily to support a subsequent Huffman or [Polar](http://www.ezcodesample.com/prefixer/prefixer_article.html) encoding. In this case, it would be used as an escape token, to either indicate end of input or perhaps reset the Huffman tree in an adaptive encoding.

## Memory Usage



## (Thought): Pre-Initialized Dictionary?

An interesting possibility with LZW-GC is to start with a pre-initialized dictionary, e.g. based on compressing a known input. With the adaptive nature of LZW-GC, we wouldn't be hurt by this even if the actual input varies wildly from the expected. But the advantage of doing so could be significant in cases where we compress lots of similar smaller inputs and want to avoid the 'warmup' costs.

 Due to the garbage collection process, this will not hinder problem specific compression.


