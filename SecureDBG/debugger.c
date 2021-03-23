#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "common.h"
#include "debugger_log.h"
#include "init.h"
#include "SecureROM_offsets.h"
#include "structs.h"

/* XXX what happens when we transition over to iBoot? */
static GLOBAL(uint8_t debuggee_cpu);
static GLOBAL(uint8_t debugger_cpu) = 5;

static uint8_t curcpu(void){
    uint64_t mpidr_el1;
    asm volatile("mrs %0, mpidr_el1" : "=r" (mpidr_el1));
    return mpidr_el1 & 0xff;
}

GLOBAL(const char *fmt) = "%s: hello from cpu%d!\n";
GLOBAL(const char *func) = "debugger_tick";

extern __attribute__ ((noreturn)) void debugger_tick(void);
/* extern __attribute__ ((naked, noreturn)) void debugger_tick(uint64_t); */

/* __attribute__ ((naked, noreturn)) void debugger_tick(uint64_t cpu5debug){ */
/*     asm("" */
/*         "mov x18, x0\n" */
/*         "sub sp, sp, #0x30\n" */
/*         "stp x29, x30, [sp, #0x20]\n" */
/*         "add x29, sp, #0x20\n" */
/*         "bl _curcpu\n" */
/*         "adrp x8, _fmt@PAGE\n" */
/*         "add x8, x8, _fmt@PAGEOFF\n" */
/*         "ldr x8, [x8]\n" */
/*         "stur w0, [x29, #-0x4]\n" */
/*         "mov x0, x8\n" */
/*         "mov x8, sp\n" */
/*         "adrp x9, _func@PAGE\n" */
/*         "add x9, x9, _func@PAGEOFF\n" */
/*         "ldr x9, [x9]\n" */
/*         "str x9, [x8]\n" */
/*         "ldur w10, [x29, #-0x4]\n" */
/*         "mov x1, x10\n" */
/*         "and x9, x1, #0xff\n" */
/*         "str x9, [x8, #0x8]\n" */
/*         "bl _dbglog\n" */




/*         "mov w1, 0x4141\n" */
/*         /1* "str w1, [x18]\n" *1/ */
/*         "b .\n" */
/*        ); */
/* } */

__attribute__ ((noreturn)) void debugger_tick(void){
    dbglog("%s: hello from cpu%d!\n", __func__, (uint32_t)curcpu());
    dbglog("AA%s: hello from cpu%d!\n", __func__, (uint32_t)curcpu());
    dbglog("hello from cpu%d!\n", (uint32_t)curcpu());
    dbglog("hfdsi %d\n", 444444);
    dbglog("Hello\n");
    dbglog("%s: test test\n", __func__);

    for(;;){

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

static GLOBAL(bool SecureDBG_init_flag) = false;

/* Called from src/usb_0xA1_2_arm64.S. Here we point SecureROM's global
 * USB interface callback to our own, set up a logging system, and kickstart
 * a debugger CPU. */
uint64_t debugger_entryp(void){
    if(SecureDBG_init_flag)
        return 0;

    if(!init())
        return 1;

    *(uint64_t *)usb_interface_request_handler = (uint64_t)SecureDBG_usb_interface_request_handler;

    debuggee_cpu = curcpu();

    /* dbglog("%s: hello from SecureROM! We are CPU %d\n", __func__, */
    /*         debuggee_cpu); */

    if(debuggee_cpu == debugger_cpu){
        dbglog("%s: why are we CPU5?\n", __func__);
        return 1;
    }

    SecureDBG_init_flag = true;

    return 0;
}
