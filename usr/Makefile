export top_dir		:= .
export include_dir  := $(top_dir)/include
export src_dir      := $(top_dir)
export build        := $(top_dir)/../build/usr

cc	:=gcc
ld	:=ld
ar	:=ar

cc_flags 	:= -Wall -Wextra -O2 -MD
cc_includes	:= -nostdinc -I./include 
ld_flags := 

### for 64bit host
cc_flags 	+= -m32
as_flags	+= -m32
ld_flags	+= --oformat=elf32-i386 -melf_i386


init_objs := $(wildcard $(src_dir)/*.c)
init_objs := $(patsubst $(src_dir)/%.c, $(build)/%.o, $(init_objs))

default:  $(build)/init.a
	@echo && echo "### Building userspace..."

$(build)/%.o : $(src_dir)/%.c
	$(cc) -c $< -o $@ $(cc_includes) $(cc_flags) -MT $(subst .d,.c,$@) 

libc.a:

$(build)/init.a: $(init_objs)	
	$(ar) crT $(build)/init.a $(init_objs)

include $(wildcard $(addprefix /*.d, $(build)/usr))
