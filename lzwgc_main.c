
#include "lzwgc.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char const * helpText() { 
    return "Usage: lzwgc (x|z) [-bN]\n" 
           "  N in range 9..15, default 12\n"
           "  processes stdin → stdout\n"; 
}

void help_exit() {
    fputs(helpText(),stderr);
    exit(1);
}

void compress(FILE* in, FILE* out, unsigned char dict_bits);
void decompress(FILE* in, FILE* out, unsigned char dict_bits);

int main(int argc, char const * args[]) {
    if(argc < 2) help_exit();
    
    char mode = args[1][0];
    unsigned short bits = 12;
    for(int ii = 2; ii < argc; ++ii) {
        char const* arg = args[ii];
        if(arg[0] == '-' && arg[1] == 'b')
            bits = atoi(arg+2);
    }

    bool const valid_mode = (('x' == mode) || ('z' == mode)) 
                        &&  ((9 <= bits) && (bits <= 15));
    if(!valid_mode) help_exit();

    if('z' == mode)      compress(stdin,stdout,bits);
    else if('x' == mode) decompress(stdin,stdout,bits);
    else exit(-1);

    return 0;
}

// C doesn't make it easy to work with tokens not aligned at 8 bits...
// this is a rather specific implementation in the context of files.
typedef struct {
    unsigned short bits_per_tok;
    unsigned short data; // 'left over' bits of data
    unsigned short bits; // how many low bits of 'data' are valid?
} tokenizer;

void tokenizer_init(tokenizer* tk, unsigned short bits_per_tok) { 
    tk->bits_per_tok = bits_per_tok;
    tk->data = 0;
    tk->bits = 0;
}

void write_tokens(tokenizer*,unsigned short* tokens, size_t count, FILE* stream);
size_t read_tokens(tokenizer*,unsigned short* tokens, size_t count, FILE* stream);

// compress will process input to output.
// this uses the knowledge that N inputs can produce at most N outputs.
void compress(FILE* in, FILE* out, unsigned char bits) {
    lzwgc_compress st;
    tokenizer_st tk;
    unsigned short const dict_size = (1 << bits) - 1; // reserve top token for client
    static const size_t buff_size = 4096;
    unsigned char  read_buff[buff_size];
    unsigned short tok_buff[buff_size];
    size_t read_count;
    size_t tok_count;

    lzwgc_compress_init(&st,dict_size);
    tokenizer_init(&tk,bits);

    while(!feof(in)) {
        size_t read_count = fread(read_buff,1,buff_size,in);
        size_t tok_count = 0;
        for(size_t ii = 0; ii < read_count; ++ii) {
            lzwgc_compress_recv(&st,read_buff[ii]);
            if(st->have_output)
                tok_buff[tok_count++] = st->token_output;
        }
        write_tokens(&tk,tok_buff,tok_count,out);
    } 
    lzwgc_compress_fini(&st);
    if(st->have_output) {
        write_tokens(out,&(st->token_output),1);
    }
}
void decompress(FILE* in, FILE* out, unsigned char bits) {
    lzwgc_decompress st;
    tokenizer tk; 
    unsigned short dict_size = (1 << bits) - 1; // reserve top token for client


    lzwgc_decompress_init(&st,dict_size);
    tokenizer_init(&tk,bits);

}






