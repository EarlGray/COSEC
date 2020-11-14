ifeq ($(shell uname),Darwin)
CROSSCOMP  ?= i686-elf-
endif

CC = $(CROSSCOMP)gcc

LIBC     := ../libc.linux.a

CFLAGS   += -m32 -nostdinc -static -fno-pic
CFLAGS   += -isystem ../include
CFLAGS   += -DLINUX=1

LDFLAGS  += -ffreestanding -nostdlib -static -fno-pie $(LIBC)


.PHONY: clean

lua: ../../liblua.a $(LIBC) lua.c
	$(CC) lua.c ../../liblua.a -o $@ -I../../../include $(CFLAGS) $(LDFLAGS)

%: %.c $(LIBC)
	$(CC) $< -o $@ $(CFLAGS) $(LDFLAGS)

$(LIBC):
	make -C .. $(notdir $(LIBC))

clean:
	-rm *.o
	-find -type f -executable -delete
