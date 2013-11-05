#ifndef __SECD_MACHINE_H__
#define __SECD_MACHINE_H__

#define errorf(...) loge(__VA_ARGS__)
#define assert_or_continue(cond, ...) \
    if (!(cond)) { errorf(__VA_ARGS__); loge("\n"); continue; }
#define assert(cond, ...) \
    if (!(cond)) { errorf(__VA_ARGS__); loge("\n"); return NULL; }
#define asserti(cond, ...) \
    if (!(cond)) { errorf(__VA_ARGS__); loge("\n"); return 0; }
#define assertv(cond, ...) \
    if (!(cond)) { errorf(__VA_ARGS__); loge("\n"); return; }

#define MEMDEBUG    0
#define MEMTRACE    0
#define CTRLDEBUG   0
#define ENVDEBUG    0

#define TAILRECURSION 1

#ifdef CASESENSITIVE
# define str_eq(s1, s2)  !strcmp(s1, s2)
# define str_cmp(s1, s2) strcmp(s1, s2)
#else
# define str_eq(s1, s2) !strcasecmp(s1, s2)
# define str_cmp(s1, s2) strcasecmp(s1, s2)
#endif

#if (MEMDEBUG)
# define memdebugf(...) printf(__VA_ARGS__)
# if (MEMTRACE)
#  define memtracef(...) printf(__VA_ARGS__)
# else
#  define memtracef(...)
# endif
#else
# define memdebugf(...)
# define memtracef(...)
#endif

#if (CTRLDEBUG)
# define ctrldebugf(...) printf(__VA_ARGS__)
#else
# define ctrldebugf(...)
#endif

#if (ENVDEBUG)
# define envdebugf(...) printf(__VA_ARGS__)
#else
# define envdebugf(...)
#endif

#ifndef __unused
# define __unused __attribute__((unused))
#endif


#define EOF_OBJ     "#<eof>"

#define DONT_FREE_THIS  INTPTR_MAX/2

#define N_CELLS     256 * 1024
#define SECD_ALIGN  4

typedef enum { false, true } bool;

typedef  struct secd    secd_t;
typedef  struct cell    cell_t;
typedef  long  index_t;

typedef  struct atom  atom_t;
typedef  struct cons  cons_t;
typedef  struct error error_t;

typedef cell_t* (*secd_opfunc_t)(secd_t *);
typedef cell_t* (*secd_nativefunc_t)(secd_t *, cell_t *);

enum cell_type {
    CELL_UNDEF,
    CELL_ATOM,
    CELL_CONS,
    CELL_ERROR,
};

enum atom_type {
    NOT_AN_ATOM,
    ATOM_INT,
    ATOM_SYM,
    ATOM_FUNC,
};

struct atom {
    enum atom_type type;
    union {
        int num;
        struct {
            size_t size;
            const char *data;
        } sym;

        struct {
            secd_opfunc_t fun;
            cell_t *sym;
        } op;

        void *ptr;
    } as;
};

struct cons {
    cell_t *car;    // shares
    cell_t *cdr;    // shares
};

struct error {
    size_t len;
    const char *msg; // owns
};

struct cell {
    // this is a packed structure:
    //      bits 0 .. SECD_ALIGN-1          - enum cell_type
    //      bits SECD_ALIGN .. CHAR_BIT * (sizeof(intptr_t)-1)   - (secd_t *)
    intptr_t type;
    union {
        atom_t  atom;
        cons_t  cons;
        error_t err;
    } as;

    size_t nref;
};

// must be aligned at 1<<SECD_ALIGN
struct secd  {
    cell_t *stack;      // list
    cell_t *env;        // list
    cell_t *control;    // list
    cell_t *dump;       // list

    cell_t *free;       // list
    cell_t *data;       // array
    cell_t *nil;        // pointer

    cell_t *global_env;

    size_t used_stack;
    size_t free_cells;
};


#endif
