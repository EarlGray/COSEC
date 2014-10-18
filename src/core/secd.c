#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/ctype.h>

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

#if COSEC
typedef ptr_t intptr_t;
typedef struct DATA DATA;
int dgetc(DATA *d);

# include <log.h>
# undef assert
# undef asserti
# undef assertv
# define NULL null
#else
typedef enum { false, true } bool;
typedef  long  index_t;
#endif

#define errorf(...) logef(__VA_ARGS__)
#define assert_or_continue(cond, ...) \
    if (!(cond)) { errorf(__VA_ARGS__); continue; }
#define assert(cond, ...) \
    if (!(cond)) { errorf(__VA_ARGS__); return NULL; }
#define asserti(cond, ...) \
    if (!(cond)) { errorf(__VA_ARGS__); return 0; }
#define assertv(cond, ...) \
    if (!(cond)) { errorf(__VA_ARGS__); return; }

#define EOF_OBJ     "#<eof>"

#define DONT_FREE_THIS  INTPTR_MAX/2

#define N_CELLS     20 * 1024
#define SECD_ALIGN  4

typedef  struct secd    secd_t;
typedef  struct cell    cell_t;

typedef  struct atom  atom_t;
typedef  struct cons  cons_t;
typedef  struct error error_t;

typedef cell_t* (*secd_opfunc_t)(secd_t *);
typedef cell_t* (*secd_nativefunc_t)(secd_t *, cell_t *);

typedef enum cell_type {
    CELL_UNDEF,
    CELL_ATOM,
    CELL_CONS,
    CELL_ERROR,
} cell_type_e;

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


void print_cell(const cell_t *c);
cell_t *free_cell(cell_t *c);

void print_env(secd_t *secd);
cell_t *lookup_env(secd_t *secd, const char *symname);
cell_t *sexp_parse(secd_t *secd, DATA *f);


/*
 *  Cell accessors
 */

inline static enum cell_type cell_type(const cell_t *c) {
    return ((1 << SECD_ALIGN) - 1) & c->type;
}

inline static secd_t *cell_secd(const cell_t *c) {
    return (secd_t *)((INTPTR_MAX << SECD_ALIGN) & c->type);
}

inline static enum atom_type atom_type(const cell_t *c) {
    if (cell_type(c) != CELL_ATOM) return NOT_AN_ATOM;
    return (enum atom_type)(c->as.atom.type);
}

inline static bool is_nil(const cell_t *cell) {
    secd_t *secd = cell_secd(cell);
    return cell == secd->nil;
}
inline static bool not_nil(const cell_t *cell) {
    return !is_nil(cell);
}

inline static long cell_index(const cell_t *cons) {
    if (is_nil(cons)) return -1;
    return cons - cell_secd(cons)->data;
}

inline static const char * symname(const cell_t *c) {
    return c->as.atom.as.sym.data;
}

inline static int numval(const cell_t *c) {
    return c->as.atom.as.num;
}

inline static cell_t *list_next(const cell_t *cons) {
    if (cell_type(cons) != CELL_CONS) {
        errorf("list_next: not a cons at [%ld]\n", cell_index(cons));
        print_cell(cons);
        return NULL;
    }
    return cons->as.cons.cdr;
}

inline static cell_t *list_head(const cell_t *cons) {
    return cons->as.cons.car;
}

inline static cell_t *get_car(const cell_t *cons) {
    return cons->as.cons.car;
}
inline static cell_t *get_cdr(const cell_t *cons) {
    return cons->as.cons.cdr;
}
inline static bool is_cons(const cell_t *cell) {
    return cell_type(cell) == CELL_CONS;
}

void print_atom(const cell_t *c) {
    switch (atom_type(c)) {
      case ATOM_INT: printf("INT(%d)", c->as.atom.as.num); break;
      case ATOM_SYM: printf("SYM(%s)", c->as.atom.as.sym.data); break;
      case ATOM_FUNC: printf("BUILTIN(%p)", c->as.atom.as.op.fun); break;
      case NOT_AN_ATOM: printf("ERROR(not an atom)");
    }
}

void sexp_print_atom(const cell_t *c) {
    switch (atom_type(c)) {
      case ATOM_INT: printf("%d", c->as.atom.as.num); break;
      case ATOM_SYM: printf("%s", c->as.atom.as.sym.data); break;
      case ATOM_FUNC: printf("#*%p", c->as.atom.as.op.fun); break;
      case NOT_AN_ATOM: printf("???");
    }
}

void print_cell(const cell_t *c) {
    assertv(c, "print_cell(NULL)\n");
    if (is_nil(c)) {
         printf("NIL\n");
         return;
    }
    printf("[%ld]^%ld: ", cell_index(c), c->nref);
    switch (cell_type(c)) {
      case CELL_CONS:
        printf("CONS([%ld], [%ld])\n", cell_index(get_car(c)), cell_index(get_cdr(c)));
        break;
      case CELL_ATOM:
        print_atom(c); printf("\n");
        break;
      default:
        printf("unknown type: %d\n", cell_type(c));
    }
}

void print_list(cell_t *list) {
    printf("  -= ");
    while (not_nil(list)) {
        assertv(is_cons(list),
                "Not a cons at [%ld]\n", cell_index(list));
        printf("[%ld]:%ld\t", cell_index(list), cell_index(get_car(list)));
        print_cell(get_car(list));
        printf("  -> ");
        list = list_next(list);
    }
    printf("NIL\n");
}

void printc(cell_t *c) {
    assertv(c, "printc(NULL)");
    if (is_cons(c))
        print_list(c);
    else
        print_cell(c);
}

void sexp_print(cell_t *cell) {
    secd_t *secd = cell_secd(cell);
    switch (cell_type(cell)) {
      case CELL_ATOM:
        sexp_print_atom(cell);
        break;
      case CELL_CONS:
        printf("(");
        cell_t *iter = cell;
        while (not_nil(iter)) {
            if (iter != cell) printf(" ");
            if (cell_type(iter) != CELL_CONS) {
                printf(". "); sexp_print(iter); break;
            }

            // to avoid cycles:
            if (iter == secd->global_env) {
                printf("*global_env*");
                break;
            }
            cell_t *head = get_car(iter);
            sexp_print(head);
            iter = list_next(iter);
        }
        printf(") ");
        break;
      case CELL_ERROR:
        printf("???"); break;
      default:
        errorf("sexp_print: unknown cell type %d", (int)cell_type(cell));
    }
}

/*
 * Reference-counting
 */

inline static cell_t *share_cell(cell_t *c) {
    if (not_nil(c)) {
        ++c->nref;
        memtracef("share[%ld] %ld\n", cell_index(c), c->nref);
    } else {
        memdebugf("share[NIL]\n");
    }
    return c;
}

static cell_t *drop_cell(cell_t *c) {
    if (is_nil(c)) {
        memdebugf("drop [NIL]\n");
        return NULL;
    }
    if (c->nref <= 0) {
        assert(c->nref > 0, "drop_cell[%ld]: negative", cell_index(c));
    }

    -- c->nref;
    memtracef("drop [%ld] %ld\n", cell_index(c), c->nref);
    if (c->nref) return c;
    return free_cell(c);
}

/*
 *  Cell memory management
 */

