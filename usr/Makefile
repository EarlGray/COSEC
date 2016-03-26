CROSSCOMP  =
ifeq ($(shell uname),Darwin)
CROSSCOMP  = /usr/local/gcc-4.8.1-for-linux64/bin/x86_64-pc-linux-
endif

CC = $(CROSSCOMP)gcc

CFLAGS   += -m32 -nostdinc -isystem ../lib/c/include
LDFLAGS  += -ffreestanding -nostdlib -static 

LIBC     := ../lib/c/libc.a
LIBCOSEC := ../lib/c/cosec.o

.PHONY: clean

init: $(LIBC) $(LIBCOSEC) init.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(LIBC):
	make -C ../lib/c libc.a

$(LIBCOSEC):
	make -C ../lib/c cosec.o

clean:
	rm init || true
