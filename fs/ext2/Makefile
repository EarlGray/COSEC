exe 	:= ext2fs
mnt 	:= mnt

ifeq ($(shell uname),Linux)
mount=./$(exe)
umount=fusermount -u
else
ifeq ($(shell uname),Darwin)
umount=umount
endif
endif

$(exe): ext2.o
	gcc e2fuse.c `pkg-config fuse --cflags --libs` ext2.o -o $(exe)

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
