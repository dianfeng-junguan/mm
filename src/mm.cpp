#include "mm.h"
#include "slab.h"

static void mm_memcpy(void* dest, const void* src, size_t n)
{
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}
static void mm_memset(void* dest, int value, size_t n)
{
    unsigned char* d = (unsigned char*)dest;
    for (size_t i = 0; i < n; i++) {
        d[i] = (unsigned char)value;
    }
}
static void default_ctor(void* ptr, size_t size)
{
    mm_memset(ptr, 0, size);
}
static void default_dtor(void* ptr, size_t size)
{
    mm_memset(ptr, 0, size);
}
#ifndef NO_GLOBAL_SLAB_CACHE_ARRAY
#define MAX_SLAB_CACHES 10
struct slab_cache global_slab_cache_array[MAX_SLAB_CACHES] = {
    //object size: 8,16,32,64,128,256,512,1024,2048,4096
    //alignment: 8 for all
    // { 8, 8, SLAB_SIZE / 8, nullptr, nullptr, nullptr, default_ctor, default_dtor },
    // { 16, 8, SLAB_SIZE / 16, nullptr, nullptr, nullptr, default_ctor, default_dtor },
    // { 32, 8, SLAB_SIZE / 32, nullptr, nullptr, nullptr, default_ctor, default_dtor },
    // { 64, 8, SLAB_SIZE / 64, nullptr, nullptr, nullptr, default_ctor, default_dtor },
    // { 128, 8, SLAB_SIZE / 128, nullptr, nullptr, nullptr, default_ctor, default_dtor },
    // { 256, 8, SLAB_SIZE / 256, nullptr, nullptr, nullptr, default_ctor, default_dtor },
    // { 512, 8, SLAB_SIZE / 512, nullptr, nullptr, nullptr, default_ctor, default_dtor },
    // { 1024, 8, SLAB_SIZE / 1024, nullptr, nullptr, nullptr, default_ctor, default_dtor },
    // { 2048, 8, SLAB_SIZE / 2048, nullptr, nullptr, nullptr, default_ctor, default_dtor },
    // { 4096, 8, SLAB_SIZE / 4096, nullptr, nullptr, nullptr, default_ctor, default_dtor }
};
#endif
#ifndef NO_GLOBAL_SLAB_CACHE_ARRAY
void* mm_malloc(size_t size, size_t alignment)
#else
void* mm_malloc(size_t size, size_t alignment, struct slab_cache* cache_array, size_t cache_array_size)
#endif
{
    struct slab_cache* cache_array_ptr =
#ifndef NO_GLOBAL_SLAB_CACHE_ARRAY
        global_slab_cache_array;
#else
        cache_array;
#endif
    size_t cache_array_size_val =
#ifndef NO_GLOBAL_SLAB_CACHE_ARRAY
        MAX_SLAB_CACHES;
#else
        cache_array_size;
#endif
    if (alignment == 0) {
        LOG("warning: passed alignment=0 while mm_alloc-ing. If you do not need alignment, pass alignment=1.\n Now automatically setting it to 1.\n")
        alignment = 1;
    }
    void* res = slab_alloc(size, alignment, cache_array_ptr, cache_array_size_val);
    if (!res) {
        LOG("first attempt allocing failed. trying to create a new cache...\n");
        //trying to create a slab cache meeting the requirements
        struct slab_cache* temp_cache;
        for (size_t i = 0; i < cache_array_size_val; i++) {
            if (cache_array_ptr[i].object_size == 0) {
                temp_cache = &cache_array_ptr[i];
                break;
            }
        }
        //ascending sort by the object_size
        //bubble sort
        for (size_t i = 0; i < cache_array_size_val - 1; i++) {
            for (size_t j = 0; j < cache_array_size_val - i - 1; j++) {
                if (cache_array_ptr[j].object_size > cache_array_ptr[j + 1].object_size) {
                    struct slab_cache temp = cache_array_ptr[j];
                    cache_array_ptr[j] = cache_array_ptr[j + 1];
                    cache_array_ptr[j + 1] = temp;
                }
            }
        }
        slab_cache_init(temp_cache, size, alignment, default_ctor, default_dtor);
        //alloc again
        return slab_alloc(size, alignment, cache_array_ptr, cache_array_size_val);
    } else {
        return res;
    }
}
#ifndef NO_GLOBAL_SLAB_CACHE_ARRAY
void mm_free(void* ptr)
#else
void mm_free(void* ptr, struct slab_cache* cache_array, size_t cache_array_size)
#endif
{
#ifndef NO_GLOBAL_SLAB_CACHE_ARRAY
    slab_free(ptr, global_slab_cache_array, MAX_SLAB_CACHES);
#else
    slab_free(ptr, cache_array, cache_array_size);
#endif
}
void* mm_realloc(void* ptr, size_t size, size_t alignment
#ifdef NO_GLOBAL_SLAB_CACHE_ARRAY
    ,
    struct slab_cache* cache_array,
    size_t cache_array_size
#endif
)
{
#ifndef NULL
#define NULL (void*)0
#endif
    if (!ptr) {
        return mm_malloc(size, alignment
#ifdef NO_GLOBAL_SLAB_CACHE_ARRAY
            ,
            cache_array,
            cache_array_size
#endif
        );
    } else {
        //simple implementation: alloc new memory and copy old data
        void* new_ptr = mm_malloc(size, alignment
#ifdef NO_GLOBAL_SLAB_CACHE_ARRAY
            ,
            cache_array,
            cache_array_size
#endif
        );
        if (!new_ptr) {
            return NULL;
        }
        //copy old data
        //reminder: we dont know the size of the old allocation, so we just copy size bytes
        //this may cause memory corruption if the new size is smaller than the old size

        mm_memcpy(new_ptr, ptr, size);
        mm_free(ptr);
        return new_ptr;
    }
}