int search_opcode_table(cell_t *sym);
bool is_control_compiled(cell_t *control);
cell_t *compile_control_path(secd_t *secd, cell_t *control);

cell_t *pop_free(secd_t *secd) {
    cell_t *cell = secd->free;
    assert(not_nil(cell), "pop_free: no free memory");

    secd->free = list_next(cell);
    memdebugf("NEW [%ld]\n", cell_index(cell));
    -- secd->free_cells;

    cell->type = (intptr_t)secd;
    return cell;
}

void push_free(cell_t *c) {
    assertv(c, "push_free(NULL)");
    assertv(c->nref == 0, "push_free: [%ld]->nref is %ld\n", cell_index(c), c->nref);
    secd_t *secd = cell_secd(c);
    c->type = (intptr_t)secd | CELL_CONS;
    c->as.cons.cdr = secd->free;
    secd->free = c;
    ++ secd->free_cells;
    memdebugf("FREE[%ld]\n", cell_index(c));
}

cell_t *new_cons(secd_t *secd, cell_t *car, cell_t *cdr) {
    cell_t *cell = pop_free(secd);
    cell->type |= CELL_CONS;
    cell->as.cons.car = share_cell(car);
    cell->as.cons.cdr = share_cell(cdr);
    return cell;
}

cell_t *new_number(secd_t *secd, int num) {
    cell_t *cell = pop_free(secd);
    cell->type |= CELL_ATOM;
    cell->as.atom.type = ATOM_INT;
    cell->as.atom.as.num = num;
    return cell;
}

cell_t *new_symbol(secd_t *secd, const char *sym) {
    cell_t *cell = pop_free(secd);
    cell->type |= CELL_ATOM;
    cell->as.atom.type = ATOM_SYM;
    cell->as.atom.as.sym.size = strlen(sym);
    cell->as.atom.as.sym.data = strdup(sym);
    //printf("new_symbol: '%s'\n", cell->as.atom.as.sym.data);
    return cell;
}

cell_t *new_clone(secd_t *secd, const cell_t *from) {
    if (!from) return NULL;
    cell_t *clone = pop_free(secd);
    memcpy(clone, from, sizeof(cell_t));
    clone->type = (intptr_t)secd | cell_type(from);
    clone->nref = 0;
    return clone;
}

cell_t *new_error(secd_t *secd, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
#define MAX_ERROR_SIZE  512
    char buf[MAX_ERROR_SIZE];
    vsnprintf(buf, MAX_ERROR_SIZE, fmt, va);
    va_end(va);

    cell_t *err = pop_free(secd);
    err->type |= CELL_ERROR;
    err->as.err.len = strlen(buf);
    err->as.err.msg = strdup(buf);
    return err;
}

void free_atom(cell_t *cell) {
    switch (cell->as.atom.type) {
      case ATOM_SYM:
        if (cell->as.atom.as.sym.size != DONT_FREE_THIS)
            free((char *)cell->as.atom.as.sym.data); break;
      default: return;
    }
}

cell_t *free_cell(cell_t *c) {
    enum cell_type t = cell_type(c);
    switch (t) {
      case CELL_ATOM:
        free_atom(c);
        break;
      case CELL_CONS:
        drop_cell(get_car(c));
        drop_cell(get_cdr(c));
        break;
      case CELL_ERROR:
        return c;
      default:
        return new_error(cell_secd(c), "free_cell: unknown cell_type 0x%x", t);
    }
    push_free(c);
    return NULL;
}

inline static cell_t *push(secd_t *secd, cell_t **to, cell_t *what) {
    cell_t *newtop = new_cons(secd, what, *to);
    drop_cell(*to);
    return (*to = share_cell(newtop));
}

inline static cell_t *pop(cell_t **from) {
    cell_t *top = *from;
    assert(not_nil(top), "pop: stack is empty");
    assert(is_cons(top), "pop: not a cons");

    cell_t *val = share_cell(get_car(top));
    *from = share_cell(get_cdr(top));
    drop_cell(top);
    return val;
}

cell_t *push_stack(secd_t *secd, cell_t *newc) {
    cell_t *top = push(secd, &secd->stack, newc);
    memdebugf("PUSH S[%ld (%ld, %ld)]\n", cell_index(top),
                        cell_index(get_car(top)), cell_index(get_cdr(top)));
    return top;
}

cell_t *pop_stack(secd_t *secd) {
    cell_t *cell = pop(&secd->stack);
    memdebugf("POP S[%ld]\n", cell_index(cell));
    return cell; // don't forget to drop_call(result)
}

cell_t *set_control(secd_t *secd, cell_t *opcons) {
    assert(is_cons(opcons),
           "set_control: failed, not a cons at [%ld]\n", cell_index(opcons));
    if (! is_control_compiled(opcons)) {
        opcons = compile_control_path(secd, opcons);
        assert(opcons, "set_control: failed to compile control path");
        //sexp_print(opcons);
    }
    return (secd->control = share_cell(opcons));
}

cell_t *pop_control(secd_t *secd) {
    return pop(&secd->control);
}

cell_t *push_dump(secd_t *secd, cell_t *cell) {
    cell_t *top = push(secd, &secd->dump, cell);
    memdebugf("PUSH D[%ld] (%ld, %ld)\n", cell_index(top),
            cell_index(get_car(top)), cell_index(get_cdr(top)));
    return top;
}

cell_t *pop_dump(secd_t *secd) {
    cell_t *cell = pop(&secd->dump);
    memdebugf("POP D[%ld]\n", cell_index(cell));
    return cell;
}

/*
 *  SECD built-ins
 */

cell_t *secd_cons(secd_t *secd) {
    ctrldebugf("CONS\n");
    cell_t *a = pop_stack(secd);

    cell_t *b = pop_stack(secd);

    cell_t *cons = new_cons(secd, a, b);
    drop_cell(a); drop_cell(b);

    return push_stack(secd, cons);
}

cell_t *secd_car(secd_t *secd) {
    ctrldebugf("CAR\n");
    cell_t *cons = pop_stack(secd);
    assert(cons, "secd_car: pop_stack() failed");
    assert(not_nil(cons), "secd_car: cons is NIL");
    assert(is_cons(cons), "secd_car: cons expected");

    cell_t *car = push_stack(secd, get_car(cons));
    drop_cell(cons);
    return car;
}

cell_t *secd_cdr(secd_t *secd) {
    ctrldebugf("CDR\n");
    cell_t *cons = pop_stack(secd);
    assert(cons, "secd_cdr: pop_stack() failed");
    assert(not_nil(cons), "secd_cdr: cons is NIL");
    assert(is_cons(cons), "secd_cdr: cons expected");

    cell_t *cdr = push_stack(secd, get_cdr(cons));
    drop_cell(cons);
    return cdr;
}

cell_t *secd_ldc(secd_t *secd) {
    ctrldebugf("LDC ");

    cell_t *arg = pop_control(secd);
    assert(arg, "secd_ldc: pop_control failed");
    if (CTRLDEBUG) printc(arg);

    push_stack(secd, arg);
    drop_cell(arg);
    return arg;
}

