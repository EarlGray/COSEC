CROSSCOMP  =
ifeq ($(shell uname),Darwin)
CROSSCOMP  = /usr/local/gcc-4.8.1-for-linux64/bin/x86_64-pc-linux-
endif

CC = $(CROSSCOMP)gcc

CFLAGS   += -m32 -nostdinc -isystem ../lib/c/include
LDFLAGS  += -ffreestanding -nostdlib -static -fno-pie

LIBC     := ../lib/c/libc.a

.PHONY: clean

init: init.c $(LIBC)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

$(LIBC):
	make -C ../lib/c libc.a

clean:
	rm init *.o || true
