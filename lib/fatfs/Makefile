FUSEEXE := fatfs_fuse

CFLAGS += $(shell pkg-config --cflags fuse)
LDFLAGS += $(shell pkg-config --libs fuse)

$(FUSEEXE): fatfuse.o
	$(CC) $^ $(LDFLAGS) -o $@

fatfuse.o : fatfuse.c

.PHONY: clean mnt umount
clean:
	rm *.o $(FUSEEXE)

mnt:
	mkdir -p ./mnt
	./$(FUSEEXE) ./mnt

umount:
	fusermount -u ./mnt