cell_t *secd_ld(secd_t *secd) {
    ctrldebugf("LD ");

    cell_t *arg = pop_control(secd);
    assert(arg, "secd_ld: stack empty");
    assert(atom_type(arg) == ATOM_SYM,
           "secd_ld: not a symbol [%ld]", cell_index(arg));
    if (CTRLDEBUG) printc(arg);

    const char *sym = symname(arg);
    cell_t *val = lookup_env(secd, sym);
    drop_cell(arg);
    assert(val, "lookup failed for %s", sym);
    return push_stack(secd, val);
}

static inline cell_t *to_bool(secd_t *secd, bool cond) {
    return ((cond)? lookup_env(secd, "#t") : secd->nil);
}

bool atom_eq(const cell_t *a1, const cell_t *a2) {
    enum atom_type atype1 = atom_type(a1);
    if (a1 == a2)
        return true;
    if (atype1 != atom_type(a2))
        return false;
    switch (atype1) {
      case ATOM_INT: return (a1->as.atom.as.num == a2->as.atom.as.num);
      case ATOM_SYM: return (!strcasecmp(symname(a1), symname(a2)));
      case ATOM_FUNC: return (a1->as.atom.as.op.fun == a2->as.atom.as.op.fun);
      default: errorf("atom_eq([%ld], [%ld]): don't know how to handle type %d\n",
                       cell_index(a1), cell_index(a2), atype1);
    }
    return false;
}

bool list_eq(const cell_t *xs, const cell_t *ys) {
    asserti(is_cons(xs), "list_eq: [%ld] is not a cons", cell_index(xs));
    if (xs == ys)   return true;
    while (not_nil(xs)) {
        if (!is_cons(xs)) return atom_eq(xs, ys);
        if (is_nil(ys)) return false;
        if (!is_cons(ys)) return false;
        const cell_t *x = get_car(xs);
        const cell_t *y = get_car(ys);
        if (not_nil(x)) {
            if (is_nil(y)) return false;
            if (is_cons(x)) {
                if (!list_eq(x, y)) return false;
            } else {
                if (!atom_eq(x, y)) return false;
            }
        } else {
            if (not_nil(y)) return false;
        }

        xs = list_next(xs);
        ys = list_next(ys);
    }
    return is_nil(ys);
}

cell_t *secd_atom(secd_t *secd) {
    ctrldebugf("ATOM\n");
    cell_t *val = pop_stack(secd);
    assert(val, "secd_atom: pop_stack() failed");
    assert(not_nil(val), "secd_atom: empty stack");

    cell_t *result = to_bool(secd, (val ? !is_cons(val) : true));
    drop_cell(val);
    return push_stack(secd, result);
}

cell_t *secd_eq(secd_t *secd) {
    ctrldebugf("EQ\n");
    cell_t *a = pop_stack(secd);
    assert(a, "secd_eq: pop_stack(a) failed");

    cell_t *b = pop_stack(secd);
    assert(b, "secd_eq: pop_stack(b) failed");

    bool eq = (is_cons(a) ? list_eq(a, b) : atom_eq(a, b));

    cell_t *val = to_bool(secd, eq);
    drop_cell(a); drop_cell(b);
    return push_stack(secd, val);
}

static cell_t *arithm_op(secd_t *secd, int op(int, int)) {
    cell_t *a = pop_stack(secd);
    assert(a, "secd_add: pop_stack(a) failed")
    assert(atom_type(a) == ATOM_INT, "secd_add: a is not int");

    cell_t *b = pop_stack(secd);
    assert(b, "secd_add: pop_stack(b) failed");
    assert(atom_type(b) == ATOM_INT, "secd_add: b is not int");

    int res = op(a->as.atom.as.num, b->as.atom.as.num);
    drop_cell(a); drop_cell(b);
    return push_stack(secd, new_number(secd, res));
}

inline static int iplus(int x, int y) {
    return x + y;
}
inline static int iminus(int x, int y) {
    return x - y;
}
inline static int imult(int x, int y) {
    return x * y;
}
inline static int idiv(int x, int y) {
    return x / y;
}
inline static int irem(int x, int y) {
    return x % y;
}

cell_t *secd_add(secd_t *secd) {
    ctrldebugf("ADD\n");
    return arithm_op(secd, iplus);
}
cell_t *secd_sub(secd_t *secd) {
    ctrldebugf("SUB\n");
    return arithm_op(secd, iminus);
}
cell_t *secd_mul(secd_t *secd) {
    ctrldebugf("MUL\n");
    return arithm_op(secd, imult);
}
cell_t *secd_div(secd_t *secd) {
    ctrldebugf("SUB\n");
    return arithm_op(secd, idiv);
}
cell_t *secd_rem(secd_t *secd) {
    ctrldebugf("SUB\n");
    return arithm_op(secd, irem);
}

cell_t *secd_leq(secd_t *secd) {
    ctrldebugf("LEQ\n");

    cell_t *opnd1 = pop_stack(secd);
    cell_t *opnd2 = pop_stack(secd);

    assert(atom_type(opnd1) == ATOM_INT, "secd_leq: int expected as opnd1");
    assert(atom_type(opnd2) == ATOM_INT, "secd_leq: int expected as opnd2");

    cell_t *result = to_bool(secd, opnd1->as.atom.as.num <= opnd2->as.atom.as.num);
    drop_cell(opnd1); drop_cell(opnd2);
    return push_stack(secd, result);
}

cell_t *secd_sel(secd_t *secd) {
    ctrldebugf("SEL ");

    cell_t *condcell = pop_stack(secd);
    if (CTRLDEBUG) printc(condcell);

    bool cond = not_nil(condcell) ? true : false;
    drop_cell(condcell);

    cell_t *thenb = pop_control(secd);
    cell_t *elseb = pop_control(secd);
    assert(is_cons(thenb) && is_cons(elseb), "secd_sel: both branches must be conses");

    cell_t *joinb = secd->control;
    secd->control = share_cell(cond ? thenb : elseb);

    push_dump(secd, joinb);

    drop_cell(thenb); drop_cell(elseb); drop_cell(joinb);
    return secd->control;
}

cell_t *secd_join(secd_t *secd) {
    ctrldebugf("JOIN\n");

    cell_t *joinb = pop_dump(secd);
    assert(joinb, "secd_join: pop_dump() failed");

    secd->control = joinb; //share_cell(joinb); drop_cell(joinb);
    return secd->control;
}



cell_t *secd_ldf(secd_t *secd) {
    ctrldebugf("LDF\n");

    cell_t *func = pop_control(secd);
    assert(func, "secd_ldf: failed to get the control path");

    cell_t *body = list_head(list_next(func));
    if (! is_control_compiled(body)) {
        cell_t *compiled = compile_control_path(secd, body);
        assert(compiled, "secd_ldf: failed to compile possible callee");
        drop_cell(body);
        func->as.cons.cdr->as.cons.car = share_cell(compiled);
    }

    cell_t *closure = new_cons(secd, func, secd->env);
    drop_cell(func);
    return push_stack(secd, closure);
}

#if TAILRECURSION
static cell_t * new_dump_if_tailrec(cell_t *control, cell_t *dump);
#endif

