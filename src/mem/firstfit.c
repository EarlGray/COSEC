#include <mem/firstfit.h>
#include <log.h>

#ifndef __LANGEXTS__
typedef unsigned uint, size_t;
typedef char bool;
#define true    1
#define false   0
#define null ((void *)0)
#endif // __LANGEXTS__

/***
 *          Memory allocator with first-fit algorithm
 *
 **************************************************************************
 *  Given a data area starting with 'startmem' and length equal to 'size':
 *
 *                General layout:
 *
 *   <-----------------  size  ----------------->
 *   <---- header  ---> <--- allocation area --->
 *
 *   |-----------------|----------  . . . -------|
 *   ^                 ^                         ^
 * startmem       startmem+ALLOC_SIZE          endmem
 *
 *
 *             Allocation area layout:
 *
 *  prev
 *  data --><---- info chunk --><------ data -------><--next chunk
 *
 *    ------|-------------------|------- . . . -----|-----
 *          ^                   ^                   ^
 *        chunk           actual pointer       chunk->next
 *
 *  Actual data are aligned by ALIGN (see code). Chunks are connected into
 *  simple circular list, allocator structure stores current, previous and
 *  pre-previous pointers.
 *  Allocation procedure can split chunks into smaller if needed, but never
 *  merges; deallocation merges free chunks if needed.
 *  Area header is identifier used by external parts of program;
 *  Chunk header contains checksum for controling heap corruption, free/used
 *  bit, pointer to next chunk and size of an attended data area.
 *  The current implementation chose sizeof(chunk_t) == 12 and ALIGN = 16 in
 *  order to store single ints effectively.
 *  The minimal possible size of heap is when it can store at least 1 byte,
 *  otherwise iniitialization fails and allocator constructor returns 'null'.
 *  If allocation is impossible, allocator returns null.
 *
 ***/


#define ALIGN 16

static inline uint aligned(uint addr) {
    if (0 == (addr & (ALIGN - 1))) return addr;
    return ALIGN + (addr & ~(ALIGN - 1));
}

#define USED ((uint)0x80000000)

struct ff_chunk_info {
    /** double-linked circular list
       chunk pointers are always ALIGN * n - CHUNK_SIZE **/
    struct ff_chunk_info *next;
    struct ff_chunk_info *prev;

    /** the highest bit is free/used bit, all other = checksum
        checksum = this + length;    **/
    uint checksum;
};

struct firstfit_allocator {
    uint startmem;
    uint endmem;
    struct ff_chunk_info *current;
};

typedef struct firstfit_allocator alloc_t;
typedef struct ff_chunk_info chunk_t;

#define CHUNK_SIZE sizeof(chunk_t)
#define ALLOC_SIZE sizeof(alloc_t)


static inline void set_used(chunk_t *this) {
    this->checksum = (uint)(this->next) + (uint)this;
    this->checksum |= USED; }

static inline void set_free(chunk_t *this) {
    this->checksum = (uint)(this->next) + (uint)this;
    this->checksum &= ~USED; }

static inline bool is_used(chunk_t *this) {
    /* I do have a reason not to write 'return ();'! */
    if (USED & this->checksum) return true;
    else return false; }

static inline bool check_sum(chunk_t *this) {
    return ((USED | ((uint)this + (uint)this->next)) ==
            (USED | this->checksum));
}

static inline chunk_t *next(chunk_t *this) {
    return this->next; }

static inline chunk_t *prev(chunk_t *this) {
    return this->prev;  }

static inline void set_prev(chunk_t *this, chunk_t *prev) {
    this->prev = prev;  }

static inline void set_next(chunk_t *this, chunk_t *next) {
    this->next = next;  }

static inline size_t get_size(chunk_t *this) {
    return (uint)this->next - (uint)this - CHUNK_SIZE;  }

static inline void *chunk_data(chunk_t *this) {
    return (void *)aligned((uint)this); }

static inline void erase(chunk_t *this) {
    this->checksum = 0;
}

static inline chunk_t *set_chunk(chunk_t *this, chunk_t *next, chunk_t *prev, bool used) {
    this->next = next;
    this->prev = prev;
    if (used) set_used(this);
    else set_free(this);
    return this;
}


