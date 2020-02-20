export top_dir      := .
export include_dir  := $(top_dir)/include
export src_dir      := $(top_dir)/src
export build        := $(top_dir)/build

STDINC_DIR   := lib/c/include
CACHE_DIR    ?= $(top_dir)/.cache

LUA          := 1
#SECD		 := 1

host_os := $(shell uname)

ifeq ($(host_os),Darwin)
crosscompile ?= /usr/local/gcc-4.8.1-for-linux64/bin/x86_64-pc-linux-
endif

cc      ?= $(crosscompile)gcc
as      ?= $(crosscompile)gcc
ld      ?= $(crosscompile)ld
ar      ?= $(crosscompile)ar
ranlib  ?= $(crosscompile)ranlib

nm      := $(crosscompile)nm
objdump := $(crosscompile)objdump


objs    := $(src_dir)/arch/boot.S
objs    += $(wildcard $(src_dir)/arch/[^b]*.S) # exclude boot.S
objs    += $(wildcard $(src_dir)/*/*.c)

objs    := $(patsubst $(src_dir)/%.S, $(build)/%.o, $(objs))
objs    := $(patsubst $(src_dir)/%.c, $(build)/%.o, $(objs))

lds     := vmcosec.lds

cc_includes := $(addprefix -I, $(include_dir) $(STDINC_DIR))

cc_flags    := -ffreestanding -nostdinc -fno-stack-protector -Wall -Wextra -Wno-inline -Wno-implicit-fallthrough -Wno-unused -O2 -MD -DCOSEC=1
as_flags    := -nostdinc -Wall -MD $(cc_includes)
ld_flags    := -static -nostdlib -T$(build)/$(lds)

ifneq ($(LUA),)
liblua      := lib/liblua.a
cc_flags    += -DCOSEC_LUA=1
endif
ifneq ($(SECD),)
libsecd		:= $(build)/libsecd.o
secdsrc		:= build/libsecd.c
cc_flags	+= -DCOSEC_SECD=1
endif

### for 64bit host
cc_flags  += -m32
as_flags  += -m32
ld_flags  += -melf_i386

libc      := lib/c/libc.a
kernel    := $(build)/kernel
initfs    := res/initfs

mnt_img     := bootfd
image       := cosec.img
cd_img      := cosec.iso

fuse    := $(shell which ext2fuse 2>/dev/null)

### Use native mount/umount for a GRUB installation, fuseext2 fails at this
ifeq ($(strip $(fuse)),)
    do_mount    := sudo mount -o loop
    do_umount   := sudo umount -l
    do_install  := sudo cp
else
    do_mount    := $(fuse) -o rw+,uid=`id -u`,gid=`id -g`
    do_umount   := fusermount -u
    do_install  := cp
endif

log_name      := fail.log
objdfile      := $(kernel).objd
pipe_file     := pipe

vbox_name   := COSEC

qemu        := qemu-system-i386
qemu_cdboot := -cdrom $(cd_img) -boot d
qemu_mltboot:= -kernel ../$(kernel) -initrd init
qemu_debug  := -D tmp/qemu.log -d int,unimp,guest_errors
#qemu_debug  += -trace virtio_notify,file=tmp/qemu.trace
#qemu_debug  += -trace virtio_queue_notify,file=tmp/qemu.trace
#qemu_debug  += -trace virtio_set_status,file=tmp/qemu.trace

ifneq ($(QEMU_TAP),)
qemu_net    := -netdev tap,ifname=$(QEMU_TAP),id=tap0,script=no,downscript=no
qemu_net    += -device virtio-net-pci,netdev=tap0
#qemu_debug  += -object filter-dump,id=tap0,netdev=tap0,file=qemu.pcap
else
qemu_net    := -netdev user,id=usr0
qemu_net    += -device virtio-net-pci,netdev=usr0
#qemu_debug  += -object filter-dump,id=usr0,netdev=usr0,file=tmp/qemu.pcap
endif

qemu_flags  := -m 64 -serial stdio $(qemu_debug)

init        := usr/init

.PHONY: run install mount umount clean
.PHONY: qemu vbox bochs runq

qemu: $(cd_img)
	$(qemu) $(qemu_cdboot) $(qemu_flags) $(qemu_net)

runq: $(kernel) $(init)
	cd usr && $(qemu) $(qemu_flags) $(qemu_mltboot)

run: install
	@echo "\n#### Running..." && \
	if [ $$DISPLAY ] ; then      \
	    make krun || make qemu || make vbox || make bochs || \
	        echo "@@@@ Error: VirtualBox, qemu or Bochs must be installed"; \
	else $(qemu) $(qemu_cdboot) $(qemu_flags) -curses; fi

vbox: install
	VBoxManage startvm $(vbox_name)

bochs: $(cd_img)
	bochs -qf res/.bochsrc-template

$(init):
	make -C $(dir $(init))

$(cd_img): $(kernel)
	@echo "Creating a LiveCD..."
	@res/mkcd

install:  $(kernel) $(initfs)
	@make mount \
	    && $(do_install) $(kernel) $(mnt_img)   \
	    && $(do_install) $(kernel).nm $(mnt_img)   \
	    && echo "\n## Kernel installed";        \
	$(do_install) $(initfs) $(mnt_img)          \
	    && echo "## Initfs installed";          \
	make umount

$(initfs):
	@res/mkinitfs $(initfs) && echo "## initfs created"

$(mnt_img):
	@mkdir $(mnt_img)

mount: $(image) $(mnt_img)
	@$(do_mount) $(image) $(mnt_img) \
	&& echo "## Image mounted"

umount:
	@$(do_umount) $(mnt_img) && echo "## Image unmounted" || true; \
	rmdir $(mnt_img)

$(image):
	@echo "\n#### Checking image"
	@if [ ! -e $(image) ]; then \
	    echo -en "\n## Generating image...\t\t"; \
	    cp res/fd.img.bz2 .; \
	    bunzip2 fd.img.bz2; \
	    mv fd.img $(image); \
	    make mount \
	        && mkdir -p $(mnt_img)/boot/grub/ \
	        && $(do_install) res/menu.lst $(mnt_img)/boot/grub \
	        && make umount; \
	    echo -e "## ...generated\n"; \
	fi

$(kernel): $(libc) $(build) $(liblua) $(libsecd) $(objs) $(build)/$(lds)
	@echo "\n#### Linking..."
	$(ld) -o $(kernel) $(objs) $(libsecd) $(liblua) $(libc) $(ld_flags)
	@echo "## ...linked"
	@[ `which $(objdump) 2>/dev/null` ] && $(objdump) -d $(kernel) > $(objdfile) || true
	@[ `which $(nm) 2>/dev/null` ] && $(nm) $(kernel) | sort > $(kernel).nm || true
	@[ `which ctags 2>/dev/null ` ] && ctags -R * || true

$(libc):
	@make CROSSCOMP=$(crosscompile) -C lib/c

$(build):
	@echo "\n#### Compiling"
	@mkdir -p $(build)
	@for d in $(src_dir)/* ; do \
	    [ -d $$d ] && mkdir $(build)/$${d#$(src_dir)/} || true; \
	done

$(build)/$(lds):    $(src_dir)/$(lds).S
	$(cc) -E $< -o $@ -P -DNOT_CC $(cc_includes)

$(build)/%.o : $(src_dir)/%.c
	$(cc) -c $< -o $@ $(cc_includes) $(cc_flags) -MT $(subst .d,.c,$@)

$(build)/%.o : $(src_dir)/%.S
	$(as) -c $< -o $@ $(as_flags) -MT $(subst .d,.c,$@)

ifneq ($(LUA),)

LUA_VER  ?= 5.2.2
LUA_DIR  := lib/lua-$(LUA_VER)

# include/lua/lua.h: $(LUA_DIR)

$(CACHE_DIR)/lua-$(LUA_VER).tar.gz:
	mkdir -p $(CACHE_DIR) ; cd $(CACHE_DIR); \
    curl -LR -O http://www.lua.org/ftp/$(notdir $@)

$(LUA_DIR): $(CACHE_DIR)/lua-$(LUA_VER).tar.gz
	tar xf $< -C lib/
	cd include ; test -L lua || ln -s ../$(LUA_DIR)/src lua

$(liblua): $(LUA_DIR)
	TOP_DIR=`pwd` && cd $(LUA_DIR)/src \
	&& make CC=$(cc) RANLIB=$(ranlib) AR='$(ar) rcu --target elf32-i386' \
	        SYSCFLAGS="-m32 -nostdinc -fno-stack-protector -I$$TOP_DIR/$(STDINC_DIR)" liblua.a
	mv $(LUA_DIR)/src/liblua.a $(top_dir)/$@

endif
clean_lua:
	rm -rf include/lua lib/liblua.a $(LUA_DIR) || true

ifneq ($(SECD),)
SECD_VER 	:= 0.1.2
SECD_TARGZ	:= $(CACHE_DIR)/secd-$(SECD_VER).tar.gz

$(libsecd): $(secdsrc)
	$(cc) -c $< -o $@ $(cc_includes) $(cc_flags)

$(secdsrc): lib/secd/libsecd.c include/secd
	cp lib/secd/libsecd.c $@

include/secd: lib/secd lib/secd/repl.secd
	mkdir -p $@
	cp -r lib/secd/include/secd/* $@/
	xxd -i lib/secd/repl.secd > $@/repl.inc

lib/secd/libsecd.c: lib/secd
	make -C $^ libsecd.c

lib/secd/repl.secd:
	make -C lib/secd repl.secd

lib/secd: $(SECD_TARGZ)
	tar -C lib/ -xf $(SECD_TARGZ)
	mv lib/SECD-$(SECD_VER) lib/secd

$(SECD_TARGZ):
	mkdir -p $(CACHE_DIR)
	curl -R -L -o $@ https://github.com/EarlGray/SECD/archive/$(SECD_VER).tar.gz

endif
clean_secd:
	rm -r $(secdsrc) include/secd || true
	make -C lib/secd clean || true

$(pipe_file):
	mkfifo $(pipe_file)

clean_kern:
	rm -r $(secdsrc) include/secd || true
	rm -rf $(build) || true

clean: clean_kern
	make -C lib/c clean || true
	make -C usr/ clean || true

distclean: clean clean_lua clean_secd

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

include $(wildcard $(addprefix /*.d, $(build)/arch $(build)/core $(build)/dev $(build)/fs $(build)/mem))
