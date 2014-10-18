export top_dir		:= .
export include_dir  := $(top_dir)/include
export src_dir      := $(top_dir)
export build        := $(top_dir)/../build/usr

host_os := $(shell uname)

ifeq ($(host_os),Darwin)
gcc_home := /usr/local/gcc-4.5.2-for-linux32
crosscompile := $(gcc_home)/bin/i586-pc-linux-
endif

cc	:=$(crosscompile)gcc
ld	:=$(crosscompile)ld
ar	:=$(crosscompile)ar

cc_flags 	:= -ffreestanding -Wall -Wextra -O2 -MD
cc_includes	:= -nostdlib -nostdinc -I./include 
ld_flags := 

### for 64bit host
cc_flags 	+= -m32
as_flags	+= -m32
ld_flags	+= --oformat=elf32-i386 -melf_i386


init_objs := $(wildcard $(src_dir)/*.c)
init_objs := $(patsubst $(src_dir)/%.c, $(build)/%.o, $(init_objs))

default:  $(build)/init.a
	@echo && echo "### Userspace built..."

$(build)/%.o : $(src_dir)/%.c
	@echo -n "CCu: "
	$(cc) -c $< -o $@ $(cc_includes) $(cc_flags) -MT $(subst .d,.c,$@) 

$(build)/init.a: $(init_objs)
	@echo "ARu: "
	$(ar) crT $(build)/init.a $(init_objs)

include $(wildcard $(addprefix /*.d, $(build)/usr))
