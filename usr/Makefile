CROSSCOMP  =
ifeq ($(shell uname),Darwin)
CROSSCOMP  = i386-elf-
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
