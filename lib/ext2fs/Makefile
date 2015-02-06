exe 	:= ext2fs
mnt 	:= mnt

HOST_OS := $(shell uname)

ifeq ($(COSEC),)
else
CFLAGS 	+= -nostdinc -I../../include
ifeq ($(HOST_OS),Darwin)
CC		?= i386-elf-gcc
endif
endif

mount=./$(exe)
ifeq ($(shell uname),Linux)
umount=fusermount -u
else
ifeq ($(shell uname),Darwin)
umount=umount
endif
endif

$(exe): ext2.o
	$(CC) e2fuse.c `pkg-config fuse --cflags --libs` ext2.o -o $(exe)

ext2.o: ext2.c

.PHONY: mount, umount, clean

mount: $(exe)
	mkdir $(mnt)
	$(mount) -d $(mnt) ../../cosec.img

umount:
	$(umount) $(mnt)
	rmdir $(mnt)

clean:
	rm *.o
