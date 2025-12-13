#include "ptrlist.h"
#include "utils.h"
/**
a slab mem struct is like this:
|| struct slab | freelist | mem blocks ||
freelist is an FILO array that holds the index of the mem blocks.
when to malloc, freelist[active++] is returned.
when to free, freelist[--active] = index of the freed block.
thus we can get a hot memblock(a block that was recently used).
*/
struct slab {
    int active;
    // pointer to freelist array. we use short here because we wont make a slab larger than 4kb.
    short* freelist;
    // pointer used in the freelist
    // pointer to the start of the memory block that actually holds the objects
    void* mem_ptr;
    PTRLIST_DEF(struct slab)
};

struct slab_cache {
    size_t object_size;
    //alignment of memory address of objects in this slab cache
    size_t alignment;
    size_t objects_num_per_slab;
    struct slab* slabs_full;
    struct slab* slabs_partial;
    struct slab* slabs_empty;
    void (*ctor)(void* ptr, size_t size);
    void (*dtor)(void* ptr, size_t size);
};
#define SLAB_SIZE 4096
/**
init the slab cache array item.
*/
void slab_cache_init(struct slab_cache* cache, size_t object_size, size_t alignment, void (*ctor)(void*, size_t), void (*dtor)(void*, size_t));

/**
alloc a slab from system memory and initialize it for the given slab cache.
*/
struct slab* creat_slab(struct slab_cache* cache);

/**
alloc a memory block from the given slab.
*/
void* alloc_memory_block(struct slab* slab);
/**
put a memory block back to the given slab.
*/
void free_memory_block(void* ptr);

/*
alloc memory from slab allocator.

## reminder: the caller should ensure that the cache_array is sorted by object_size ascendingly.
*/
void* slab_alloc(size_t size, size_t alignment, struct slab_cache* cache_array, size_t cache_array_size);
void slab_free(void* ptr, struct slab_cache* cache_array, size_t cache_array_size);