cell_t *secd_ap(secd_t *secd) {
    ctrldebugf("AP\n");

    cell_t *closure = pop_stack(secd);
    assert(is_cons(closure), "secd_ap: closure is not a cons");

    cell_t *argvals = pop_stack(secd);
    assert(argvals, "secd_ap: no arguments on stack");
    assert(is_cons(argvals), "secd_ap: a list expected for arguments");

    cell_t *func = get_car(closure);
    cell_t *newenv = get_cdr(closure);

    if (atom_type(func) == ATOM_FUNC) {
        secd_nativefunc_t native = (secd_nativefunc_t)func->as.atom.as.op.fun;
        cell_t *result = native(secd, argvals);
        assert(result, "secd_ap: a built-in routine failed");
        push_stack(secd, result);

        drop_cell(closure); drop_cell(argvals);
        return result;
    }
    assert(is_cons(func), "secd_ap: not a cons at func definition");

    cell_t *argnames = get_car(func);
    cell_t *control = list_head(list_next(func));
    assert(is_cons(control), "secd_ap: control path is not a list");

    if (! is_control_compiled( control )) {
        // control has not been compiled yet
        cell_t *compiled = compile_control_path(secd, control);
        assert(compiled, "secd_ap: failed to compile callee");
        //drop_cell(control); // no need: will be dropped with func
        control = compiled;
    }

#if TAILRECURSION
    cell_t *new_dump = new_dump_if_tailrec(secd->control, secd->dump);
    if (new_dump) {
        cell_t *dump = secd->dump;
        secd->dump = share_cell(new_dump);
        drop_cell(dump);  // dump may be new_dump, so don't drop before share
    } else {
#endif
        push_dump(secd, secd->control);
        push_dump(secd, secd->env);
        push_dump(secd, secd->stack);
#if TAILRECURSION
    }
#endif

    drop_cell(secd->stack);
    secd->stack = secd->nil;

    cell_t *frame = new_cons(secd, argnames, argvals);
    cell_t *oldenv = secd->env;
    drop_cell(oldenv);
    secd->env = share_cell(new_cons(secd, frame, newenv));

    drop_cell(secd->control);
    secd->control = share_cell(control);

    drop_cell(closure); drop_cell(argvals);
    return control;
}

cell_t *secd_rtn(secd_t *secd) {
    ctrldebugf("RTN\n");

    assert(is_nil(secd->control), "secd_rtn: commands after RTN");

    assert(not_nil(secd->stack), "secd_rtn: stack is empty");
    cell_t *top = pop_stack(secd);
    assert(is_nil(secd->stack), "secd_rtn: stack holds more than 1 value");

    cell_t *prevstack = pop_dump(secd);
    cell_t *prevenv = pop_dump(secd);
    cell_t *prevcontrol = pop_dump(secd);

    drop_cell(secd->env);

    secd->stack = share_cell(new_cons(secd, top, prevstack));
    drop_cell(top); drop_cell(prevstack);

    secd->env = prevenv; // share_cell(prevenv); drop_cell(prevenv); 
    secd->control = prevcontrol; // share_cell(prevcontrol); drop_cell(prevcontrol);

    return top;
}


cell_t *secd_dum(secd_t *secd) {
    ctrldebugf("DUM\n");

    cell_t *oldenv = secd->env;
    cell_t *newenv = new_cons(secd, secd->nil, oldenv);

    secd->env = share_cell(newenv);
    drop_cell(oldenv);

    return newenv;
}

cell_t *secd_rap(secd_t *secd) {
    ctrldebugf("RAP\n");

    cell_t *closure = pop_stack(secd);
    cell_t *argvals = pop_stack(secd);

    cell_t *newenv = get_cdr(closure);
    cell_t *func = get_car(closure);
    cell_t *argnames = get_car(func);
    cell_t *control = get_car(list_next(func));

    if (! is_control_compiled( control )) {
        // control has not been compiled yet
        cell_t *compiled = compile_control_path(secd, control);
        assert(compiled, "secd_rap: failed to compile callee");
        control = compiled;
    }

    push_dump(secd, secd->control);
    push_dump(secd, get_cdr(secd->env));
    push_dump(secd, secd->stack);

    cell_t *frame = new_cons(secd, argnames, argvals);
#if CTRLDEBUG
    printf("new frame: \n"); print_cell(frame);
    printf(" argnames: \n"); printc(argnames);
    printf(" argvals : \n"); printc(argvals);
#endif
    newenv->as.cons.car = share_cell(frame);

    drop_cell(secd->stack);
    secd->stack = secd->nil;

    cell_t *oldenv = secd->env;
    secd->env = share_cell(newenv);

    drop_cell(secd->control);
    secd->control = share_cell(control);

    drop_cell(oldenv);
    drop_cell(closure); drop_cell(argvals);
    return secd->control;
}


cell_t *secd_read(secd_t *secd) {
    ctrldebugf("READ\n");

    cell_t *inp = sexp_parse(secd, NULL);
    assert(inp, "secd_read: failed to read");

    push_stack(secd, inp);
    return inp;
}

cell_t *secd_print(secd_t *secd) {
    ctrldebugf("PRINT\n");

    cell_t *top = get_car(secd->stack);
    assert(top, "secd_print: no stack");

    sexp_print(top);
    printf("\n");
    return top;
}


/*
 * Some native functions
 */


cell_t *secdf_list(secd_t __unused *secd, cell_t *args) {
    ctrldebugf("secdf_list\n");
    return args;
}

cell_t *secdf_null(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_nullp\n");
    assert(not_nil(args), "secdf_copy: one argument expected");
    return to_bool(secd, is_nil(list_head(args)));
}

cell_t *secdf_nump(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_nump\n");
    assert(not_nil(args), "secdf_copy: one argument expected");
    return to_bool(secd, atom_type(list_head(args)) == ATOM_INT);
}

cell_t *secdf_symp(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_symp\n");
    assert(not_nil(args), "secdf_copy: one argument expected");
    return to_bool(secd, atom_type(list_head(args)) == ATOM_SYM);
}

static cell_t *list_copy(secd_t *secd, cell_t *list, cell_t **out_tail) {
    if (is_nil(list))
        return secd->nil;

    cell_t *new_head, *new_tail;
    new_head = new_tail = new_cons(secd, list_head(list), secd->nil);

    while (not_nil(list = list_next(list))) {
        cell_t *new_cell = new_cons(secd, get_car(list), secd->nil);
        new_tail->as.cons.cdr = share_cell(new_cell);
        new_tail = list_next(new_tail);
    }
    if (out_tail)
        *out_tail = new_tail;
    return new_head;
}

cell_t *secdf_copy(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_copy\n");
    return list_copy(secd, list_head(args), NULL);
}

cell_t *secdf_append(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_append\n");
    assert(args, "secdf_append: args is NULL");

    if (is_nil(args)) 
        return args;

    cell_t *xs = list_head(args);
    assert(is_cons(list_next(args)), "secdf_append: expected two arguments");

    cell_t *argtail = list_next(args);
    if (is_nil(argtail))
        return xs;

    cell_t *ys = list_head(argtail);
    if (not_nil(list_next(argtail))) {
          ys = secdf_append(secd, argtail);
    }

    if (is_nil(xs))
        return ys;

    cell_t *sum = xs;
    cell_t *sum_tail = xs;
    while (true) {
        if (sum_tail->nref > 1) {
            sum_tail = NULL; // xs must be copied
            break;
        }
        if (is_nil(list_next(sum_tail)))
            break;
        sum_tail = list_next(sum_tail);
    }

    if (sum_tail) {
        ctrldebugf("secdf_append: destructive append\n");
        sum_tail->as.cons.cdr = share_cell(ys);
        sum = xs;
    } else {
        ctrldebugf("secdf_append: copying append\n");
        cell_t *sum_tail;
        sum = list_copy(secd, xs, &sum_tail);
        sum_tail->as.cons.cdr = share_cell(ys);
    }

    return sum;
}

