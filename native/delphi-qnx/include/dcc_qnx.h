#ifndef DCC_QNX_H
#define DCC_QNX_H

#include <stddef.h>
#include <stdint.h>

typedef int ssize_t;

#define RTLD_LAZY 0x0001
#define RTLD_GLOBAL 0x0100
#define O_WRONLY 0x0001
#define O_CREAT 0x0100
#define O_TRUNC 0x0200

int open(const char *, int, ...);
int close(int);
ssize_t write(int, const void *, size_t);
void *dlopen(const char *, int);
void *dlsym(void *, const char *);
void *memcpy(void *, const void *, size_t);
void *memset(void *, int, size_t);
size_t strlen(const char *);

#endif
