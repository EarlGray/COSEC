CROSSCOMP =

CC  = $(CROSSCOMP)gcc
AR  = $(CROSSCOMP)ar

CFLAGS	= -ffreestanding -nostdinc -isystem ./include

.PHONY: clean

srcs := $(wildcard ./src/*.c)
objs := $(patsubst ./src/%.c, ./build/%.o, $(srcs))

libc.a: build $(objs)
	@echo "srcs: $(srcs)"
	@echo "objs: $^"
	$(AR) crT $@ $(objs)

build:
	@mkdir build

./build/%.o: ./src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -r build libc.a