cell_t *secdf_eofp(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_eofp\n");
    cell_t *arg1 = list_head(args);
    if (atom_type(arg1) != ATOM_SYM)
        return secd->nil;
    return to_bool(secd, str_eq(symname(arg1), EOF_OBJ));
}

cell_t *secdf_ctl(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_ctl\n");
    cell_t *arg1 = list_head(args);

    if (atom_type(arg1) == ATOM_SYM) {
        if (str_eq(symname(arg1), "free")) {
            printf("SECDCTL: Available cells: %d\n", secd->free_cells);
        } else if (str_eq(symname(arg1), "env")) {
            print_env(secd);
        } else if (str_eq(symname(arg1), "help")) {
            printf("SECDCTL: options are 'help', 'env', 'free'\n");
        }
    }
    return args;
}

cell_t *secdf_getenv(secd_t *secd, cell_t __unused *args) {
    ctrldebugf("secdf_getenv\n");
    return secd->env;
}

cell_t *secdf_bind(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_bind\n");

    assert(not_nil(args), "secdf_bind: can't bind nothing to nothing");
    cell_t *sym = list_head(args);
    assert(atom_type(sym) == ATOM_SYM, "secdf_bind: a symbol must be bound");

    args = list_next(args);
    assert(not_nil(args), "secdf_bind: No value for binding");
    cell_t *val = list_head(args);

    cell_t *env;
    // is there the third argument?
    if (not_nil(list_next(args))) {
        args = list_next(args);
        env = list_head(args);
    } else {
        env = secd->global_env;
    }
    
    cell_t *frame = list_head(env);
    cell_t *old_syms = get_car(frame);
    cell_t *old_vals = get_cdr(frame);

    // an intersting side effect: since there's no check for 
    // re-binding an existing symbol, we can create multiple 
    // copies of it on the frame, the last added is found 
    // during value lookup, but the old ones are persistent
    frame->as.cons.car = share_cell(new_cons(secd, sym, old_syms));
    frame->as.cons.cdr = share_cell(new_cons(secd, val, old_vals));

    drop_cell(old_syms); drop_cell(old_vals);
    return sym;
}

#define INIT_SYM(name) {    \
    .type = CELL_ATOM,      \
    .nref = DONT_FREE_THIS, \
    .as.atom = {            \
      .type = ATOM_SYM,     \
      .as.sym = {           \
        .size = DONT_FREE_THIS, \
        .data = (name) } } }

#define INIT_NUM(num) {     \
    .type = CELL_ATOM,      \
    .nref = DONT_FREE_THIS, \
    .as.atom = {            \
      .type = ATOM_INT,     \
      .as.num = (num) }}

#define INIT_FUNC(func) {   \
    .type = CELL_ATOM,      \
    .nref = DONT_FREE_THIS, \
    .as.atom = {            \
      .type = ATOM_FUNC,   \
      .as.op = {           \
        .fun = (secd_opfunc_t)(func),     \
        .sym = NULL        \
    }}}

const cell_t cons_func  = INIT_FUNC(secd_cons);
const cell_t car_func   = INIT_FUNC(secd_car);
const cell_t cdr_func   = INIT_FUNC(secd_cdr);
const cell_t add_func   = INIT_FUNC(secd_add);
const cell_t sub_func   = INIT_FUNC(secd_sub);
const cell_t mul_func   = INIT_FUNC(secd_mul);
const cell_t div_func   = INIT_FUNC(secd_div);
const cell_t rem_func   = INIT_FUNC(secd_rem);
const cell_t leq_func   = INIT_FUNC(secd_leq);
const cell_t ldc_func   = INIT_FUNC(secd_ldc);
const cell_t ld_func    = INIT_FUNC(secd_ld);
const cell_t eq_func    = INIT_FUNC(secd_eq);
const cell_t atom_func  = INIT_FUNC(secd_atom);
const cell_t sel_func   = INIT_FUNC(secd_sel);
const cell_t join_func  = INIT_FUNC(secd_join);
const cell_t ldf_func   = INIT_FUNC(secd_ldf);
const cell_t ap_func    = INIT_FUNC(secd_ap);
const cell_t rtn_func   = INIT_FUNC(secd_rtn);
const cell_t dum_func   = INIT_FUNC(secd_dum);
const cell_t rap_func   = INIT_FUNC(secd_rap);
const cell_t read_func  = INIT_FUNC(secd_read);
const cell_t print_func = INIT_FUNC(secd_print);
const cell_t stop_func  = INIT_FUNC(NULL);

const cell_t ap_sym     = INIT_SYM("AP");
const cell_t add_sym    = INIT_SYM("ADD");
const cell_t atom_sym   = INIT_SYM("ATOM");
const cell_t car_sym    = INIT_SYM("CAR");
const cell_t cdr_sym    = INIT_SYM("CDR");
const cell_t cons_sym   = INIT_SYM("CONS");
const cell_t div_sym    = INIT_SYM("DIV");
const cell_t dum_sym    = INIT_SYM("DUM");
const cell_t eq_sym     = INIT_SYM("EQ");
const cell_t join_sym   = INIT_SYM("JOIN");
const cell_t ld_sym     = INIT_SYM("LD");
const cell_t ldc_sym    = INIT_SYM("LDC");
const cell_t ldf_sym    = INIT_SYM("LDF");
const cell_t leq_sym    = INIT_SYM("LEQ");
const cell_t mul_sym    = INIT_SYM("MUL");
const cell_t print_sym  = INIT_SYM("PRINT");
const cell_t rap_sym    = INIT_SYM("RAP");
const cell_t read_sym   = INIT_SYM("READ");
const cell_t rem_sym    = INIT_SYM("REM");
const cell_t rtn_sym    = INIT_SYM("RTN");
const cell_t sel_sym    = INIT_SYM("SEL");
const cell_t stop_sym   = INIT_SYM("STOP");
const cell_t sub_sym    = INIT_SYM("SUB");

const cell_t t_sym      = INIT_SYM("#t");
const cell_t nil_sym    = INIT_SYM("NIL");

const struct {
    const cell_t *sym;
    const cell_t *val;
    int args;       // takes 'args' control cells after the opcode
} opcode_table[] = {
    // opcodes: for information, not to be called
    // keep symbols sorted properly
    { &add_sym,     &add_func,  0},
    { &ap_sym,      &ap_func,   0},
    { &atom_sym,    &atom_func, 0},
    { &car_sym,     &car_func,  0},
    { &cdr_sym,     &cdr_func,  0},
    { &cons_sym,    &cons_func, 0},
    { &div_sym,     &div_func,  0},
    { &dum_sym,     &dum_func,  0},
    { &eq_sym,      &eq_func,   0},
    { &join_sym,    &join_func, 0},
    { &ld_sym,      &ld_func,   1},
    { &ldc_sym,     &ldc_func,  1},
    { &ldf_sym,     &ldf_func,  1},
    { &leq_sym,     &leq_func,  0},
    { &mul_sym,     &mul_func,  0},
    { &print_sym,   &print_func,0},
    { &rap_sym,     &rap_func,  0},
    { &read_sym,    &read_func, 0},
    { &rem_sym,     &rem_func,  0},
    { &rtn_sym,     &rtn_func,  0},
    { &sel_sym,     &sel_func,  2},
    { &stop_sym,    &stop_func, 0},
    { &sub_sym,     &sub_func,  0},

    { NULL,         NULL,       0}
};

