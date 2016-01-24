the Coolest Operating System Ever Created
-----------------------------------------

I'm kidding :)
Seriously, it's just my first effort in writing OS for protected mode of x86.
It is quite simple and buggy, you won't see anything great for a long time.

Tools, languages and platform used:
- gcc 4.5, GNU make;
- written in C with gas assembly;
- aimed for i386 CPUs;
- loaded with GRUB, multiboot specification;

Features:
- a simplest kernel shell with few built-in commands primarily for debugging;
- built-in Lua shell with `sys` module;
- a subset of libc, heap management;
- preemptive multitasking demo (two example tasks running concurrently);
- simplest userspace init and syscalls;
- posix virtual filesystem (inodes, mountpoints, filelinks), ramfs;
- processes are work in progress;

Nothing more yet (no paging, no decent userland, no lot of things);


Try it!
-------

You can find a fresh image at https://dmytrish.net/d/cosec.iso

Run:

- using Qemu: `qemu-system-i386 -cdrom cosec.iso`;
- using http://copy.sh/v86 : choose `cosec.iso` as "CD image" and start emulation;
- set it as a livecd in a x86 virtual machine of your choice.

Resources
---------
- a small wiki: https://github.com/EarlGray/COSEC/wiki ;
- a gitter chat, if you have any questions or would like to contact me: https://gitter.im/EarlGray/COSEC ;
