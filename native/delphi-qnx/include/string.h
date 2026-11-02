#ifndef DCC_QNX_STRING_H
#define DCC_QNX_STRING_H

#include <stddef.h>

void *memcpy(void *destination,const void *source,size_t bytes);
void *memset(void *destination,int value,size_t bytes);
int memcmp(const void *left,const void *right,size_t bytes);
size_t strlen(const char *text);

#endif
