
OBJS = gzmount.o
CC = gcc
DEBUG = -g -O0
CFLAGS = -Wall -c $(DEBUG) -D_FILE_OFFSET_BITS=64
LFLAGS = -Wall $(DEBUG) -lfuse

all: gzmount

gzmount: $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o gzmount `pkg-config fuse --cflags --libs` `pkg-config zlib --cflags --libs`

#	gcc -g -Wall -O0 -lfuse -D_FILE_OFFSET_BITS=64 vdifs.c -o vdifs

test: gzmount unmount
	./gzmount 1.gz test
	cat test/1
	
clean: unmount
	rm -f gzmount $(OBJS) *~

unmount:
	fusermount -u test || true

