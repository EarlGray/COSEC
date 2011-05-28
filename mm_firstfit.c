#include <mm_firstfit.h>

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
typedef uint8_t byte;

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
    byte *startmem;
    byte *endmem;
    struct ff_chunk_info *current;
};

typedef struct firstfit_allocator alloc_t;
typedef struct ff_chunk_info chunk_t;

#define CHUNK_SIZE sizeof(chunk_t)
#define ALLOC_SIZE sizeof(alloc_t)


static inline void set_used(chunk_t *this) {
    this->checksum |= USED; }

static inline void set_free(chunk_t *this) {
    this->checksum &= ~USED; }

static inline bool is_used(chunk_t *this) {
    return USED & this->checksum; }

static inline chunk_t *next(chunk_t *this) {
    return this->next; }

static inline chunk_t *prev(chunk_t *this) {
    return this->prev;  }

static inline size_t get_size(chunk_t *this) {
    return (byte *)this->next - (byte *)this + CHUNK_SIZE;  }

static inline void *chunk_data(chunk_t *this) {
    return (void *)((byte*)this + CHUNK_SIZE); }

static inline chunk_t *set_chunk(chunk_t *this, chunk_t *next, chunk_t *prev, bool used) {
    this->next = next;
    this->prev = prev;
    if (used) set_used(this);
    else set_free(this);
    return this;
}

#define ALIGN 16

static inline uint aligned(uint addr) {
    if (0 == (addr % ALIGN)) return addr;
    return ALIGN + (addr & ~ALIGN);
}


struct firstfit_allocator * 
firstfit_new(void *startmem, size_t size) {
    if (size < aligned(ALLOC_SIZE + CHUNK_SIZE) + 1) 
        return null;    /* not enough room even for 1 byte */

    alloc_t * this = (alloc_t *)startmem;
    this->startmem = (byte *)startmem;
    this->endmem = size + (byte *)startmem;
    chunk_t *initial = (chunk_t *)
            ((byte *)this->startmem + aligned(ALLOC_SIZE + CHUNK_SIZE) - CHUNK_SIZE);
    set_chunk(initial, initial, initial, false);
    
    this->current = initial;
    return this;
}

void * firstfit_malloc(struct firstfit_allocator *this, uint size) {
    if (size == 0) return;

    chunk_t *chunk = this->current;
    while (true) {
        if (!is_used(chunk) && ((get_size(chunk) - size) > 0)) {  
            /* allocate this chunk */
            // possible position of new chunk counting from current + CHUNK_SIZE
            uint new_chunk_offset = aligned(size);
            if (new_chunk_offset - CHUNK_SIZE < size) 
                new_chunk_offset += aligned(CHUNK_SIZE);

            // is there enough place
            if ((new_chunk_offset + 1) < get_size(chunk)) {
                /* yes, split */
                chunk_t *new_chunk = 
                    (chunk_t *)( (byte *)chunk + new_chunk_offset );
                    // without CHUNK_SIZE, (((chunk+CHUNK_SIZE) + new_chunk_offset) - CHUNK_SIZE)

                set_chunk(chunk, new_chunk, prev(chunk), true);
                set_chunk(new_chunk, chunk->next, chunk, false);
                
                this->current = new_chunk;
                return chunk_data(chunk);
            }

            set_used(chunk);

            this->current = chunk;
            return chunk_data(chunk);
        }

        chunk = next(chunk);

        /* full cycle over chunks */
        if (chunk == this->current)
            return null;   /* no memory */
    }
}

void firstfit_free(struct firstfit_allocator *this, void *p) {
    if (p == null) return;

    // DRAFT

    chunk_t *chunk = (chunk_t *)((byte *p) - CHUNK_SIZE);
}
