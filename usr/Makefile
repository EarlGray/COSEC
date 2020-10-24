ifeq ($(shell uname),Darwin)
CROSSCOMP  ?= i686-elf-
endif

CC = $(CROSSCOMP)gcc

CFLAGS   += -m32 -O1 -nostdinc -static -fno-pic
CFLAGS   += -isystem ../lib/c/include
CFLAGS   += -DCOSEC=1

LDFLAGS  += -ffreestanding -nostdlib -static -fno-pie

LIBC     := ../lib/c/libc.a

.PHONY: clean

init: init.c $(LIBC)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

$(LIBC):
	make -C ../lib/c libc.a

clean:
	rm init *.o || true
