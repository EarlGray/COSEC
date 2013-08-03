#ifndef __FLOPPY_H___
#define __FLOPPY_H___

#define FLOPPY_DMA_AREA     ((void *)0x90000)

/* FDC base port */
#define FDC0    0x3f0
#define FDC1    0x370

void floppy_setup(void);

void floppy_reset(int fdc);

int floppy_seek(int fdc, unsigned cyli, int head);
int floppy_read_track(int fdc, index_t track);
int floppy_write_track(int fdc, index_t track);

#endif // __FLOPPY_H___
