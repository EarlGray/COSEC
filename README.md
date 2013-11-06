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
- small subset of libc, heap management; 
- preemptive multitasking demo (two example tasks running concurrently);
- simplest userspace init and syscalls (at the moment, just one actually);
- posix virtual filesystem (inodes, mountpoints, filelinks), ramfs;
Nothing more yet (no paging, no processes, no decent userland, no lot of things);

