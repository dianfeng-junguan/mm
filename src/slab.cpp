#include "slab.h"

#define NULPTR ((void*)0)
#define ALIGN_UP(v, alignment) (((v) + (alignment)-1) & ~((alignment)-1))
//temp code
extern void* bulk_alloc(size_t size);
extern void bulk_free(void* ptr, size_t size);

void slab_cache_init(struct slab_cache* cache, size_t object_size, size_t alignment, void (*ctor)(void*, size_t), void (*dtor)(void*, size_t))
{
    cache->object_size = object_size;
    cache->alignment = alignment;
    size_t aligned_object_size = ALIGN_UP(object_size, alignment);
    // calculate how many objects can fit in one slab
    // because we need one "short" for each object in the freelist
    // that equals every object needs (aligned_object_size + sizeof(short)) bytes

    cache->objects_num_per_slab = (SLAB_SIZE - sizeof(struct slab)) / (aligned_object_size + sizeof(short));
    cache->slabs_full = (struct slab*)NULPTR;
    cache->slabs_partial = (struct slab*)NULPTR;
    cache->slabs_empty = (struct slab*)NULPTR;
    cache->ctor = ctor;
    cache->dtor = dtor;
}

struct slab* create_slab(struct slab_cache* cache)
{
    size_t aligned_object_size = ALIGN_UP(cache->object_size, cache->alignment);

    // 1. Calculate required size
    // We need space for the slab metadata, all the objects, AND the padding needed for alignment.
    // The maximum padding needed is (alignment - 1).
    size_t objects_total_size = aligned_object_size * cache->objects_num_per_slab;
    size_t freelist_array_size = sizeof(short) * cache->objects_num_per_slab;
    size_t metadata_size = sizeof(struct slab) + freelist_array_size;
    size_t slab_size = metadata_size + objects_total_size + (cache->alignment - 1);

    void* slab_mem = bulk_alloc(slab_size);
    if (slab_mem == NULPTR) {
        return (struct slab*)NULPTR;
    }

    struct slab* new_slab = (struct slab*)slab_mem;

    // 2. Calculate the aligned starting address for the objects area.
    // This is the first address after the slab metadata that is a multiple of `cache->alignment`.
    void* objects_start = (void*)ALIGN_UP((unsigned long long)slab_mem + metadata_size, cache->alignment);
    new_slab->mem_ptr = objects_start;

    // setting up freelist pointer
    new_slab->freelist = (short*)((unsigned long long)slab_mem + sizeof(struct slab));
    new_slab->active = 0;
    // initialize freelist
    for (short i = 0; i < cache->objects_num_per_slab; i++) {
        new_slab->freelist[i] = i;
    }

    new_slab->next = (struct slab*)NULPTR;
    new_slab->prev = (struct slab*)NULPTR;

    return new_slab;
}

void* alloc_memory_block(struct slab* slab, struct slab_cache* cache)
{
    if (slab->active >= slab->freelist[0] + slab->freelist[1]) {
        // slab is full
        return NULPTR;
    }
    short index = slab->freelist[slab->active];
    slab->active++;
    size_t aligned_object_size = ALIGN_UP(cache->object_size, cache->alignment);
    void* block_ptr = (void*)((unsigned long long)slab->mem_ptr + index * aligned_object_size);
    if (cache->ctor) {
        cache->ctor(block_ptr, cache->object_size);
    }
    return block_ptr;
}

void free_memory_block(struct slab* slab, struct slab_cache* cache, void* ptr)
{
    size_t aligned_object_size = ALIGN_UP(cache->object_size, cache->alignment);
    short index = (short)(((unsigned long long)ptr - (unsigned long long)slab->mem_ptr) / aligned_object_size);
    slab->active--;
    slab->freelist[slab->active] = index;
    if (cache->dtor) {
        cache->dtor(ptr, cache->object_size);
    }
}

void* slab_alloc(size_t size, size_t alignment, struct slab_cache* cache_array, size_t cache_array_size)
{
    //find a suitable slab cache
    struct slab_cache* target_cache = (struct slab_cache*)NULPTR;
    for (size_t i = 0; i < cache_array_size; i++) {
        if (cache_array[i].object_size >= size && cache_array[i].alignment >= alignment) {
            target_cache = &cache_array[i];
            break;
        }
    }
    if (target_cache == NULPTR) {
        return NULPTR;
    }
    //try to find a partial slab first
    struct slab* target_slab = target_cache->slabs_partial;
    if (target_slab == NULPTR) {
        //try to find an empty slab
        target_slab = target_cache->slabs_empty;
        if (target_slab == NULPTR) {
            //create a new slab
            target_slab = create_slab(target_cache);
            if (target_slab == NULPTR) {
                return NULPTR;
            }
            //insert the new slab into the partial list, because we are going to allocate from it
            PTRLIST_INSERT(&target_cache->slabs_partial, target_slab);
        } else {
            //move the slab from partial to full if needed
            if (target_slab->active == (int)(target_cache->objects_num_per_slab - 1)) {
                PTRLIST_DROP(target_slab);
                PTRLIST_INSERT(&target_cache->slabs_full, target_slab);
            }
        }
    }

    void* block_ptr = alloc_memory_block(target_slab, target_cache);
    return block_ptr;
}

void slab_free(void* ptr, struct slab_cache* cache_array, size_t cache_array_size)
{
    //find which slab this ptr belongs to
    for (size_t i = 0; i < cache_array_size; i++) {
        struct slab_cache* cache = &cache_array[i];
        //search in partial slabs
        struct slab* slab = cache->slabs_partial;
        while (slab) {
            unsigned long long slab_start = (unsigned long long)slab;
            unsigned long long slab_end = slab_start + SLAB_SIZE;
            if ((unsigned long long)ptr >= slab_start && (unsigned long long)ptr < slab_end) {
                free_memory_block(slab, cache, ptr);
                return;
            }
            slab = PTRLIST_NEXT(slab);
        }
        //search in full slabs
        slab = cache->slabs_full;
        while (slab) {
            unsigned long long slab_start = (unsigned long long)slab;
            unsigned long long slab_end = slab_start + SLAB_SIZE;
            if ((unsigned long long)ptr >= slab_start && (unsigned long long)ptr < slab_end) {
                free_memory_block(slab, cache, ptr);
                return;
            }
            slab = PTRLIST_NEXT(slab);
        }
    }
}