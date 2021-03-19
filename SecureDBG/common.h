#ifndef COMMON
#define COMMON

/* Make sure global variables go into __TEXT. Otherwise, their initial
 * values are useless, since clang sticks them into bss and they'll end
 * up getting whatever trash is inside AOP SRAM */
#define GLOBAL(x) x __attribute__ ((section("__TEXT,__text")))

void aop_sram_memcpy(volatile void *, volatile void *, size_t);
void aop_sram_strcpy(volatile char *, const char *);
size_t aop_sram_strlen(volatile char *);

#endif
