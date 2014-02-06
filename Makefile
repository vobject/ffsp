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

tags:
	ctags -R --c++-kinds=+p --fields=+iaS --extra=+q . ../test/fuse-2.8.5 /usr/include/stdint.h /usr/include/unistd.h /usr/include/stdlib.h /usr/include/stdio.h /usr/include/string.h /usr/include/errno.h /usr/include/fcntl.h /usr/include/getopt.h /usr/include/linux/stddef.h /usr/include/sys/stat.h

clean:
	rm *.o mkfs.ffsp mount.ffsp
	
.PHONY: all tags clean
