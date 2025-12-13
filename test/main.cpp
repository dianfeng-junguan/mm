#include "mm.h"
#include "slab.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mock implementations for bulk allocation for testing purposes
void* bulk_alloc(size_t size)
{
    return malloc(size);
}

void bulk_free(void* ptr, size_t size)
{
    (void)size; // size is unused in this mock
    free(ptr);
}

// --- Test Helper Functions ---
void ctor_test(void* ptr, size_t size)
{
    printf("CTOR called for object at %p, size %zu\n", ptr, size);
    memset(ptr, 0xAA, size); // Fill with a pattern
}

void dtor_test(void* ptr, size_t size)
{
    printf("DTOR called for object at %p, size %zu\n", ptr, size);
    memset(ptr, 0xDD, size); // Fill with a different pattern
}

void test_basic_alloc_free()
{
    printf("\n--- Test: Basic Allocation and Free ---\n");
    struct slab_cache cache;
    slab_cache_init(&cache, 128, 8, ctor_test, dtor_test);

    void* p1 = slab_alloc(128, 8, &cache, 1);
    assert(p1 != NULL);
    printf("Allocated p1: %p\n", p1);

    // Check if ctor was called (memory should be 0xAA)
    assert(*(unsigned char*)p1 == 0xAA);

    slab_free(p1, &cache, 1);
    printf("Freed p1: %p\n", p1);

    // Check if dtor was called (memory should be 0xDD)
    assert(*(unsigned char*)p1 == 0xDD);

    printf("Basic alloc/free test PASSED.\n");
}

void test_slab_lifecycle()
{
    printf("\n--- Test: Slab Lifecycle (Partial -> Full -> Empty) ---\n");
    struct slab_cache cache;
    slab_cache_init(&cache, 32, 8, NULL, NULL);

    size_t num_objects = cache.objects_num_per_slab;
    void** ptrs = (void**)malloc(sizeof(void*) * num_objects);
    assert(ptrs != NULL);

    // 1. Allocate until slab is full
    printf("Allocating %zu objects to fill the slab...\n", num_objects);
    for (size_t i = 0; i < num_objects; ++i) {
        ptrs[i] = slab_alloc(32, 8, &cache, 1);
        assert(ptrs[i] != NULL);
        printf("  Allocated ptrs[%zu]: %p\n", i, ptrs[i]);
    }

    // At this point, the slab should be in the 'full' list
    assert(cache.slabs_partial == NULL);
    assert(cache.slabs_empty == NULL);
    assert(cache.slabs_full != NULL);
    printf("Slab is now in the 'full' list, as expected.\n");

    // 2. Try to allocate one more, should fail from this slab and create a new one
    void* p_extra = slab_alloc(32, 8, &cache, 1);
    assert(p_extra != NULL);
    printf("Allocated an extra object, creating a new slab: %p\n", p_extra);
    assert(cache.slabs_full != NULL); // Old slab is still full
    assert(cache.slabs_partial != NULL); // New slab is now partial

    // 3. Free one object from the full slab
    printf("Freeing one object (ptrs[0]) from the first slab...\n");
    slab_free(ptrs[0], &cache, 1);
    ptrs[0] = NULL;

    // The first slab should now move from 'full' to 'partial'
    assert(cache.slabs_full == NULL); // The first slab is no longer full
    assert(cache.slabs_partial != NULL);
    assert(PTRLIST_NEXT(cache.slabs_partial) != NULL); // Should have two partial slabs now
    printf("First slab moved from 'full' to 'partial' list.\n");

    // 4. Free all remaining objects from the first slab
    printf("Freeing remaining %zu objects from the first slab...\n", num_objects - 1);
    for (size_t i = 1; i < num_objects; ++i) {
        slab_free(ptrs[i], &cache, 1);
    }

    // The first slab should now be in the 'empty' list
    assert(cache.slabs_empty != NULL);
    printf("First slab moved to 'empty' list.\n");

    // 5. Free the extra object from the second slab
    printf("Freeing the object from the second slab...\n");
    slab_free(p_extra, &cache, 1);

    // The second slab should also move to the 'empty' list
    assert(cache.slabs_empty != NULL);
    assert(PTRLIST_NEXT(cache.slabs_empty) != NULL); // Two empty slabs
    assert(cache.slabs_partial == NULL);
    assert(cache.slabs_full == NULL);
    printf("Second slab also moved to 'empty' list.\n");

    free(ptrs);
    printf("Slab lifecycle test PASSED.\n");
}

void test_alignment()
{
    printf("\n--- Test: Alignment ---\n");
    const size_t alignment = 128;
    struct slab_cache cache;
    slab_cache_init(&cache, 256, alignment, NULL, NULL);

    void* p1 = slab_alloc(256, alignment, &cache, 1);
    assert(p1 != NULL);
    printf("Allocated p1 with %zu-byte alignment requirement: %p\n", alignment, p1);

    // Check alignment
    assert(((uintptr_t)p1 % alignment) == 0);
    printf("Address is correctly aligned.\n");

    slab_free(p1, &cache, 1);
    printf("Alignment test PASSED.\n");
}

void test_multiple_caches()
{
    printf("\n--- Test: Multiple Caches ---\n");
    const int NUM_CACHES = 3;
    struct slab_cache caches[NUM_CACHES];

    // Initialize caches for different sizes
    slab_cache_init(&caches[0], 16, 8, NULL, NULL); // Small
    slab_cache_init(&caches[1], 128, 8, NULL, NULL); // Medium
    slab_cache_init(&caches[2], 1024, 8, NULL, NULL); // Large

    // Allocate from each cache
    void* p_small = slab_alloc(16, 8, caches, NUM_CACHES);
    assert(p_small != NULL);
    printf("Allocated small object: %p\n", p_small);

    void* p_large = slab_alloc(1000, 8, caches, NUM_CACHES);
    assert(p_large != NULL);
    printf("Allocated large object: %p\n", p_large);

    void* p_medium = slab_alloc(100, 8, caches, NUM_CACHES);
    assert(p_medium != NULL);
    printf("Allocated medium object: %p\n", p_medium);

    // Free them
    slab_free(p_large, caches, NUM_CACHES);
    printf("Freed large object.\n");
    slab_free(p_small, caches, NUM_CACHES);
    printf("Freed small object.\n");
    slab_free(p_medium, caches, NUM_CACHES);
    printf("Freed medium object.\n");

    // All slabs should be in the empty list for their respective caches
    assert(caches[0].slabs_empty != NULL && caches[0].slabs_partial == NULL);
    assert(caches[1].slabs_empty != NULL && caches[1].slabs_partial == NULL);
    assert(caches[2].slabs_empty != NULL && caches[2].slabs_partial == NULL);

    printf("Multiple caches test PASSED.\n");
}

int main()
{
    printf("--- Starting Slab Allocator Tests ---\n");

    test_basic_alloc_free();
    test_alignment();
    test_slab_lifecycle();
    test_multiple_caches();

    printf("\n--- All tests completed successfully! ---\n");

    return 0;
}
