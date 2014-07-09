
#include "lzwgc.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

char const * helpText() { 
    return "Usage: lzwgc (x|c) [-bN]\n" 
           "  x to extract; c to compress\n"
           "  N in range 9..24, default 12\n"
           "  processes stdin to stdout\n"; 
}

void help_exit() {
    fputs(helpText(),stderr);
    exit(1);
}

void compress(FILE* in, FILE* out, int dict_bits);
void decompress(FILE* in, FILE* out, int dict_bits);

int main(int argc, char const * args[]) {
    if(argc < 2) help_exit();
    
    char mode = args[1][0];
    unsigned short bits = 12;
    for(int ii = 2; ii < argc; ++ii) {
        char const* arg = args[ii];
        if(arg[0] == '-' && arg[1] == 'b') {
            bits = atoi(arg+2);
        } else {
            fprintf(stderr,"unrecognized token: %s\n",arg);
            help_exit();
        }
    }
    bool const valid_bits = (9 <= bits) && (bits <= 24);
    if(!valid_bits) help_exit();

    if('c' == mode)      compress(stdin,stdout,bits);
    else if('x' == mode) decompress(stdin,stdout,bits);
    else help_exit();

    return 0;
}

// for now, write tokens unpacked bigendian, 2-3 octets
void write_token(FILE* out, token_t tok, int bits) {
    int const t16 = (tok>>16) & 0xff;
    int const t8  = (tok>>8) & 0xff;
    int const t0  = tok & 0xff;
    if(bits > 16) fputc(t16,out);
    fputc(t8,out);
    fputc(t0,out);
}
bool read_token(FILE* in, token_t* tok, int bits) {
    int t16 = 0;
    int t8  = 0;
    int t0  = 0;
    if(bits > 16) t16 = fgetc(in);
    t8 = fgetc(in);
    t0 = fgetc(in);
    (*tok) = ((t16 & 0xff)<<16) + ((t8 & 0xff)<<8) + (t0 & 0xff);
    return (t16 != EOF) && (t8 != EOF) && (t0 != EOF);
}



// compress will process input to output.
// this uses the knowledge that N inputs can produce at most N outputs.
void compress(FILE* in, FILE* out, int bits) {
    lzwgc_compress st;
    uint32_t const dict_size = (1 << bits) - 1; // reserve top token for client
    size_t const buff_size = 4096;
    unsigned char read_buff[buff_size];
    lzwgc_compress_init(&st,dict_size);

    while(!feof(in)) {
        size_t read_count = fread(read_buff,1,buff_size,in);
        for(size_t ii = 0; ii < read_count; ++ii) {
            lzwgc_compress_recv(&st,read_buff[ii]);
            if(st.have_output) {
                write_token(out, st.token_output, bits);
            }
        }
    } 
    lzwgc_compress_fini(&st);
    if(st.have_output) {
        write_token(out, st.token_output, bits);
    }
}


void decompress(FILE* in, FILE* out, int bits) {
    lzwgc_decompress st;
    uint32_t const dict_size = (1 << bits) - 1; // reserve top token for client
    lzwgc_decompress_init(&st,dict_size);
    token_t tok;
    while(read_token(in,&tok,bits)) {
        lzwgc_decompress_recv(&st,tok);
        fwrite(st.output_chars,1,st.output_count,out);
    }
    lzwgc_decompress_fini(&st);
}






