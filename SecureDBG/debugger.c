#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "common.h"
#include "debugger_log.h"
#include "dram.h"
#include "init.h"
#include "SecureROM_offsets.h"
#include "structs.h"
#include "synopsys_regs.h"

static GLOBAL(bool SecureDBG_init_flag) = false;

/* XXX what happens when we transition over to iBoot? */
static GLOBAL(uint8_t debuggee_cpu);
static GLOBAL(uint8_t debugger_cpu) = 5;

static uint8_t curcpu(void){
    uint64_t mpidr_el1;
    asm volatile("mrs %0, mpidr_el1" : "=r" (mpidr_el1));
    return mpidr_el1 & 0xff;
}

extern __attribute__ ((noreturn)) void debugger_tick(void);

extern void mmu_enable(uint64_t);
extern uint64_t mmu_disable(void);

__attribute__ ((noreturn)) void debugger_tick(void){
    /* dbglog("%s: hello from cpu%d!\n", __func__, (uint32_t)curcpu()); */
    /* dbglog("yoo\n"); */
    /* for(;;); */

    /* uint64_t tcr, ttbr0; */
    /* asm volatile("mrs %0, tcr_el1" : "=r" (tcr)); */
    /* asm volatile("mrs %0, ttbr0_el1" : "=r" (ttbr0)); */
    /* dbglog("cpu5: TCR_EL1: %#llx TTBR0_EL1: %#llx\n", tcr, ttbr0); */

    /* for(;;); */
    int a = 0x41;
    uint64_t old_sctlr = mmu_disable();
    a = 0xff;
    mmu_enable(old_sctlr);
    dbglog("%s: a %#x\n", __func__, a);

    for(;;);

    for(;;){
        if(SecureDBG_init_flag){
            extern volatile uint64_t __romrelocptesTEST[] asm("section$start$__TEXT$__romrelocptes2");
            volatile uint64_t *relocptesp = (volatile uint64_t *)__romrelocptesTEST;
            /* dcache_clean_and_invalidate_PoC(relocptesp, 0x40); */
            uint32_t *test = (uint32_t *)0x102000000;
            uint64_t res = 0x4142434444434241;

            /* res = at_s1e1r(test); */
            res  = *(uint32_t *)test;

            dbglog("%s: read test %#llx\n", __func__, res);
            /* bool res = dram_bringup(); */
            
            /* /1* panic("", "%s: DRAM bringup res %d\n", __func__, res); *1/ */
            /* dbglog("%s: DRAM bringup res %d\n", __func__, res); */

            for(;;);
            /* if(res){ */
            /*     volatile uint32_t *dram = (volatile uint32_t *)0x800000000; */
            /*     identity_map_rw((uint64_t)dram, 0x4000); */
            /*     dbglog("%s: dram read %#x\n", __func__, *dram); */
            /*     *dram = 0x41424344; */
            /*     dbglog("%s: dram read %#x\n", __func__, *dram); */
            /* } */
        }
    }
}

enum {
    SecureDBG_LOG_READ = 0x4000,
    SecureDBG_MAXREQ,
};

static bool is_SecureDBG_request(uint16_t req){
    return req >= SecureDBG_LOG_READ && req < SecureDBG_MAXREQ;
}

/* Our USB interface callback will preserve the original functionality
 * by calling out to the code inside src/usb_0xA1_2_arm64.S if the
 * command doesn't match any in this file. (at least, if we're executing
 * inside SecureROM, what about iBoot?) */
static int SecureDBG_usb_interface_request_handler(struct usb_request_packet *req,
        void *bufout){
    uint16_t request = req->wValue;

    if(!is_SecureDBG_request(request))
        return ipwndfu_usb_interface_request_handler(req, bufout);


    if(request == SecureDBG_LOG_READ){
        /* dbglog("%s: sending back log (bufout=%#llx)...\n", __func__, */
        /*         (uint64_t)bufout); */

        size_t len;
        char *log = getlog(&len);

        if(len == 0){
            aop_sram_strcpy(log, "!NOLOG!");
            len = 7;
        }

        aop_sram_memcpy(io_buffer, log, len);
        usb_core_do_io(0x80, io_buffer, len, NULL);
    }

    return 0;
}

/* Called from src/usb_0xA1_2_arm64.S. Here we point SecureROM's global
 * USB interface callback to our own, set up a logging system, and kickstart
 * a debugger CPU. */
uint64_t debugger_entryp(void){
    /* *(volatile uint32_t *)0x2352bc000 &= 0xfffffffe; */

    if(SecureDBG_init_flag)
        return 0;

    if(!init())
        return 1;

    *(uint64_t *)usb_interface_request_handler = (uint64_t)SecureDBG_usb_interface_request_handler;

    debuggee_cpu = curcpu();

    dbglog("%s: hello from SecureROM! We are CPU %d\n", __func__,
            debuggee_cpu);

    if(debuggee_cpu == debugger_cpu){
        dbglog("%s: why are we CPU5?\n", __func__);
        return 1;
    }

    SecureDBG_init_flag = true;

    /* uint64_t tcr, ttbr0; */
    /* asm volatile("mrs %0, tcr_el1" : "=r" (tcr)); */
    /* asm volatile("mrs %0, ttbr0_el1" : "=r" (ttbr0)); */
    /* dbglog("cpu0: TCR_EL1: %#llx TTBR0_EL1: %#llx\n", tcr, ttbr0); */

    return 0;
}
