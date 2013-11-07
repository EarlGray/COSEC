Compilation
-----------

You need just basic tools (GNU make, gcc, optionally objdump, ctags) for 
compilation on Linux (both 32 and 64 bit). Compilation is done using 

    $ make kernel

After successful build the kernel is available as `build/kernel`

Creating a floppy image: cosec.img
----------------------------------

To create a floppy image and install kernel there:

    $ make install

(this requires sudo'ing to mount the image as ext2 filesystem unless you're doing 
this with `ext2fuse`).

Root privileges: unless you have `fuseext2` installed (this is checked by Makefile),
mount/umount will be used, which requires sudo. You might wish to install `fuseext2`
to work without root privileges, but it is still recommended to generate image with 
native sudo. In order to do use native `mount` when `ext2fuse` is installed, use

    $ make fuse=

To make boot image manually, use res/fd.tar.bz2 (floppy image with preinstalled 
GRUB, copy res/menu.lst to ${floppy.img}/boot/grub/, put ${kernel} to ${floppy}/.
For GRUB: use `multiboot` command or `kernel --type=multiboot (fd0)/kernel` before 
`boot`!

Creating a LiveCD: cosec.iso
----------------------------

If you're on Linux, you can use `res/mkcd` to create a CD image with `grub-mkrescue`/
`xorriso`:

    $ make kernel
    $ res/mkcd

Now `cosec.iso` will be available.

Running
-------
To run it,  you need QEMU or Bochs, VirtualBox (for the latter create VM named 
COSEC with `cosec.img` attached) installed.  Default action of Makefile is 
compiling kernel, creating a floppy image and installing kernel there, then running
one of the VMs if found, so you just need to do 

    $ make

To run using QEMU:

    # without creating any image;
    $ qemu-system-i386 -kernel build/kernel

    # from floppy image:
    $ qemu-system-i386 -fda cosec.img

    # from CD image:
    $ qemu-system-i386 -cdrom cosec.iso

    # run without GUI:
    $ qemu-system-i386 -curses -cdrom cosec.iso

To run using VirtualBox: create a virtual machine named COSEC with cosec.img 
attached as a floppy image, then:

    $ make vbox

The same for Bochs:
    
    $ make bochs

Compilation and running on OS X
-------------------------------

OS X requires a GCC cross-compiler for i386 Linux, which can be found here:
http://crossgcc.rts-software.org/doku.php?id=compiling_for_linux

After installation, the toolchain will be available in `/usr/local/gcc-4.5.2-for-linux32/`
Then just use `make kernel`.

Creating an image is hard on OS X (there's no ext2 tools, no GRUB tools), so the 
only option is:

    $ sudo port install qemu                    # or brew/fink
    $ qemu-system-i386 -kernel build/kernel


View 'make help' output for a brief information.

Please report issues to dmytrish@gmail.com 
