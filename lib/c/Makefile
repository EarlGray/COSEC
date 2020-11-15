ifeq ($(shell uname),Darwin)
CROSSCOMP	?= i686-elf-
endif

CC  = $(CROSSCOMP)gcc
AR  = $(CROSSCOMP)ar

CFLAGS	+= -m32 -ffreestanding -nostdinc -fno-pic -isystem ./include
CFLAGS	+= -O2

ifneq ($(DEBUG),)
CFLAGS  += -g
endif

.PHONY: clean

srcs := $(wildcard ./src/*.c)
objs_linux := crt0.o i386.o $(patsubst ./src/%.c, ./build_linux/%.o, $(srcs))
objs_cosec := crt0.o i386.o $(patsubst ./src/%.c, ./build_cosec/%.o, $(srcs))
objs_kosec :=        i386.o $(patsubst ./src/%.c, ./build_kosec/%.o, $(srcs))


%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

# COSEC userspace libc
libc.a: build_cosec $(objs_cosec)
	$(AR) cr $@ $(objs_cosec)

build_cosec:
	@mkdir -p build_cosec

build_cosec/%.o: ./src/%.c
	$(CC) $(CFLAGS) -DCOSEC=1 -c $< -o $@

# kernel libc, with userspace parts omitted:
libc0.a: build_kosec $(objs_kosec)
	$(AR) cr $@ $(objs_kosec)

build_kosec:
	@mkdir -p build_kosec

build_kosec/%.o: ./src/%.c
	$(CC) $(CFLAGS) -DCOSEC_KERN=1 -c $< -o $@

# linux build for testing
libc.linux.a: build_linux $(objs_linux)
	$(AR) cr $@ $(objs_linux)

build_linux:
	@mkdir -p build_linux

build_linux/%.o: ./src/%.c
	$(CC) $(CFLAGS) -DLINUX=1 -c $< -o $@

clean:
	rm -r build* libc*.a *.o || true
