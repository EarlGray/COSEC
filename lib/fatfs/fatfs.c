#include "fatfs.h"

#ifndef __packed
# define __packed __attribute__((packed))
#endif

static int read_common_bpb(struct common_bios_param_block *cpbp) {
}

#ifdef STANDALONE
int main(int argc, char **argv) {
}
#endif
