/**
C版本的内存管理器头文件。
*/
#include "utils.h"
#ifndef NO_GLOBAL_SLAB_CACHE_ARRAY
void *mm_malloc(size_t size, size_t alignment);

void mm_free(void *ptr);

void *mm_realloc(void *ptr, size_t size, size_t alignment);

#else
void *mm_malloc(size_t size, size_t alignment, struct slab_cache *cache_array,
                size_t cache_array_size);

void mm_free(void *ptr, struct slab_cache *cache_array,
             size_t cache_array_size);

void *mm_realloc(void *ptr, size_t size, size_t alignment,
                 struct slab_cache *cache_array, size_t cache_array_size);
#endif