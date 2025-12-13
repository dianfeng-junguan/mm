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
    // The slab is full, cannot allocate.
    if (slab->active >= cache->objects_num_per_slab) {
        return NULPTR;
    }
    // Get the index of the next free object from the freelist.
    // The freelist is used as a stack, 'active' points to the top.
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

    // Push the freed index back onto the freelist stack.
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
        // Find the first cache that is large enough. Assumes cache_array is sorted by size.
        if (cache_array[i].object_size >= size && cache_array[i].alignment >= alignment) {
            target_cache = &cache_array[i];
            break;
        }
    }
    if (target_cache == NULPTR) {
        // No suitable cache found. In a real system, you might create a new cache
        // or fallback to a different allocator. Here we just fail.
        return NULPTR;
    }

    struct slab* target_slab = (struct slab*)NULPTR;

    // 1. Try to use a partially full slab first.
    if (target_cache->slabs_partial) {
        target_slab = target_cache->slabs_partial;
    }
    // 2. If no partial slabs, try to use an empty slab.
    else if (target_cache->slabs_empty) {
        target_slab = target_cache->slabs_empty;
        // This slab was empty, now it will be partial. Move it.
        PTRLIST_DROP(target_slab);
        target_cache->slabs_empty = target_slab->next;
        PTRLIST_INSERT(&target_cache->slabs_partial, target_slab);
    }
    // 3. If no partial and no empty slabs, create a new one.
    else {
        target_slab = create_slab(target_cache);
        if (target_slab == NULPTR) {
            return NULPTR; // Out of memory
        }
        // The new slab is immediately partial because we are about to allocate from it.
        PTRLIST_INSERT(&target_cache->slabs_partial, target_slab);
    }

    void* block_ptr = alloc_memory_block(target_slab, target_cache);

    // After allocation, check if the slab has become full.
    if (target_slab->active == target_cache->objects_num_per_slab) {
        PTRLIST_DROP(target_slab);
        target_cache->slabs_partial = target_slab->next;
        PTRLIST_INSERT(&target_cache->slabs_full, target_slab);
    }

    return block_ptr;
}

void slab_free(void* ptr, struct slab_cache* cache_array, size_t cache_array_size)
{
    // A simple, but inefficient way to find the slab.
    // A better way is to store a pointer to the slab or cache in the object's metadata
    // or use page alignment tricks to find the slab metadata.
    for (size_t i = 0; i < cache_array_size; i++) {
        struct slab_cache* cache = &cache_array[i];
        size_t aligned_object_size = ALIGN_UP(cache->object_size, cache->alignment);
        size_t objects_total_size = aligned_object_size * cache->objects_num_per_slab;

        // --- Search in partial slabs ---
        struct slab* slab = cache->slabs_partial;
        while (slab) {
            unsigned long long objects_start = (unsigned long long)slab->mem_ptr;
            unsigned long long objects_end = objects_start + objects_total_size;
            if ((unsigned long long)ptr >= objects_start && (unsigned long long)ptr < objects_end) {
                int active_before = slab->active;
                free_memory_block(slab, cache, ptr);
                // If the slab was full and now has a free spot, move it to partial.
                // (This case is handled below, as it must have been in the full list before)

                // If the slab is now completely empty, move it to the empty list.
                if (slab->active == 0) {
                    PTRLIST_DROP(slab);
                    LOG("[LOG] slab moved to empty\n");
                    if (cache->slabs_partial == slab) {
                        cache->slabs_partial = slab->next;
                    }
                    PTRLIST_INSERT(&cache->slabs_empty, slab);
                }
                return;
            }
            slab = PTRLIST_NEXT(slab);
        }

        // --- Search in full slabs ---
        slab = cache->slabs_full;
        while (slab) {
            unsigned long long objects_start = (unsigned long long)slab->mem_ptr;
            unsigned long long objects_end = objects_start + objects_total_size;
            if ((unsigned long long)ptr >= objects_start && (unsigned long long)ptr < objects_end) {
                int active_before = slab->active;
                free_memory_block(slab, cache, ptr);
                // If the slab was full and now has a free spot, move it to partial.
                if (active_before == cache->objects_num_per_slab) {
                    PTRLIST_DROP(slab);
                    if (cache->slabs_full == slab) {
                        cache->slabs_full = slab->next;
                    }
                    PTRLIST_INSERT(&cache->slabs_partial, slab);
                }
                return;
            }
            slab = PTRLIST_NEXT(slab);
        }
    }
}