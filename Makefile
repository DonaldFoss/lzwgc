
all: lzwgc 

lzwgc: lzwgc.c lzwgc_main.c
	gcc -Wall -std=c99 $^ -o $@ 

clean:
	rm *~
	rm *.o
	rm lzwgc

