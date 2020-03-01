export top_dir      := $(shell pwd)
export include_dir  := $(top_dir)/include
export src_dir      := $(top_dir)/src
export build        := $(top_dir)/build

STDINC_DIR   := $(top_dir)/lib/c/include
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

cc_includes := -isystem $(STDINC_DIR) $(addprefix -I, $(include_dir))

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

libc      := lib/libc.a
kernel    := $(build)/kernel

cd_img      := cosec.iso

fuse    := $(shell which ext2fuse 2>/dev/null)

objdfile      := $(kernel).objd

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
qemu_debug  += -object filter-dump,id=usr0,netdev=usr0,file=tmp/qemu.pcap
endif

qemu_flags  := -m 64 -serial stdio $(qemu_debug) $(QEMU_OPT)

init        := usr/init

.PHONY: run install mount umount clean
.PHONY: qemu runq default

default: qemu

include $(wildcard lib/*.mk)

qemu: $(cd_img)
	$(qemu) $(qemu_cdboot) $(qemu_flags) $(qemu_net)

runq: $(kernel) $(init)
	cd usr && $(qemu) $(qemu_flags) $(qemu_mltboot)

run: install
	@echo "\n#### Running..." && \
	if [ $$DISPLAY ] ; then      \
	    make krun || make qemu || \
	        echo "@@@@ Error: VirtualBox, qemu or Bochs must be installed"; \
	else $(qemu) $(qemu_cdboot) $(qemu_flags) -curses; fi

$(init):
	make -C $(dir $(init))

$(cd_img): $(kernel) $(init)
	@echo "Creating a LiveCD..."
	@res/mkcd

$(kernel): $(libc) $(liblua) $(libsecd) $(build) $(objs) $(build)/$(lds)
	@echo "\n#### Linking..."
	$(ld) -o $(kernel) $(objs) $(libsecd) $(liblua) $(libc) $(ld_flags)
	@echo "## ...linked"
	@[ `which $(objdump) 2>/dev/null` ] && $(objdump) -d $(kernel) > $(objdfile) || true
	@[ `which $(nm) 2>/dev/null` ] && $(nm) $(kernel) | sort > $(kernel).nm || true
	@[ `which ctags 2>/dev/null ` ] && ctags -R * || true

$(libc):
	@make CROSSCOMP=$(crosscompile) -C lib/c libc.a
	@rm lib/libc.a 2>/dev/null; ln lib/c/libc.a lib/libc.a

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

clean_kern:
	rm -r $(secdsrc) include/secd || true
	rm -rf $(build) || true

clean: clean_kern
	make -C lib/c clean || true
	make -C usr/ clean || true

distclean: clean clean_lua clean_secd

help:
	@echo "USAGE:"; \
	echo "  [run] - (default action) compile sources, link, generate image if needed, install kernel, search a virtual machine, run"; \
	echo "  qemu - make all the things needed for run and run in the specified emulator"; \
	echo "  $(cd_img) - make a live iso;" \
	echo "  $(kernel) - compile and link kernel"; \

include $(wildcard $(addprefix /*.d, $(build)/arch $(build)/core $(build)/dev $(build)/fs $(build)/mem))