int search_opcode_table(cell_t *sym) {
    int a = 0;
    int b = 0;
    while (opcode_table[b].sym) ++b;
    while (a < b) {
        //printf("a = %d, b = %d\n", a, b);
        int c = (a + b) / 2;
        int ord = str_cmp( symname(sym), symname(opcode_table[c].sym));
        if (ord == 0) return c;
        if (ord > 0) b = c;
        else a = c;
    }
    return -1;
}

cell_t *compile_control_path(secd_t *secd, cell_t *control) {
    assert(control, "control path is NULL");
    cell_t *compiled = secd->nil;

    cell_t *cursor = control;
    cell_t *compcursor = compiled;
    while (not_nil(cursor)) {
        cell_t *opcode = list_head(cursor);
        if (atom_type(opcode) != ATOM_SYM) {
            errorf("compile_control: not a symbol in control path\n");
            sexp_print(opcode);
            return NULL;
        }

        int opind = search_opcode_table(opcode);
        assert(opind != -1, "Opcode not found: %s", symname(opcode))

        cell_t *new_cmd = new_clone(secd, opcode_table[opind].val);
        cell_t *cc = new_cons(secd, new_cmd, secd->nil);
        if (not_nil(compcursor)) {
            compcursor->as.cons.cdr = share_cell(cc);
            compcursor = list_next(compcursor);
        } else {
            compiled = compcursor = cc;
        }
        cursor = list_next(cursor);

        cell_t *new_tail;
        if (opcode_table[opind].args > 0) {
            if (new_cmd->as.atom.as.op.fun == &secd_sel) {
                cell_t *thenb = compile_control_path(secd, list_head(cursor));
                new_tail = new_cons(secd, thenb, secd->nil);
                compcursor->as.cons.cdr = share_cell(new_tail);
                compcursor = list_next(compcursor);
                cursor = list_next(cursor);

                cell_t *elseb = compile_control_path(secd, list_head(cursor));
                new_tail = new_cons(secd, elseb, secd->nil);
                compcursor->as.cons.cdr = share_cell(new_tail);
                compcursor = list_next(compcursor);
                cursor = list_next(cursor);
            } else {
                new_tail = new_cons(secd, list_head(cursor), secd->nil);
                compcursor->as.cons.cdr = share_cell(new_tail);
                compcursor = list_next(compcursor);
                cursor = list_next(cursor);
            }
        }
    }
    return compiled;
}

bool is_control_compiled(cell_t *control) {
    return atom_type(list_head(control)) == ATOM_FUNC;
}

#if TAILRECURSION
/*
 * returns a new dump for a tail-recursive call
 * or NULL if no Tail Call Optimization.
 */
static cell_t * new_dump_if_tailrec(cell_t *control, cell_t *dump) {
    if (is_nil(control))
        return NULL;

    cell_t *nextop = list_head(control);
    if (atom_type(nextop) != ATOM_FUNC)
        return NULL;
    secd_opfunc_t opfun = nextop->as.atom.as.op.fun;

    if (opfun == &secd_rtn) {
        return dump;
    } else if (opfun == &secd_join) {
        cell_t *join = list_head(dump);
        return new_dump_if_tailrec(join, list_next(dump));
    } else if (opfun == &secd_cons) {
        /* a situation of CONS CAR - it is how `begin` implemented */
        cell_t *nextcontrol = list_next(control);
        cell_t *afternext = list_head(nextcontrol);
        if (atom_type(afternext) != ATOM_FUNC)
            return NULL;
        if (afternext->as.atom.as.op.fun != &secd_car)
            return NULL;
        return new_dump_if_tailrec(list_next(nextcontrol), dump);
    }

    /* all other commands (except DUM, which must have RAP after it)
     * mess with the stack. TCO's not possible: */
    return NULL;
}
#endif

const cell_t list_sym   = INIT_SYM("list");
const cell_t append_sym = INIT_SYM("append");
const cell_t copy_sym   = INIT_SYM("copy");
const cell_t nullp_sym  = INIT_SYM("null?");
const cell_t nump_sym   = INIT_SYM("number?");
const cell_t symp_sym   = INIT_SYM("symbol?");
const cell_t eofp_sym   = INIT_SYM("eof-object?");
const cell_t debug_sym  = INIT_SYM("secdctl");
const cell_t env_sym    = INIT_SYM("interaction-environment");
const cell_t bind_sym   = INIT_SYM("secd-bind!");

const cell_t list_func  = INIT_FUNC(secdf_list);
const cell_t append_func = INIT_FUNC(secdf_append);
const cell_t copy_func  = INIT_FUNC(secdf_copy);
const cell_t nullp_func = INIT_FUNC(secdf_null);
const cell_t nump_func  = INIT_FUNC(secdf_nump);
const cell_t symp_func  = INIT_FUNC(secdf_symp);
const cell_t eofp_func  = INIT_FUNC(secdf_eofp);
const cell_t debug_func = INIT_FUNC(secdf_ctl);
const cell_t getenv_fun = INIT_FUNC(secdf_getenv);
const cell_t bind_func  = INIT_FUNC(secdf_bind);

const struct {
    const cell_t *sym;
    const cell_t *val;
} native_functions[] = {
    // native functions
    { &list_sym,    &list_func  },
    { &append_sym,  &append_func },
    { &nullp_sym,   &nullp_func },
    { &nump_sym,    &nump_func  },
    { &symp_sym,    &symp_func  },
    { &copy_sym,    &copy_func  },
    { &eofp_sym,    &eofp_func  },
    { &debug_sym,   &debug_func  },
    { &env_sym,     &getenv_fun },
    { &bind_sym,    &bind_func  },

    // symbols
    { &t_sym,       &t_sym      },
    { NULL,         NULL        } // must be last
};

void fill_global_env(secd_t *secd) {
    int i;
    cell_t *symlist = secd->nil;
    cell_t *vallist = secd->nil;

    cell_t *env = new_cons(secd, secd->nil, secd->nil);

    for (i = 0; native_functions[i].sym; ++i) {
        cell_t *sym = new_clone(secd, native_functions[i].sym);
        cell_t *val = new_clone(secd, native_functions[i].val);
        sym->nref = val->nref = DONT_FREE_THIS;
        cell_t *closure = new_cons(secd, val, env);
        symlist = new_cons(secd, sym, symlist);
        vallist = new_cons(secd, closure, vallist);
    }

    cell_t *sym = new_clone(secd, &nil_sym);
    cell_t *val = secd->nil;
    symlist = new_cons(secd, sym, symlist);
    vallist = new_cons(secd, val, vallist);

    cell_t *frame = new_cons(secd, symlist, vallist);
    env->as.cons.car = share_cell(frame);

    secd->env = share_cell(env);
    secd->global_env = secd->env;
}

