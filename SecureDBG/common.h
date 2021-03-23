#ifndef COMMON
#define COMMON

#include <stdarg.h>

/* Make sure global variables go into __TEXT. Otherwise, their initial
 * values are useless, since clang sticks them into bss and they'll end
 * up getting whatever trash is inside AOP SRAM */
#define GLOBAL(x) x __attribute__ ((section("__TEXT,__text")))

extern volatile uint32_t cpu5_init_done;
extern volatile uint64_t __romreloc_start[] asm("section$start$__TEXT$__romreloc");

void aop_sram_memcpy(volatile void *, volatile void *, size_t);
void aop_sram_strcpy(volatile char *, const char *);
size_t aop_sram_strlen(volatile char *);
void aop_sram_vsnprintf(volatile char *, size_t, const char *, va_list);

#endif
