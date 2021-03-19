#include <stdint.h>
#include <stdlib.h>

void aop_sram_memcpy(volatile void *dst, volatile void *src, size_t n){
    volatile uint8_t *dstp = (volatile uint8_t *)dst;
    volatile uint8_t *srcp = (volatile uint8_t *)src;

    for(int i=0; i<n; i++)
        dstp[i] = srcp[i];
}

void aop_sram_strcpy(volatile char *dst, const char *src){
    volatile char *src0 = (volatile char *)src;
    while((*dst++ = *src0++));
    *dst = '\0';
}

size_t aop_sram_strlen(volatile char *src){
    volatile char *p = src;

    while(*p)
        p++;

    return p - src;
}