cell_t *lookup_env(secd_t *secd, const char *symbol) {
    cell_t *env = secd->env;
    assert(cell_type(env) == CELL_CONS, "lookup_env: environment is not a list\n");

    while (not_nil(env)) {       // walk through frames
        cell_t *frame = get_car(env);
        if (is_nil(frame)) {
            //printf("lookup_env: warning: skipping OMEGA-frame...\n");
            env = list_next(env);
            continue;
        }
        cell_t *symlist = get_car(frame);
        cell_t *vallist = get_cdr(frame);

        while (not_nil(symlist)) {   // walk through symbols
            cell_t *cur_sym = get_car(symlist);
            if (atom_type(cur_sym) != ATOM_SYM) {
                errorf("lookup_env: variable at [%ld] is not a symbol\n",
                        cell_index(cur_sym));
                symlist = list_next(symlist); vallist = list_next(vallist);
                continue;
            }

            if (str_eq(symbol, symname(cur_sym))) {
                return get_car(vallist);
            }
            symlist = list_next(symlist);
            vallist = list_next(vallist);
        }

        env = list_next(env);
    }
    printf("lookup_env: %s not found\n", symbol);
    return NULL;
}

cell_t *lookup_symbol(secd_t *secd, const char *symbol) {
    cell_t *env = secd->env;
    assert(cell_type(env) == CELL_CONS, "lookup_symbol: environment is not a list\n");

    while (not_nil(env)) {       // walk through frames
        cell_t *frame = get_car(env);
        cell_t *symlist = get_car(frame);

        while (not_nil(symlist)) {   // walk through symbols
            cell_t *cur_sym = get_car(symlist);
            assert(atom_type(cur_sym) != ATOM_SYM,
                    "lookup_symbol: variable at [%ld] is not a symbol\n", cell_index(cur_sym));

            if (str_eq(symbol, symname(cur_sym))) {
                return cur_sym;
            }
            symlist = list_next(symlist);
        }

        env = list_next(env);
    }
    return NULL;
}

void print_env(secd_t *secd) {
    cell_t *env = secd->env;
    int i = 0;
    printf("Environment:\n");
    while (not_nil(env)) {
        printf(" Frame #%d:\n", i++);
        cell_t *frame = get_car(env);
        cell_t *symlist = get_car(frame);
        cell_t *vallist = get_cdr(frame);

        while (not_nil(symlist)) {
            cell_t *sym = get_car(symlist);
            cell_t *val = get_car(vallist);
            if (atom_type(sym) != ATOM_SYM)
                logef("print_env: not a symbol at *%p in vallist\n", sym);
            printf("  %s\t=>\t", symname(sym));
            print_cell(val);

            symlist = list_next(symlist);
            vallist = list_next(vallist);
        }

        env = list_next(env);
    }
}

/*
 *  SECD parser
 *  A parser of a simple Lisp subset
 */
#define MAX_LEXEME_SIZE     256

typedef  int  token_t;
typedef  struct secd_parser secd_parser_t;

enum {
    TOK_EOF = -1,
    TOK_STR = -2,
    TOK_NUM = -3,

    TOK_QUOTE = -4,
    TOK_QQ = -5,
    TOK_UQ = -6,
    TOK_UQSPL = -7,

    TOK_ERR = -65536
};

const char not_symbol_chars[] = " ();\n";

struct secd_parser {
    token_t token;
    DATA *f;

    /* lexer guts */
    int lc;
    int numtok;
    char strtok[MAX_LEXEME_SIZE];
    char issymbc[UCHAR_MAX + 1];

    int nested;
};

cell_t *sexp_read(secd_t *secd, secd_parser_t *p);

secd_parser_t *init_parser(secd_parser_t *p, DATA *f) {
    p->lc = ' ';
    p->f = f;  // (f ? f : stdin);
    p->nested = 0;

    memset(p->issymbc, false, 0x20);
    memset(p->issymbc + 0x20, true, UCHAR_MAX - 0x20);
    const char *s = not_symbol_chars;
    while (*s)
        p->issymbc[(unsigned char)*s++] = false;
    return p;
}

secd_parser_t *new_parser(DATA *f) {
    secd_parser_t *p = (secd_parser_t *)calloc(1, sizeof(secd_parser_t));
    return init_parser(p, f);
}

inline static int nextchar(secd_parser_t *p) {
    //printf("nextchar\n");
    p->lc = dgetc(p->f);
    //printf("dgetc='%c'(%x) ", p->lc, p->lc);
    return p->lc;
}

token_t lexnext(secd_parser_t *p) {
    /* skip spaces */
    while (isspace(p->lc))
        nextchar(p);

    switch (p->lc) {
      case EOF:
        return (p->token = TOK_EOF);
      case ';':
        /* comment */
        do nextchar(p); while (p->lc != '\n');
        return lexnext(p);

      case '(': case ')': 
        /* one-char tokens */
        p->token = p->lc;
        nextchar(p);
        return p->token;
      case '\'': 
        nextchar(p);
        return (p->token = TOK_QUOTE);
      case '`':
        nextchar(p);
        return (p->token = TOK_QQ);
      case ',':
        /* may be ',' or ',@' */
        nextchar(p);
        if (p->lc == '@') {
            nextchar(p);
            return (p->token = TOK_UQSPL);
        }
        return (p->token = TOK_UQ);
    }

    if (isdigit(p->lc)) {
        char *s = p->strtok;
        do {
            *s++ = p->lc; nextchar(p);
        } while (isdigit(p->lc));
        *s = '\0';

        p->numtok = atoi(p->strtok);
        return (p->token = TOK_NUM);
    }

    if (p->issymbc[(unsigned char)p->lc]) {
        char *s = p->strtok;
        do {
            *s++ = p->lc;
            nextchar(p);
        } while (p->issymbc[(unsigned char)p->lc]);
        *s = '\0';

        return (p->token = TOK_STR);
    }
    return TOK_ERR;
}

static const char * special_form_for(int token) {
    switch (token) {
      case TOK_QUOTE: return "quote";
      case TOK_QQ:    return "quasiquote";
      case TOK_UQ:    return "unquote";
      case TOK_UQSPL: return "unquote-splicing";
    }
    return NULL;
}

static void print_token(int tok, secd_parser_t *p) {
    switch (tok) {
       case TOK_STR: printf("TOK_STR: '%s'\n", p->strtok); break;
       case TOK_NUM: printf("TOK_NUM: %d\n", p->numtok); break;
       case TOK_EOF: printf("TOK_EOF\n"); break;
       case TOK_QUOTE: printf("TOK_Q\n"); break;
       case TOK_QQ: printf("TOK_QQ\n"); break;
       case TOK_UQ: printf("TOK_UQ\n"); break;
       default: printf("Token: %c\n", tok);
    }
}

