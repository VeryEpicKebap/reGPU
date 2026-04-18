
all:
	gcc -O3 -march=native -mtune=native regpu.c -o regpu -lX11 -lXext

clean:
	rm regpu
