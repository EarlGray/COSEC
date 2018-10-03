#if COSEC_SECD

#include <secd/secd.h>
#include <secd/secd_io.h>

#include <cosec/log.h>

#include <stdlib.h>
#include <string.h>

#define N_CELLS   (64 * 1024)

void kshell_secd(void *cmd, const char *arg) {
    if (exitpoint() != EXITENV_EXITPOINT) {
        logmsge("SECD exited");
        return;
    }

    secd_t secd;
    size_t heapsize = sizeof(cell_t) * N_CELLS;
    cell_t *heap = (cell_t *)malloc(heapsize);
    memset(heap, 0, heapsize);
    logmsgf("secd: heap at %x\n", (size_t)heap);

    init_secd(&secd, heap, N_CELLS);
    logmsgf("secd: init_secd(*0x%x)\n", (size_t)&secd);

    cell_t *cmdport = SECD_NIL;
    cmdport = secd_fopen(&secd, arg[0] ? arg : "/repl.secd", "r");
    logmsgf("secd: secd_fopen()\n");

    cell_t *inp = sexp_parse(&secd, cmdport); // cmdport is dropped after
    logmsgf("secd: sexp_parse()\n");
    if (is_nil(inp) || !is_cons(inp)) {
        secd_errorf(&secd, "list of commands expected\n");
        dbg_printc(&secd, inp);
        goto heapfree;
    }

    run_secd(&secd, inp);

heapfree:
    kfree(heap);
}

#endif // COSEC_SECD
