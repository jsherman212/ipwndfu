#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "common.h"
#include "debugger_log.h"
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

extern void cpu5_iorvbar(void);

__attribute__ ((noreturn)) void debugger_tick(void){
    dbglog("%s: we are alive\n", __func__);

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
    /* if(SecureDBG_init_flag) */
    /*     return 0; */

    uint64_t res = loginit();

    if(res)
        return res;

    *(uint64_t *)usb_interface_request_handler = (uint64_t)SecureDBG_usb_interface_request_handler;

    /* Letters array doesn't work */
    /* char letters[] = { 'A', 'B', 'C', 'D' };//, 'E' }; */
    for(int i=0; i<4; i++){//sizeof(letters)/sizeof(*letters); i++){
        for(int k=0; k<0x300; k++){
            /* dbglog("%c", letters[i]); */
            dbglog("%c", 'A');
        }
    }

    debuggee_cpu = curcpu();

    dbglog("%s: hello from SecureROM! We are CPU %d\n", __func__,
            debuggee_cpu);

    if(debuggee_cpu == debugger_cpu){
        dbglog("%s: why are we CPU5?\n", __func__);
        return 1;
    }

    SecureDBG_init_flag = true;

    return res;
}
