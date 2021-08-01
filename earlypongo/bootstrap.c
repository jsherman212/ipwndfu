#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "common.h"
#include "debugger_log.h"
#include "dram.h"
#include "fb.h"
#include "init.h"
#include "panic.h"
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
    return 0;

    /* XXX currently panics */
    if(request == earlypongo_LOG_READ){
        /* dbglog("%s: sending back log (bufout=%#llx)...\n", __func__, */
        /*         (uint64_t)bufout); */

        size_t len;
        char *log = getlog(&len);
        /* return 0; */
        /* log = "TEST"; */

        /* len = aop_sram_strlen(log); */

        if(len == 0){
            /* TODO not on AOP SRAM anymore but these functions still work */
            /* TODO rename */
            /* aop_sram_strcpy(log, "!NOLOG!"); */
            len = 7;
            memmove(log, "!NOLOG!", 7);
        }

        /* aop_sram_memcpy(io_buffer, log, len); */
        /* void *mem = malloc(len); */
        memmove(io_buffer, log, len);
        /* memmove(mem, log, len); */
        usb_core_do_io(0x80, io_buffer, len, NULL);
        /* usb_core_do_io(0x80, log, len, NULL); */
        /* usb_core_do_io(0x80, mem, len, NULL); */
    }

    return 0;
}

/* ROM code only takes up 8 pages */
asm(".section __TEXT,__romreloc\n"
    ".align 14\n"
    ".space 0x20000, 0x0\n"
    ".section __TEXT,__text\n");

/* L3 page tables for relocated ROM */
asm(".section __TEXT,__romrelocptes\n"
    ".align 14\n"
    ".space 0x4000, 0x0\n"
    ".section __TEXT,__text\n");

static void patchable_rom(void){

    extern volatile uint64_t romreloc[] asm("section$start$__TEXT$__romreloc");
    extern volatile uint64_t romrelocptes[] asm("section$start$__TEXT$__romrelocptes");

    memmove((void *)romreloc, (void *)0x100000000, 0x20000);

    volatile uint64_t *relocptesp = romrelocptes;
    volatile uint64_t *relocptesend = relocptesp + (0x20000 / 0x4000);

    uint64_t oa = (uint64_t)romreloc;
    uint64_t newl2entry = ((uint64_t)relocptesp & 0xffffffffc000) |
        (1 << 1) | (1 << 0);

    while(relocptesp < relocptesend){
        /* PTE template:
         *  ign: 0
         *  Bits[58:55]: 0
         *  XN: 0
         *  PXN: 0
         *  HINT: 0
         *  Bits[51:48]: 0
         *  OutputAddress: oa
         *  nG: 0
         *  AF: 1
         *  SH: 2
         *  AP: priv=read-write, user=no-access
         *  NS: 0
         *  AttrIdx: 0 (device memory)
         *  V: 1
         */
        uint64_t pte = oa | (1 << 10) | (2 << 8) | (1 << 1) | (1 << 0);
        /* uint64_t pte = oa | (1 << 10) | (2 << 8) | (1 << 5) | */
        /*     (1 << 2) | (1 << 1) | (1 << 0); */
        /* NS bit set does not panic for malloc'ed PTEs, does with AOP SRAM PTEs */
        /* 0 shareability does not panic for malled PTEs, does with AOP SRAM PTEs */
        /* uint64_t pte = oa; */
        /* pte |= (1 << 10);//AF */
        /* pte |= (2 << 8); //shareability */
        /* /1* pte |= (0 << 8); //shareability *1/ */
        /* /1* pte |= (0 << 5); //NS *1/ */
        /* pte |= (1 << 5); //NS */
        /* /1* pte |= (0 << 2); //attridx *1/ */
        /* pte |= (1 << 2); //attridx */
        /* pte |= (1 << 1);//always 1 */
        /* pte |= (1 << 0);//valid */

        /* dbglog("%s: New PTE for %#llx: %#llx\n", __func__, */
        /*         (uint64_t)relocptesp, pte); */

        *relocptesp = pte;

        asm volatile("dsb sy");
        asm volatile("isb sy");

        oa += 0x4000;
        relocptesp++;
    }

    *(uint64_t *)0x18000c400 = newl2entry;

    asm volatile("dsb sy");
    asm volatile("isb sy");
    asm volatile("tlbi vmalle1");
    asm volatile("isb sy");
}

