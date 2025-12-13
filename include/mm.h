/**
C版本的内存管理器头文件。
*/
#include "utils.h"
void* mm_malloc(size_t size, size_t alignment);
void mm_free(void* ptr);
void* mm_realloc(void* ptr, size_t size);