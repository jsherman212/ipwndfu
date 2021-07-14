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

enum {
    earlypongo_LOG_READ = 0x4000,
    earlypongo_MAXREQ,
};

static bool is_earlypongo_request(uint16_t req){
    return req >= earlypongo_LOG_READ && req < earlypongo_MAXREQ;
}

/* Our USB interface callback will preserve the original functionality
 * by calling out to the code inside src/usb_0xA1_2_arm64.S if the
 * command doesn't match any in this file */
static int earlypongo_usb_interface_request_handler(struct usb_request_packet *req,
        void *bufout){
    uint16_t request = req->wValue;

    if(!is_earlypongo_request(request))
        return ipwndfu_usb_interface_request_handler(req, bufout);

    /* XXX currently panics */
    if(request == earlypongo_LOG_READ){
        /* dbglog("%s: sending back log (bufout=%#llx)...\n", __func__, */
        /*         (uint64_t)bufout); */

        size_t len;
        char *log = getlog(&len);

        if(len == 0){
            /* TODO not on AOP SRAM anymore but these functions still work */
            /* TODO rename */
            aop_sram_strcpy(log, "!NOLOG!");
            len = 7;
        }

        aop_sram_memcpy(io_buffer, log, len);
        usb_core_do_io(0x80, io_buffer, len, NULL);
    }

    return 0;
}

/* Called from src/usb_0xA1_2_arm64.S. Here we map 4 more MB of SRAM from
 * the L2 cache. That SRAM is mapped from 0x180200000 - 0x180600000. We
 * place the pongo embedded in ourselves some point on that SRAM. We also
 * relocate ROM to that SRAM so it is patchable. A logging system is set up
 * (for early debugging) and then we tell the ROM to boot. When we are called,
 * we are executing on AOP SRAM because there is not enough space for me to
 * map more AP SRAM in checkm8's payloads. AOP SRAM is cursed so once we
 * map that additional 4 MB of AP SRAM we relocate ourselves there. */

/* 1. Map 4 more MB of AP SRAM from L2 cache (0x180200000 - 0x180600000)
 * 2. Relocate ourselves to that SRAM ASAP because AOP SRAM is cursed. We'll
 *    stick ourselves at 0x180200000
 * 3. Copy ROM to that SRAM. We'll stick that at 0x1802
 * 4. Modify TTE that translates ROM virtual addresses to point to the ROM
 *    we just copied so ROM instructions are patchable
 * 5. Extract embedded pongoOS and stick it at 0x180300000
 * [...]
 */
uint64_t earlypongo_bootstrap(void){
    extern void sram_expand(void);
    extern void relocate_and_jump(void *dst, volatile void *start,
            volatile void *end);

    extern volatile uint64_t text_start[] asm("segment$start$__TEXT");
    extern volatile uint64_t text_end[] asm("segment$end$__TEXT");

    sram_expand();
    relocate_and_jump((void *)0x180200000, text_start, text_end);
    /* From this point on we are off AOP SRAM and on AP SRAM */

    loginit();

    void *pcpage;
    asm volatile("adrp %0, 0" : "=r" (pcpage));
    dbglog("%s: alive after relocation execing on page @ %p\n",
            __func__, pcpage);

    uint64_t *v = (uint64_t *)0x180200000;
    /* *v = 0x55555555; */

    /* XXX This likely needs to be rebased onto AP SRAM */
    *(uint64_t *)usb_interface_request_handler = (uint64_t)earlypongo_usb_interface_request_handler;

    return *v;
}
