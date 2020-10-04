CROSSCOMP =
ifeq ($(shell uname),Darwin)
CROSSCOMP = /usr/local/gcc-4.8.1-for-linux64/bin/x86_64-pc-linux-
endif

CC  = $(CROSSCOMP)gcc
AR  = $(CROSSCOMP)ar

CFLAGS	= -m32 -ffreestanding -nostdinc -isystem ./include

.PHONY: clean

srcs := $(wildcard ./src/*.c)
objs := crt0.o $(patsubst ./src/%.c, ./build/%.o, $(srcs))

libc.a: build cosec.o $(objs)
	$(AR) cr $@ cosec.o $(objs)

# kernel libc, with userspace parts omitted:
libc0.a: build $(objs)
	$(AR) cr $@ $(objs)

# linux build for testing
libc.linux.a: build linux.o $(objs)
	$(AR) cr $@ linux.o $(objs)

cosec.o: ./os/cosec.c
	$(CC) $(CFLAGS) -c $< -o $@

linux.o: ./os/linux.c
	$(CC) $(CFLAGS) -c $< -o $@

crt0.o: crt0.S
	$(CC) $(CFLAGS) -c $< -o $@

build:
	@mkdir -p build

./build/%.o: ./src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -r build libc*.a *.o || true
