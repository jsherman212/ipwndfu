#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "common.h"
#include "debugger_log.h"
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

__attribute__ ((noreturn)) void debugger_tick(void){
    dbglog("%s: hello from cpu%d!\n", __func__, (uint32_t)curcpu());
    dbglog("%s: interrupt mask %#x\n", __func__, rGINTMSK);

    uint32_t prev_gintsts = 0;
    for(uint64_t i=0; ; i++){
        uint32_t gintsts = rGINTSTS;

        if(prev_gintsts != gintsts){
            dbglog("%s: %lld: gintsts changed: %#x\n", __func__, i, gintsts);
            prev_gintsts = gintsts;

            if(SecureDBG_init_flag){
                /* panic("%s: cpu5 panic", __func__); */
                volatile uint32_t *demote = (volatile uint32_t *)0x2352bc000;
                *demote = (*demote & 0xfffffffe);
                /* *(volatile uint32_t *)0x515141424324 = 0; */
                dbglog("%s: wrote to demote mmio\n", __func__);
            }
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

    return 0;
}
