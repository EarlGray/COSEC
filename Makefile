include_dir     := ./include
src_dir         := .
build           := .

cc  :=  gcc
as  :=  gcc
ld  :=  ld

cc_flags    := -c -ffreestanding -nostdinc -Wall -Wextra -Winline -MD -O2 #-fdump-tree-gimple
as_flags    := -c -Wall -MD 
ld_flags    := -static -nostdlib -Ttext=0x110000  #-s

# for 64bit host
cc_flags 	+= -m32
as_flags	+= -m32
ld_flags	+= --oformat=elf32-i386 -melf_i386

objs        := $(addsuffix /sluice.S, $(build))                                      # must come first
objs		+= $(patsubst ../%, %, $(wildcard $(addsuffix /[^s]*.S, $(build))))		 # no s*.S files to make sliuce.S first :(
objs		+= $(patsubst ../%, %, $(wildcard $(addsuffix /*.c, $(build))))

objs		:= $(objs:.S=.o)
objs		:= $(objs:.c=.o)

kernel      := kernel

mnt_dir     := mnt
vbox_name   := COSEC
image       := cosec.img

log_name	:= fail.log
objdump     := $(kernel).objd

run:	$(image)
	@echo -e "\n#### Running..."
	@if [ `which NO_VBoxManage 2>/dev/null` ]; then 	\
		if [ -z "`VBoxManage list vms | grep $(vbox_name)`" ]; then	\
			VBoxManage createvm --name $(vbox_name) --register;	\
			VBoxManage modifyvm $(vbox_name)			\
				--memory 32 --floppy `pwd`/$(image);	\
		fi; 					\
		VBoxManage startvm $(vbox_name) 2>&1 | tee $(log_name);	\
		rm -f 2011*;	\
	else \
	if [ `which NO_bochs 2>/dev/null` ]; then 	\
		bochs 2>&1 | tee $(log_name);	\
	else \
	if [ `which qemu 2>/dev/null` ]; then	\
		qemu -fda $(image) -boot a;	\
	else echo "Error: qemu or VirtualBox must be installed";\
	fi; fi; fi

vboxrun:	$(image)
	

$(image):	$(kernel) 
	### Check if there is an image
	@if [ ! -e $(image) ]; then	\
        echo -en "\n#### Generating image...\t\t"; \
		cp res/fd.img.bz2 .;	\
		bunzip2 fd.img.bz2;	\
		mv fd.img $(image);	\
		make mount && sudo cp res/menu.lst $(mnt_dir)/boot/grub/ && make umount;		\
		echo -e "[ready]\n";	\
	fi
	@if [ ! -d $(build) ]; then mkdir $(build); fi
	make mount && sudo cp $(build)/$(kernel) $(mnt_dir) && make umount

mount:  
	@mkdir -p $(mnt_dir)
	@sudo mount -o loop $(image) $(mnt_dir)
	@echo "Image mounted."

umount:
	@sudo umount $(mnt_dir) || /bin/true
	@rmdir $(mnt_dir)
	
VPATH	:= 	../

$(kernel): $(build) $(objs)
	$(ld) -o $(build)/$(kernel)	$(objs) $(ld_flags) && \
	if [ `which objdump 2>/dev/null` ]; then objdump -d $(kernel) > $(objdump); fi
	if [ `which ctags 2>/dev/null ` ]; then ctags * -R; fi
	
$(build):
	mkdir -p $(build)
	
%.o : %.c
	$(cc) $< -o $(build)/$@ $(cc_flags) $(addprefix -I, $(include_dir))

%.o : %.S
	$(as) $< -o $(build)/$@ $(as_flags) $(addprefix -I, $(include_dir))

.PHONY: clean
clean:
	rm -rf $(build)/*.[od]
	#rm -rf $(build)/$(kernel)

include $(wildcard $(addprefix /*.d, $(build)))

