CC		:= gcc
#CC		:= clang -fno-color-diagnostics
CFLAGS		+= -O2 -Wall -Wextra -pedantic -std=c99 -g $(shell pkg-config fuse --cflags)
LDFLAGS		+= $(shell pkg-config fuse --libs)
VPATH		:= libffsp

all: mkfs.ffsp mount.ffsp

mkfs.ffsp: mkfs.ffsp.o log.o debug.o io_raw.o io.o utils.o inode_cache.o inode_group.o inode.o eraseblk.o gc.o summary.o
	${CC} -o $@ $^ ${LDFLAGS}

mount.ffsp: mount.ffsp.o log.o debug.o io_raw.o io.o utils.o inode_cache.o inode_group.o inode.o eraseblk.o mount.o gc.o summary.o
	${CC} -o $@ $^ ${LDFLAGS}

scan:
	scan-build -k make all

clean:
	rm *.o mkfs.ffsp mount.ffsp
	
.PHONY: all clean
