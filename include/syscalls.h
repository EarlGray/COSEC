#ifndef __SYSCALLS_H__
#define __SYSCALLS_H__

extern int sys_mount(const char *source, const char *target,
          const char *fstype, uint mountflags,
          const void *data);

#endif //__SYSCALLS_H__
