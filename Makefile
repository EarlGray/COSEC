export top_dir		:= .
export include_dir  := $(top_dir)/include
export src_dir      := $(top_dir)
export build        := $(top_dir)/build

cc  :=  gcc
as  :=  gcc
ld  :=  ld

cc_flags    := -ffreestanding -nostdinc -nostdlib -Wall -Wextra -Winline -O2 -MD 
as_flags    := -Wall -MD $(addprefix -I, $(include_dir))
ld_flags    := -static -nostdlib -Tlink.ld

cc_includes := -include include/globl.h $(addprefix -I, $(include_dir)) 

### for 64bit host
cc_flags 	+= -m32
as_flags	+= -m32
ld_flags	+= --oformat=elf32-i386 -melf_i386

objs		:= $(src_dir)/arch/boot.S
objs		+= $(wildcard $(src_dir)/arch/[^b]*.S)	# exclude boot.S
objs		+= $(wildcard $(src_dir)/[^u]*/*.c)		# exclude usr/ folder
objs		+= $(wildcard $(src_dir)/*.c)			

objs		:= $(patsubst $(src_dir)/%.S, $(build)/%.o, $(objs))
objs		:= $(patsubst $(src_dir)/%.c, $(build)/%.o, $(objs))

libinit		:= $(build)/usr/init.a
kernel      := kernel

mnt_dir     := mount_point
image       := cosec.img

fuse		:=
#$(shell which fuseext2)

### Use native mount/umount for a GRUB installation, fuseext2 fails at this
ifeq ($(strip $(fuse)),)
    do_mount	:= sudo mount -o loop 
    do_umount	:= sudo umount 
    do_install	:= sudo cp 
else
    do_mount	:= $(fuse) -o rw+,uid=`id -u`,gid=`id -g` 
    do_umount	:= fusermount -u 
    do_install	:= cp
endif

log_name	:= fail.log
objdump     := $(build)/$(kernel).objd
pipe_file	:= pipe

vbox_name   := COSEC
qemu_flags	:= -fda $(image) -boot a -m 64 -net nic,model=rtl8139 -serial stdio

.PHONY: run install mount umount clean
.PHONY: qemu vbox bochs

run:	install
	@echo "\n#### Running..." && \
	if [ $$DISPLAY ] ; then	\
		make vbox || make qemu || make bochs || \
			echo "@@@@ Error: VirtualBox, qemu or Bochs must be installed";	\
	else qemu $(qemu_flags) -curses;	fi

qemu:	install 
	@if [ -S $(pipe_file) ]; 							\
	then qemu $(qemu_flags) -serial unix:$(pipe_file) ;	\
	else qemu $(qemu_flags) ;							\
	fi 

vbox:	install
	VBoxManage startvm $(vbox_name)

bochs:	install
	bochs


install:  $(kernel) 
	@make mount \
	&& $(do_install) $(build)/$(kernel) $(mnt_dir) \
	&& echo "\n## Kernel installed"; make umount

$(mnt_dir):	
	@mkdir $(mnt_dir) 

mount:	$(image) $(mnt_dir)
	@$(do_mount) $(image) $(mnt_dir) \
	&& echo "## Image mounted"

umount:	
	@$(do_umount) $(mnt_dir) &&	echo "## Image unmounted" || true; \
	rmdir $(mnt_dir)

$(image):	 
	@echo "\n#### Checking image"
	@if [ ! -e $(image) ]; then	\
        echo -en "\n## Generating image...\t\t"; \
		cp res/fd.img.bz2 .;	\
		bunzip2 fd.img.bz2;	\
		mv fd.img $(image);	\
		make mount \
			&& mkdir -p $(mnt_dir)/boot/grub/ \
			&& $(do_install) res/menu.lst $(mnt_dir)/boot/grub \
			&& make umount;	\
		echo -e "## ...generated\n";	\
	fi

$(kernel): $(build) $(objs) $(libinit)
	@echo "\n#### Linking..."
	@echo -n "LD: "
	$(ld) -o $(build)/$(kernel)	$(objs) $(libinit) $(ld_flags) && echo "## ...linked"
	@if [ `which objdump 2>/dev/null` ]; then objdump -d $(build)/$(kernel) > $(objdump); fi
	@if [ `which nm 2>/dev/null` ]; then nm $(build)/$(kernel) | sort > $(build)/$(kernel).nm; fi
	@if [ `which ctags 2>/dev/null ` ]; then ctags -R *; fi
	
$(build):
	@echo "\n#### Compiling"
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

$(libinit):
	@cd usr && make

$(pipe_file):
	mkfifo $(pipe_file)

clean:
	rm -rf $(build)

help:
	@echo "USAGE:"; \
	echo "	[run] - (default action) compile sources, link, generate image if needed, install kernel, search a virtual machine, run"; \
	echo "	qemu | vbox | bochs - make all the things needed for run and run in the specified emulator"; \
	echo "	install - check kernel and image and install former to latter";	\
	echo "	kernel - compile and link kernel"; \
	echo "	mount/umount - mount/umount image (root privileges are required unless using FUSE)";	\
	echo "  You may wish to install fuseext2 tools to work without root privileges, but it is still recommended"; \
	echo "  to make image with native sudo. In order to do this, use"; \
	echo "	make fuse=";	echo

include $(wildcard $(addprefix /*.d, $(build)))