static void install_panic_hook(void){
    uint32_t *panicp = (uint32_t *)panic;

    /* LDR X16, #0x8 */
    panicp[0] = 0x58000050;
    /* BR X16 */
    panicp[1] = 0xd61f0200;
    panicp[2] = (uint32_t)_panic;
    panicp[3] = (uint32_t)((uint64_t)_panic >> 32);

    dcache_clean_PoU(panicp, 4*sizeof(uint32_t));
    icache_invalidate_PoU(panicp, 4*sizeof(uint32_t));
}

static void cpu5_kickstart(void){
    /* Ensure CPU5's debug registers can be written to */
    uint64_t cpu5_coresight = 0x208510000;
    uint64_t cpu5_trace = 0x208510000 + 0x30000;

    volatile uint32_t *cpu5_edlar = (volatile uint32_t *)(cpu5_coresight + 0xfb0);
    volatile uint32_t *cpu5_edlsr = (volatile uint32_t *)(cpu5_coresight + 0xfb4);
    volatile uint32_t *cpu5_edscr = (volatile uint32_t *)(cpu5_coresight + 0x88);
    /* dbglog("%s: edscr: %#x\n", __func__, *cpu5_edscr); */

    *cpu5_edlar = 0xc5acce55;

    /* Wait for regs to be unlocked (if they weren't already) */
    while(*cpu5_edlsr & 2){}

    /* Bring up CPU5 via the external debug interface */
    extern void cpu5_iorvbar(void);

    *(volatile uint64_t *)0x208550000 = (volatile uint64_t )cpu5_iorvbar;
    asm volatile("dsb sy");
    asm volatile("isb sy");
    *(volatile uint32_t *)0x208510310 = 0x8;
    asm volatile("dsb sy");
    asm volatile("isb sy");

#define DBGWRAP_DBGHALT         (1ULL << 31)
#define DBGWRAP_Restart         (1uLL << 30)

    volatile uint64_t *cpu5_dbgwrap = (volatile uint64_t *)cpu5_trace;
    *cpu5_dbgwrap = (*cpu5_dbgwrap & ~DBGWRAP_DBGHALT) | DBGWRAP_Restart;

    /* while(!cpu5_init_done){} */
    for(int i=0; i<10000000; i++){}
}

void debugger_tick(void){
    /* volatile char *a = (volatile char*)0x413421439903; */
    /* *a =0; */
    dbglog("cpu5\n");


    for(;;);
    /* reboot2(); */
}

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
    /* extern volatile uint64_t text_end[] asm("segment$end$__TEXT"); */
    extern volatile uint64_t data_start[] asm("segment$start$__DATA");
    extern volatile uint64_t data_end[] asm("segment$end$__DATA");

    sram_expand();
    /* relocate_and_jump((void *)0x180200000, text_start, text_end); */
    relocate_and_jump((void *)0x180200000, text_start, data_end);
    /* From this point on we are off AOP SRAM and on AP SRAM */
    dcache_clean_and_invalidate_PoC((void *)0x180000000, 0x200000);
    /* dcache_clean_and_invalidate_PoC(data_start, ((uintptr_t)data_end - (uintptr_t)data_start)); */
    loginit();


    patchable_rom();
    install_panic_hook();
    uint64_t res = fb_init();
    /* cpu5_kickstart(); */

    /* void *pcpage; */
    /* asm volatile("adrp %0, 0" : "=r" (pcpage)); */
    /* dbglog("%s: alive after relocation execing on page @ %p\n", */
    /*         __func__, pcpage); */

    uint64_t *v = (uint64_t *)0x180200000;
    /* *v = 0x55555555; */

    /* XXX This likely needs to be rebased onto AP SRAM */
    *(uint64_t *)usb_interface_request_handler = (uint64_t)earlypongo_usb_interface_request_handler;

    /* for(int i=0; i<10000000; i++){} */

    /* return 0x5544664464743; */
    volatile uint32_t *p = (volatile uint32_t *)(*(uint64_t *)0x100018100);
    /* return *(uint64_t *)0x100018100; */
    /* return *p; */
    return res;

    /* for(int i=0; i<0x800; i++) */
    /*     ((volatile char*)io_buffer)[i]; */

    /* void *p = malloc(155); */

    /* return *v; */
    /* extern volatile uint64_t logs[] asm("section$start$__TEXT$__logs"); */
    /* memset(logs, 0, 0xc000); */
    return (uint64_t)io_buffer;
    /* return *(uint64_t*)logs; */
}
