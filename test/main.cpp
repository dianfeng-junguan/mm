#include "mm.h"
#include <stdio.h>
#include <stdlib.h>
void* bulk_alloc(size_t size)
{
    return malloc(size);
}
void bulk_free(void* ptr, size_t size)
{
    free(ptr);
}
int main()
{
    printf("Test\n");
    char* mem = (char*)mm_malloc(128, 1);
    printf("alloced mem 128bytes with result %p\n", mem);
    printf("alloced mem 128bytes with alignment 128 bytes with result %p\n", mm_malloc(128, 128));
    mm_free(mem);
    return 0;
}