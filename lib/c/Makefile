ifeq ($(shell uname),Darwin)
CROSSCOMP	?= i686-elf-
endif

CC  = $(CROSSCOMP)gcc
AR  = $(CROSSCOMP)ar

CFLAGS	= -m32 -ffreestanding -nostdinc -fno-pic -isystem ./include
CFLAGS	+= -O2

.PHONY: clean

srcs := $(wildcard ./src/*.c)
objs       := crt0.o $(patsubst ./src/%.c, ./build_cosec/%.o, $(srcs))
objs_linux := crt0.o $(patsubst ./src/%.c, ./build_linux/%.o, $(srcs))

libc.a: build_cosec cosec.o $(objs)
	$(AR) cr $@ cosec.o $(objs)

# kernel libc, with userspace parts omitted:
libc0.a: build_cosec $(objs)
	$(AR) cr $@ $(objs)

# linux build for testing
libc.linux.a: build_linux linux.o $(objs_linux)
	$(AR) cr $@ linux.o $(objs_linux)

cosec.o: ./os/cosec.c
	$(CC) $(CFLAGS) -c $< -o $@

linux.o: ./os/linux.c
	$(CC) $(CFLAGS) -c $< -o $@

crt0.o: crt0.S
	$(CC) $(CFLAGS) -c $< -o $@

build_cosec:
	@mkdir -p build_cosec

build_linux:
	@mkdir -p build_linux

build_cosec/%.o: ./src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

build_linux/%.o: ./src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -r build* libc*.a *.o || true
