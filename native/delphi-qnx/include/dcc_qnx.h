#ifndef DCC_QNX_H
#define DCC_QNX_H

#include <stddef.h>
#include <stdint.h>

typedef int ssize_t;
typedef long off_t;
typedef unsigned pthread_t;

#define RTLD_LAZY 0x0001
#define RTLD_GLOBAL 0x0100
#define RTLD_DEFAULT ((void *)0)
#define RTLD_NEXT ((void *)-1)
#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR 0x0002
#define O_CREAT 0x0100
#define O_TRUNC 0x0200
#define SEEK_SET 0
#define SEEK_END 2
#define PROT_READ 0x00000100
#define PROT_WRITE 0x00000200
#define PROT_EXEC 0x00000400
#define MAP_SHARED 0x1
#define MAP_PRIVATE 0x2
#define MAP_FIXED 0x00000010
#define MAP_ANON 0x00080000
#define NOFD (-1)
#define MS_SYNC 0x00000002
#define MS_INVALIDATE_ICACHE 0x01000000
#define MS_CACHE_ONLY 0x02000000
#define MAP_FAILED ((void *)-1)

int open(const char *, int, ...);
int close(int);
ssize_t write(int, const void *, size_t);
struct iovec;
ssize_t writev(int, const struct iovec *, int);
ssize_t read(int, void *, size_t);
off_t lseek(int, off_t, int);
int ftruncate(int, off_t);
void *mmap(void *, size_t, int, int, int, off_t);
int mprotect(void *, size_t, int);
int msync(void *, size_t, int);
int munmap(void *, size_t);
int rename(const char *,const char *);
int delay(unsigned);
int pthread_create(pthread_t *,const void *,void *(*)(void *),void *);
int pthread_detach(pthread_t);
int pthread_join(pthread_t, void **);
int accept(int, void *, void *);
int shutdown(int, int);
void *dlopen(const char *, int);
void *dlsym(void *, const char *);
void *memcpy(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);
size_t strlen(const char *);

#endif
