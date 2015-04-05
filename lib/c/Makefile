CROSSCOMP =
ifeq ($(shell uname),Darwin)
CROSSCOMP = i386-elf-
endif

CC  = $(CROSSCOMP)gcc
AR  = $(CROSSCOMP)ar

CFLAGS	= -m32 -ffreestanding -nostdinc -isystem ./include

.PHONY: clean

srcs := $(wildcard ./src/*.c)
objs := $(patsubst ./src/%.c, ./build/%.o, $(srcs))

libc.a: build $(objs)
	@echo "srcs: $(srcs)"
	@echo "objs: $^"
	$(AR) crT $@ $(objs)

cosec.o: ./cosec/cosec.c
	$(CC) $(CFLAGS) -c $< -o $@

build:
	@mkdir build

./build/%.o: ./src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -r build libc.a cosec.o || true
