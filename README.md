[English](#english) | [中文](#中文)

<div id="english">

# Memory Manager

A custom memory management library based on slab allocator.

## Key File Documentation

### `mm.cpp`

This file contains the core implementation of the general-purpose memory allocator.

-   `void* mm_malloc(size_t size, size_t alignment)`: Allocates a memory block of at least `size` bytes from the heap. Returns a pointer to the allocated block, or `NULL` if the request fails.
-   `void mm_free(void* ptr)`: Frees a previously allocated memory block pointed to by `ptr`.

### `slab.cpp`

This file implements a slab allocator.

## Reminder

The allocator by default uses a global slab_cache array. If you want to maintain your own slab_cache array, define NO_GLOBAL_SLAB_CACHE_ARRAY while compiling.
</div>

---

<div id="中文">

# 内存管理器

本项目是一个自定义的内存管理库，包含一个通用分配器和一个 slab 分配器。

## 关键文件文档

### `mm.cpp`

该文件包含通用内存分配器的核心实现。

-   `void* mm_malloc(size_t size,size_t alignment)`: 从堆中分配一个至少为 `size` 字节的内存块。返回指向已分配块的指针，如果请求失败则返回 `NULL`。
-   `void mm_free(void* ptr)`: 释放由 `ptr` 指向的先前分配的内存块。

### `slab.cpp`

该文件实现了一个 slab 分配器，可高效地分配和释放相同大小的对象。

## 注意事项

分配器默认维护一个全局的slab_cache数组，如果你需要自己维护数组，可以通过定义NO_GLOBAL_SLAB_CACHE_ARRAY来取消全局slab_cache数组。

</div>