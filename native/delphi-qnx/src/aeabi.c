#include <stddef.h>

/* Clang may lower naturally aligned structure copies to this ARM EABI helper.
 * Keep it inside the preload object so the build does not depend on a QNX SDK
 * compiler-runtime archive. */
void __aeabi_memcpy4(void *destination,const void *source,size_t bytes){
    unsigned char *out=(unsigned char *)destination;
    const unsigned char *in=(const unsigned char *)source;
    while(bytes--)*out++=*in++;
}
void __aeabi_memcpy8(void *destination,const void *source,size_t bytes){
    __aeabi_memcpy4(destination,source,bytes);
}
