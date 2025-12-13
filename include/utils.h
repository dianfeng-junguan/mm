typedef unsigned long long size_t;

#ifdef _DEBUG
#include <stdio.h>
#define LOG(fmt, ...) printf(fmt, ##__VA_ARGS__);
#else
#define LOG(fmt, ...)
#endif