void try_to_repair(chunk_t *chunk) {
    //
}


struct firstfit_allocator *
firstfit_new(void *startmem, size_t size) {
    if (size < aligned(ALLOC_SIZE + CHUNK_SIZE) + CHUNK_SIZE + 1)
        return null;    /** not enough room even for 1 byte **/

    alloc_t * this = (alloc_t *)startmem;
    this->startmem = (uint)startmem;
    this->endmem = size + (uint)startmem;
    chunk_t *initial = (chunk_t *)
            (aligned(this->startmem + ALLOC_SIZE + CHUNK_SIZE) - CHUNK_SIZE);
    chunk_t *heap_end = (chunk_t *)
            (ALIGN * (this->endmem / ALIGN) - CHUNK_SIZE);
    set_chunk(initial, heap_end, heap_end, false);
    set_chunk(heap_end, initial, initial, true);    // never merge

    this->current = initial;
    return this;
}

void * firstfit_malloc(struct firstfit_allocator *this, uint size) {
    if ((size == 0) || (size > MAX_INT)) return null;

    chunk_t *chunk = this->current;
    do {
        if (!is_used(chunk) && ((int)get_size(chunk) >= (int)size)) {
            /** allocate this chunk **/
            // is there enough place for a new chunk after this one?
            if ((get_size(chunk) - size) >= aligned(CHUNK_SIZE)) {
                // possible position of new chunk counting from current+CHUNK_SIZE
                uint new_chunk_offset = aligned(size + CHUNK_SIZE) - CHUNK_SIZE;

                /** yes, split **/
                chunk_t *new_chunk =
                    (chunk_t *)( (uint)chunk_data(chunk) + new_chunk_offset );

                set_prev(next(chunk), new_chunk);
                set_chunk(new_chunk, next(chunk), chunk, false);
                set_chunk(chunk, new_chunk, prev(chunk), true);
            }

            this->current = next(chunk);
            set_used(chunk);
            return chunk_data(chunk);
        }

        chunk = next(chunk);
        if (! check_sum(chunk)) {
            logef("Heap corruption at *0x%x ", (ptr_t)chunk);
            try_to_repair(chunk);
            return null;
        }

    } while (chunk != this->current);

    /** full cycle over chunks? **/
    return null;   // no memory
}

void firstfit_free(struct firstfit_allocator *this, void *p) {
    assertv(p, "firstfit warning: trying to deallocate null");

    chunk_t *chunk = (chunk_t *)((uint)p - CHUNK_SIZE);
    if (! check_sum(chunk)) {
        try_to_repair(chunk);
        return;
    }

    set_free(chunk);

    chunk_t *next_chunk = next(chunk);
    if (!is_used(next_chunk)) {
        // merge
        set_chunk(chunk, next(next_chunk), prev(chunk), false);
        erase(next_chunk);
    }

    chunk_t *prev_chunk = prev(chunk);
    if (!is_used(prev_chunk)) {
        // merge
        set_chunk(prev_chunk, next(chunk), prev(prev_chunk), false);
        erase(chunk);
        chunk = prev_chunk;
    }

    this->current = chunk;
}

void *firstfit_corruption(struct firstfit_allocator *this) {
    chunk_t *c = this->current;
    do if (! check_sum(c)) return c;
    while ((c = next(c)) != this->current);
    return null;
}

#include <log.h>

void heap_info(struct firstfit_allocator *this) {
    printf("header(*%x): startmem = 0x%x, endmem = 0x%x, current = 0x%x\n",
            (uint)this, (uint)this->startmem, (uint)this->endmem, (uint)this->current);
    chunk_t *cur = this->current;
    do {
        printf("  0x%x %s (0x%x : 0x%x) [0x%x]\n",
              (uint)cur, (is_used(cur) ? "used" : "free"),
              CHUNK_SIZE + (uint)cur,
              (uint)next(cur), get_size(cur));
        cur = next(cur);
        if (! check_sum(cur)) {
            printf("HEAP ERROR: Invalid checksum, heap corruption at 0x%x\n", cur);
            return;
        }

    } while (cur != this->current);
}
