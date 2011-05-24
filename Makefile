include_dir     := ./include
src_dir         := .
build           := ./build

cc  :=  gcc
as  :=  gcc
ld  :=  ld

cc_flags    := -c -ffreestanding -nostdinc -nostdlib -Wall -Wextra -Winline -MD -O2 #-fdump-tree-gimple
as_flags    := -c -Wall -MD 
ld_flags    := -static -nostdlib -Ttext=0x110000  #-s

# for 64bit host
cc_flags 	+= -m32
as_flags	+= -m32
ld_flags	+= --oformat=elf32-i386 -melf_i386

objs		:= $(src_dir)/sluice.S
objs		+= $(wildcard $(src_dir)/[^s]*.S)
objs		+= $(wildcard $(src_dir)/*.c)

objs		:= $(patsubst $(src_dir)/%.S, $(build)/%.o, $(objs))
objs		:= $(patsubst $(src_dir)/%.c, $(build)/%.o, $(objs))

kernel      := kernel

mnt_dir     := mnt
vbox_name   := COSEC
image       := cosec.img

log_name	:= fail.log
objdump     := $(kernel).objd

.PHONY: run mount umount clean

run:	$(image)
	@echo "\n#### Running..."
	@if [ `which NO_bochs 2>/dev/null` ]; then 	\
		bochs 2>&1 | tee $(log_name);	\
	else \
	if [ `which qemu 2>/dev/null` ]; then	\
		qemu -fda $(image) -boot a;	\
	else \
	if [ `which VBoxManage 2>/dev/null` ]; then 	\
		VBoxManage startvm $(vbox_name) 2>&1 | tee $(log_name);	\
		rm -f 2011*;	\
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
	@echo $(objs)
	mkdir -p $(build)
	
$(build)/%.o : $(src_dir)/%.c
	@echo -n "CC: "
	$(cc) $< -o $@ $(cc_flags) -MT $(build)/$@ $(addprefix -I, $(include_dir))

$(build)/%.o : $(src_dir)/%.S
	@echo -n "AS: "
	$(as) $< -o $@ -MT $(build)/$@ $(as_flags) $(addprefix -I, $(include_dir))

clean:
	rm -rf $(build)/*.[od]
	mv $(build)/$(kernel) $(kernel)
	rmdir $(build)

include $(wildcard $(addprefix /*.d, $(build)))

