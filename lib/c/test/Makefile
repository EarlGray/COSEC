CROSSCOMP  =
ifeq ($(shell uname),Darwin)
CROSSCOMP  = /usr/local/gcc-4.8.1-for-linux64/bin/x86_64-pc-linux-
endif

CC = $(CROSSCOMP)gcc

LIBC     := ../libc.linux.a

CFLAGS   += -m32 -nostdinc -static -fno-pic
CFLAGS   += -isystem ../include
CFLAGS   += -DLINUX=1

LDFLAGS  += -ffreestanding -nostdlib -static -fno-pie $(LIBC)


.PHONY: clean

%: %.c $(LIBC)
	$(CC) $< -o $@ $(CFLAGS) $(LDFLAGS)

$(LIBC):
	make -C .. $(notdir $(LIBC))

clean:
	rm *.o || true
