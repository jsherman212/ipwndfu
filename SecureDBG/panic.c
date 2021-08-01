#include <stdarg.h>
#include <stdint.h>

#include "common.h"
#include "debugger_log.h"
#include "structs.h"

static GLOBAL(int g_panic_count) = 0;

static void nested_panic_check(void){
    g_panic_count++;

    if(g_panic_count > 2){
        dbglog("nested panic, stopping here...\n");
        for(;;);
    }
}

struct frame {
    struct frame *fp;
    uint64_t lr;
};

static void backtrace(struct frame *f){
    int fnum = 0;

    while(f){
        dbglog("Frame %d: fp 0x%llx lr 0x%llx\n", fnum,
                (uint64_t)f->fp, f->lr);
        f = f->fp;
        fnum++;
    }

    dbglog("\n--- Could not unwind past frame %d\n", fnum - 1);
}

__attribute__ ((noreturn)) void _panic(const char *a1, const char *fmt, ...){
    nested_panic_check();

    dbglog("panic called!!\n");

    /* State structure is in x21 */
    uint64_t x21;
    asm volatile("mov %0, x21" : "=r" (x21));

    struct rstate *state = (struct rstate *)x21;

    void *caller = __builtin_return_address(0);

    uint64_t mpidr;
    asm volatile("mrs %0, mpidr_el1" : "=r" (mpidr));
    uint8_t curcpu = mpidr & 0xff;

    va_list args;
    va_start(args, fmt);

    dbglog("\npanic(cpu %d caller %p): ", curcpu, caller);
    vdbglog(fmt, args);
    dbglog("\n\t");

    va_end(args);

    for(int i=0; i<sizeof(state->x)/sizeof(*state->x); i++){
        dbglog("x%d: 0x%-16.16llx\t", i, state->x[i]);

        if(i > 0 && (i+1)%5 == 0)
            dbglog("\n\t");
    }

    dbglog("\n\tfp: 0x%-16.16llx\tlr: 0x%-16.16llx\tsp: 0x%-16.16llx"
            "\tpc: 0x%-16.16llx\n", state->fp, state->lr, state->sp, state->pc);

    uint64_t esr, far;
    asm volatile("mrs %0, esr_el1" : "=r" (esr));
    asm volatile("mrs %0, far_el1" : "=r" (far));

    dbglog("\tESR_EL1: 0x%-8.8x\tFAR_EL1: 0x%-16.16llx\n\n",
            (uint32_t)esr, far);

    /* backtrace(__builtin_frame_address(1)); */
    backtrace((struct frame *)state->fp);

    for(;;);
}
