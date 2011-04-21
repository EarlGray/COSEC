include_dir     := ./include
src_dir         := .
build           := .

cc  :=  gcc
as  :=  gcc
ld  :=  ld
cc_flags    := -c -ffreestanding -nostdlib -nostdinc -Wall -Wextra -Winline -MD -m32 -O2
as_flags    := -c -nostdlib -nostdinc -Wall -MD -m32
ld_flags    := -static -nostdlib -Ttext=0x110000 --oformat=elf32-i386 -melf_i386 #-s

objs        := $(addsuffix /sluice.S, $(build))                                      # must come first
objs		+= $(patsubst ../%, %, $(wildcard $(addsuffix /[^s]*.S, $(build))))		 # no s*.S files to make sliuce.S first :(
objs		+= $(patsubst ../%, %, $(wildcard $(addsuffix /*.c, $(build))))

objs		:= $(objs:.S=.o)
objs		:= $(objs:.c=.o)

kernel      := kernel
mnt_dir     := mnt
image       := cosec.img
vbox_name   := COSEC

objdump     := $(kernel).objd

run:	$(image)
	@echo -e "\n#### Running..."
	@if [ `which VirtualBox 2>/dev/null` ]; then 	\
		if [ -z "`VBoxManage list vms | grep $(vbox_name)`" ]; then	\
			VBoxManage createvm --name $(vbox_name) --register;	\
			VBoxManage modifyvm $(vbox_name)			\
				--memory 32 --floppy `pwd`/$(image);	\
		fi; 					\
		VirtualBox --startvm $(vbox_name);	\
	else if [ `which qemu 2>/dev/null` ]; then	\
		qemu -fda $(image) -curses -boot a;	\
	else echo "Error: qemu or VirtualBox must be installed";\
	fi; fi

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
	$(ld) -o $(build)/$(kernel)	$(objs) $(ld_flags) && objdump -d $(kernel) > $(objdump)
	ctags * -R
	
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

