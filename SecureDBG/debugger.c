#include <stdbool.h>
#include <stdint.h>

static bool SecureDBG_init_flag = false;

/* SecureROM/iBoot are single core, so we can safely start up another
 * CPU as the debugger CPU. The debuggee CPU is whatever CPU is executing
 * the code inside debugger_entryp. */
static uint8_t debugger_cpu;
static uint8_t debuggee_cpu;

/* Called from src/usb_0xA1_2_arm64.S */
uint64_t debugger_entryp(void){
    /* if(SecureDBG_init_flag) */
    /*     return 0; */

    uint64_t mpidr_el1;
    asm volatile("mrs %0, ttbr0_el1" : "=r" (mpidr_el1));
    /* return (int)(mpidr_el1 & 0xff); */
    /* return mpidr_el1; */
    /* return 0; */
    /* return 0x5555555588888888; */
    return 0x4141414141414141;
}