cell_t *read_list(secd_t *secd, secd_parser_t *p) {
    cell_t *head = secd->nil, *tail = secd->nil;
    cell_t *newtail, *val;
    while (true) {
        int tok = lexnext(p);
        switch (tok) {
          case TOK_STR:
              val = new_symbol(secd, p->strtok);
              break;
          case TOK_NUM:
              val = new_number(secd, p->numtok);
              break;
          case TOK_EOF: case ')':
              -- p->nested;
              return head;
          case '(':
              ++ p->nested;
              val = read_list(secd, p);
              if (p->token == TOK_ERR) {
                  free_cell(head);
                  errorf("read_list: TOK_ERR\n");
                  return NULL;
              }
              if (p->token == TOK_EOF) {
                  free_cell(head);
                  errorf("read_list: TOK_EOF, ')' expected\n");
              }
              assert(p->token == ')', "read_list: not a closing bracket");
              break;

           case TOK_QUOTE: case TOK_QQ: 
           case TOK_UQ: case TOK_UQSPL: {
              const char *formname = special_form_for(tok);
              assert(formname, "No formname for token=%d\n", tok);
              val = sexp_read(secd, p);
              val = new_cons(secd, new_symbol(secd, formname),
                                   new_cons(secd, val, secd->nil));
              } break;

           default:
              errorf("Unknown token: 0x%x ('%1$c')", tok);
              free_cell(head);
              return NULL;
        }

        newtail = new_cons(secd, val, secd->nil);
        if (not_nil(head)) {
            tail->as.cons.cdr = share_cell(newtail);
            tail = newtail;
        } else {
            head = tail = newtail;
        }
    }
}

cell_t *sexp_read(secd_t *secd, secd_parser_t *p) {
    cell_t *inp = secd->nil;
    int tok;
    switch (tok = lexnext(p)) {
      case '(':
        inp = read_list(secd, p);
        if (p->token != ')') {
            errorf("read_secd: failed\n");
            if (inp) drop_cell(inp);
            return NULL;
        }
        break;
      case TOK_NUM:
        inp = new_number(secd, p->numtok);
        break;
      case TOK_STR:
        inp = new_symbol(secd, p->strtok);
        break;
      case TOK_EOF:
        return new_symbol(secd, EOF_OBJ);

      case TOK_QUOTE: case TOK_QQ:
      case TOK_UQ: case TOK_UQSPL: {
        const char *formname = special_form_for(tok);
        assert(formname, "No  special form for token=%d\n", tok);
        inp = sexp_read(secd, p);
        inp = new_cons(secd, new_symbol(secd, formname),
                             new_cons(secd, inp, secd->nil));
        } break;

      default:
        errorf("Unknown token: 0x%x ('%1$c')", tok);
        return NULL;
    }
    return inp;
}

cell_t *sexp_parse(secd_t *secd, DATA *f) {
    secd_parser_t p;
    init_parser(&p, f);
    return sexp_read(secd, &p);
}

cell_t *read_secd(secd_t *secd, DATA *f) {
    secd_parser_t p;
    init_parser(&p, f);

    if (lexnext(&p) != '(') {
        errorf("read_secd: a list of commands expected\n");
        return NULL;
    }

    cell_t *result = read_list(secd, &p);
    if (p.token != ')') {
        errorf("read_secd: the end bracket expected\n");
        if (result) drop_cell(result);
        return NULL;
    }
    return result;
}

/*
 * SECD machine
 */

secd_t * init_secd(secd_t *secd) {
    /* allocate memory chunk */
    secd->data = (cell_t *)calloc(N_CELLS, sizeof(cell_t));
    if (!secd->data) return null;

    /* mark up a list of free cells */
    int i;
    for (i = 0; i < N_CELLS - 1; ++i) {
        cell_t *c = secd->data + i;
        c->type = (intptr_t)secd | CELL_CONS;
        c->as.cons.cdr = secd->data + i + 1;
    }
    cell_t * c = secd->data + N_CELLS - 1;
    secd->nil = c;
    c->type = (intptr_t)secd | CELL_CONS;
    c->as.cons.cdr = NULL;

    secd->free = secd->data;
    secd->stack = secd->dump =  secd->nil;
    secd->control = secd->env =  secd->nil;

    secd->free_cells = N_CELLS - 1;
    secd->used_stack = 0;

    return secd;
}

void run_secd(secd_t *secd) {
    cell_t *op;
    while (true)  {
        op = pop_control(secd);
        assertv(op, "run: no command");
        assert_or_continue(
                atom_type(op) == ATOM_FUNC,
                "run: not an opcode at [%ld]\n", cell_index(op));

        secd_opfunc_t callee = (secd_opfunc_t) op->as.atom.as.op.fun;
        if (NULL == callee) return;  // STOP

        cell_t *ret = callee(secd);
        assertv(ret, "run: Instruction failed\n");
        drop_cell(op);
    }
}

secd_t __attribute__((aligned(1 << SECD_ALIGN))) secd;

int secd_main(DATA *f) {
    if (!init_secd(&secd))
        return 1;

    fill_global_env(&secd);
    if (ENVDEBUG) print_env(&secd);

    envdebugf(">>>>>\n");
    cell_t *inp = read_secd(&secd, f);

    asserti(inp, "read_secd failed");
    if (is_nil(inp)) {
        printf("no commands.\n\n");
        return 0;
    }

    set_control(&secd, inp);
    if (ENVDEBUG) {
        envdebugf("Control path:\n");
        print_list(secd.control);
    }

    envdebugf("<<<<<\n");
    run_secd(&secd);

    envdebugf("-----\n");
    if (not_nil(secd.stack)) {
        envdebugf("Stack head:\n");
        printc(get_car(secd.stack));
    } else {
        envdebugf("Stack is empty\n");
    }
    free(secd.data);
    return 0;
}

#if COSEC
/*
 *  Data: simple file emulation to fetch scm2secd.secd to the machine
 */

struct DATA {
    uint8_t *pos;
};

extern uint8_t _binary_src_core_repl_secd_start;
extern uint8_t _binary_src_core_repl_secd_end;
extern size_t _binary_src_core_repl_secd_size;

#define CONSOLE_BUFFER 256
#define DBUF_SIZE      (CONSOLE_BUFFER * 2)
struct {
    // circular buffer:
    int getpos;     // left border of data, to output with dgetc()
    int inppos;     // right border of data, to append new data
    char buf[DBUF_SIZE];
} dstdin = {
    .getpos = 0,
    .inppos = 0,
};

char dbuf[CONSOLE_BUFFER];

int dgetc(DATA *d) {
    if (!d) { 
        /* not so simple, line bufferization required */
        if (dstdin.getpos == dstdin.inppos) {
            console_readline(dbuf, CONSOLE_BUFFER);
            //printf("console_read: '%s'\n", dbuf);

            char *src = dbuf;
            char *dst = dstdin.buf + dstdin.inppos;
            while (*src) {
                *dst = *src;
                ++src; ++dst;
                if (dst >= (dstdin.buf + DBUF_SIZE))
                   dst = dstdin.buf;
            }
            *dst = '\n';
            ++dst; if (dst >= (dstdin.buf + DBUF_SIZE)) dst = dstdin.buf;
            dstdin.inppos = dst - dstdin.buf;
            return dstdin.buf[ dstdin.getpos ++];
        } else {
            char c = dstdin.buf[ dstdin.getpos ];
            ++dstdin.getpos;
            if (dstdin.getpos >= DBUF_SIZE)
                dstdin.getpos = 0;
            return (int) c;
        }
    }

    if (d->pos >= &_binary_src_core_repl_secd_end) 
        return EOF;

    uint8_t c = *(d->pos);
    ++ d->pos;
    return (int)c;
}

int run_lisp(void) {
    DATA d = { .pos = &_binary_src_core_repl_secd_start };
    return secd_main(&d);
}

#endif
