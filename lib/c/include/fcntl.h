#ifndef __COSEC_FCNTL_H__
#define __COSEC_FCNTL_H__

#define O_RDWR      0x0001
#define O_RDONLY    0x0002
#define O_WRONLY    0x0004
#define O_APPEND    0x0008
#define O_CREAT     0x0010
#define O_TRUNC     0x0020
#define O_NOCTTY    0x0040
#define O_NOFOLLOW  0x0080

enum file_search_mode_t {
    SEEK_SET,
    SEEK_CUR,
    SEEK_END
};


#endif //__COSEC_FCNTL_H__
