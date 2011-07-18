export top_dir		:= .
export include_dir  := $(top_dir)/include
export src_dir      := $(top_dir)
export build        := $(top_dir)/build

cc  :=  gcc
as  :=  gcc
ld  :=  ld

cc_flags    := -ffreestanding -nostdinc -nostdlib -Wall -Wextra -Winline -O2 -MD 
as_flags    := -Wall -MD $(addprefix -I, $(include_dir))
ld_flags    := -static -nostdlib -Ttext=0x100000  #-s

cc_includes := -include include/globl.h $(addprefix -I, $(include_dir)) 

# for 64bit host
cc_flags 	+= -m32
as_flags	+= -m32
ld_flags	+= --oformat=elf32-i386 -melf_i386

objs		:= $(src_dir)/boot.S
objs		+= $(wildcard $(src_dir)/[^b]*.S)
objs		+= $(wildcard $(src_dir)/*.c)
objs		+= $(wildcard $(src_dir)/*/*.c)

objs		:= $(patsubst $(src_dir)/%.S, $(build)/%.o, $(objs))
objs		:= $(patsubst $(src_dir)/%.c, $(build)/%.o, $(objs))

kernel      := kernel

mnt_dir     := mnt
image       := cosec.img

log_name	:= fail.log
objdump     := $(kernel).objd
pipe_file	:= com

vbox_name   := COSEC
qemu_flags	:= -fda $(image) -boot a -m 32 -net nic,model=rtl8139  -ctrl-grab

.PHONY: run mount umount clean

#VBoxManage startvm $(vbox_name) 2>&1 | tee $(log_name);	
run:
	
	@echo "\n#### Running..." && \
	if [ $$DISPLAY ] ; then	\
		make vbox || make qemu || make bochs || \
			echo "###Error: VirtualBox, qemu or Bochs must be installed";	\
	else qemu $(qemu_flags) -curses;	fi
	
.PHONY: qemu vbox bochs

qemu:	$(image) 
	@if [ -S $(pipe_file) ]; 							\
	then qemu $(qemu_flags) -serial unix:$(pipe_file);	\
	else qemu $(qemu_flags);							\
	fi 

vbox:	$(image)
	VBoxManage startvm $(vbox_name)

bochs:	$(image)
	bochs

$(image):	$(kernel) 
	@echo "\n### Check if there is an image"
	@if [ ! -e $(image) ]; then	\
        echo -en "\n#### Generating image...\t\t"; \
		cp res/fd.img.bz2 .;	\
		bunzip2 fd.img.bz2;	\
		mv fd.img $(image);	\
		make mount && sudo cp res/menu.lst $(mnt_dir)/boot/grub/ && make umount;		\
		echo -e "[ready]\n";	\
	fi
	@if [ ! -d $(build) ]; then mkdir $(build); fi
	@make mount && [ -r $(build)/$(kernel) ] && sudo cp $(build)/$(kernel) $(mnt_dir) \
		&& echo "\n### Copied" && make umount

mount:  
	@mkdir -p $(mnt_dir)
	@sudo mount -o loop $(image) $(mnt_dir) && echo "Image mounted."

umount:
	@sudo umount $(mnt_dir) || /bin/true
	@rmdir $(mnt_dir)

$(kernel): $(build) $(objs) 
	@echo "\n### Linking..."
	@echo -n "LD: "
	$(ld) -o $(build)/$(kernel)	$(objs) $(ld_flags) 
	@if [ `which objdump 2>/dev/null` ]; then objdump -d $(build)/$(kernel) > $(objdump); fi
	@if [ `which ctags 2>/dev/null ` ]; then ctags -R *; fi
	
$(build):
	@echo "\n### Compiling..."
	@mkdir -p $(build)
	@for d in * ; do		\
		[ -d $$d ] && mkdir $(build)/$$d || true;	\
	done
	
$(build)/%.o : %.c
	@echo -n "CC: "
	$(cc) -c $< -o $@ $(cc_includes) $(cc_flags) -MT $(subst .d,.c,$@) 

$(build)/%.o : $(src_dir)/%.S
	@echo -n "AS: "
	$(as) -c $< -o $@ $(as_flags) -MT $(subst .d,.c,$@)

$(pipe_file):
	mkfifo $(pipe_file)

clean:
	rm -rf $(build)

include $(wildcard $(addprefix /*.d, $(build)))

