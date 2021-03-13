#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "debugger_log.h"
#include "SecureROM_offsets.h"
#include "structs.h"

/* XXX what happens when we transition over to iBoot? */

/* SecureROM/iBoot are single core, so we can safely start up another
 * CPU as the debugger CPU. The debuggee CPU is whatever CPU is executing
 * the code inside debugger_entryp. */
static uint8_t debugger_cpu;
static uint8_t debuggee_cpu;

__attribute__ ((noreturn)) static void debugger_tick(void){
    for(;;){

    }
}

static uint8_t curcpu(void){
    uint64_t mpidr_el1;
    asm volatile("mrs %0, ttbr0_el1" : "=r" (mpidr_el1));
    return mpidr_el1 & 0xff;
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
        void **bufoutp){
    uint16_t request = req->wValue;

    dbglog("%s: got request %#x\n", __func__, request);

    if(!is_SecureDBG_request(request))
        return ipwndfu_usb_interface_request_handler(req, bufoutp);

    if(request == SecureDBG_LOG_READ){
        dbglog("%s: sending back log...\n", __func__);
        size_t len = 0;
        char *log = getlog(&len);
        usb_core_do_io(0x80, log, req->wLength, NULL);
    }

    return 0;
}

static bool SecureDBG_init_flag = false;

/* Called from src/usb_0xA1_2_arm64.S. Here we point SecureROM's global
 * USB interface callback to our own, set up a logging system, and kickstart
 * a debugger CPU. */
uint64_t debugger_entryp(void){
    uint64_t res = 0;
    /* if(SecureDBG_init_flag) */
    /*     return 0; */

    res = loginit();

    if(res)
        return res;

    dbglog("%s: hello from SecureROM!\n", __func__);

    debuggee_cpu = curcpu();
    
    /* Low powered caller? Low powered debugger */
    if(debuggee_cpu == 0)
        debugger_cpu = 1;
    /* High powered caller? High powered debugger */
    else
        debugger_cpu = 5;

    *(uint64_t *)usb_interface_request_handler = (uint64_t)SecureDBG_usb_interface_request_handler;

    SecureDBG_init_flag = true;

    return res;
}
