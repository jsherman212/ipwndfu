#ifndef COMMON
#define COMMON

/* Make sure global variables go into __TEXT. Otherwise, their initial
 * values are useless, since clang sticks them into bss and they'll end
 * up getting whatever trash is inside AOP SRAM */
#define GLOBAL(x) x __attribute__ ((section("__TEXT,__text")))

extern volatile uint64_t __romreloc_start[] asm("section$start$__TEXT$__romreloc");

#define AOP_SRAM_REBASE (0x700000000)
#define AOP_SRAM_VA_TO_PA(va) (((uintptr_t)va) - AOP_SRAM_REBASE)
#define AOP_SRAM_PA_TO_VA(pa) (((uintptr_t)pa) + AOP_SRAM_REBASE)

#define ROM_TO_AOP_SRAM_VA(ra)   (((uintptr_t)ra - 0x100000000) + (uintptr_t)__romreloc_start)
#define ROM_TO_AOP_SRAM_PA(ra)   (((uintptr_t)ra - 0x100000000) + AOP_SRAM_VA_TO_PA(__romreloc_start))

void aop_sram_memcpy(volatile void *, volatile void *, size_t);
void aop_sram_strcpy(volatile char *, const char *);
size_t aop_sram_strlen(volatile char *);

#endif
