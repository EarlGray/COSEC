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
vbox_name   := COSEC
image       := cosec.img

log_name	:= fail.log
objdump     := $(kernel).objd

.PHONY: run mount umount clean

#VBoxManage startvm $(vbox_name) 2>&1 | tee $(log_name);	
run:	$(image)
	@echo "\n#### Running..."
	@if [ `which NObochs 2>/dev/null` ]; then 	\
		bochs 2>&1 | tee $(log_name);	\
	else \
	if [ `which VBoxManage 2>/dev/null` ]; then 	\
		VBoxManage startvm $(vbox_name);	\
		rm -f 2011*;	\
	else \
	if [ `which qemu 2>/dev/null` ]; then	\
		qemu -fda $(image) -boot a -m 32;	\
	else \
		echo "Error: qemu or VirtualBox must be installed";\
	fi; fi; fi

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
	@make mount
	@if [ -r $(build)/$(kernel) ]; then from=$(build)/$(kernel);	\
	else if [ -r $(kernel) ]; then from=$(kernel); fi; fi;	\
	sudo cp $$from $(mnt_dir) && echo "\n### Copied"
	@make umount

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
	@if [ `which ctags 2>/dev/null ` ]; then ctags * -R; fi
	
$(build):
	@echo "\n### Compiling..."
	@mkdir -p $(build)
	@for d in * ; do		\
		[ -d $$d ] && mkdir $(build)/$$d || /bin/true;	\
	done
	
$(build)/%.o : %.c
	@echo -n "CC: "
	$(cc) -c $< -o $@ $(cc_includes) $(cc_flags) -MT $(subst .d,.c,$@) 

$(build)/%.o : $(src_dir)/%.S
	@echo -n "AS: "
	$(as) -c $< -o $@ $(as_flags) -MT $(subst .d,.c,$@)

clean:
	rm -rf $(build)

include $(wildcard $(addprefix /*.d, $(build)))

