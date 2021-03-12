#include <stdint.h>

uint64_t debugger_entryp(void){
    uint64_t mpidr_el1;
    asm volatile("mrs %0, ttbr0_el1" : "=r" (mpidr_el1));
    /* return (int)(mpidr_el1 & 0xff); */
    /* return mpidr_el1; */
    return 0x4142434445464748;
}
