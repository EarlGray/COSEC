#include <mm_firstfit.h>

#include <defs.h>

#ifndef __MISC_H
typedef unsigned uint, size_t;
typedef char bool;
#define true    1
#define false   0
#define null ((void *)0)
#endif

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

#define USED ((uint)0x80000000)

struct ff_chunk_info {
    /* double-linked circular list 
       chunk pointers are always ALIGN * n - CHUNK_SIZE */
    struct ff_chunk_info *next;
    struct ff_chunk_info *prev;

    /* the highest bit is free/used bit, all other = checksum
        checksum = this + length;    */  
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

static inline size_t get_size(chunk_t *this) {
    return (uint)this->next - ((uint)this + CHUNK_SIZE);  }

static inline void *chunk_data(chunk_t *this) {
    return (void *)((uint)this + CHUNK_SIZE); }

static inline void erase(chunk_t *this) {
    this->next = this->prev = 0;
}

static inline chunk_t *set_chunk(chunk_t *this, chunk_t *next, chunk_t *prev, bool used) {
    this->next = next;
    this->prev = prev;
    if (used) set_used(this);
    else set_free(this);
    return this;
}

#define ALIGN 16

static inline uint aligned(uint addr) {
    if (0 == (addr & (ALIGN - 1))) return addr;
    return ALIGN + (addr & ~(ALIGN - 1));
}

void try_to_repair(chunk_t *chunk) {
    // 
}


struct firstfit_allocator * 
firstfit_new(void *startmem, size_t size) {
    if (size < aligned(ALLOC_SIZE + CHUNK_SIZE) + CHUNK_SIZE + 1) 
        return null;    /* not enough room even for 1 byte */

    alloc_t * this = (alloc_t *)startmem;
    this->startmem = (uint)startmem;
    this->endmem = size + (uint)startmem;
    chunk_t *initial = (chunk_t *)
            (aligned(this->startmem + ALLOC_SIZE + CHUNK_SIZE) - CHUNK_SIZE);
    chunk_t *heap_end = (chunk_t *)
            ((uint)this->endmem - CHUNK_SIZE);
    set_chunk(initial, heap_end, heap_end, false);
    set_chunk(heap_end, initial, initial, true);    // never merge 
    
    this->current = initial;
    return this;
}

void * firstfit_malloc(struct firstfit_allocator *this, uint size) {
    if (size == 0) return null;

    chunk_t *chunk = this->current;
    while (true) {
        if (!is_used(chunk) && (((int)get_size(chunk) - (int)size) >= 0)) {  
            /* allocate this chunk */
            // possible position of new chunk counting from current + CHUNK_SIZE
            uint new_chunk_offset = aligned(size);
            if (new_chunk_offset - CHUNK_SIZE < size) 
                new_chunk_offset += aligned(CHUNK_SIZE);

            // is there enough place
            if ((new_chunk_offset + 1) < get_size(chunk)) {
                /* yes, split */
                chunk_t *new_chunk = 
                    (chunk_t *)( (uint)chunk + new_chunk_offset );
                    // without CHUNK_SIZE, (((chunk+CHUNK_SIZE) + new_chunk_offset) - CHUNK_SIZE)

                set_chunk(new_chunk, chunk->next, chunk, false);
                set_chunk(chunk, new_chunk, prev(chunk), true);
                
                this->current = new_chunk;
                return chunk_data(chunk);
            }

            set_used(chunk);

            this->current = chunk;
            return chunk_data(chunk);
        }

        chunk = next(chunk);
        if (! check_sum(chunk)) {
            try_to_repair(chunk);
            return null;
        }

        /* full cycle over chunks */
        if (chunk == this->current)
            return null;   /* no memory */
    }
}

void firstfit_free(struct firstfit_allocator *this, void *p) {
    if (p == null) return;

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

void heap_info(struct firstfit_allocator *this) {
    k_printf("header: \tstartmem = 0x%x\tendmem = 0x%x\tcurrent = 0x%x\n",
            (uint)this->startmem, (uint)this->endmem, (uint)this->current);
    chunk_t *cur = this->current;
    do {
        k_printf("    chunk at 0x%.8x:\t(0x%.8x : 0x%.8x) [0x%.8x] ; used = %d\n",
              (uint)cur, CHUNK_SIZE + (uint)cur, (uint)next(cur), get_size(cur), (uint)is_used(cur));
        cur = next(cur);
        if (! check_sum(cur)) {
            k_printf("HEAP ERROR: Invalid checksum, heap corruption\n");
            return;
        }

    } while (cur != this->current);
}
