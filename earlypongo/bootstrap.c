#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "common.h"
#include "debugger_log.h"
#include "panic.h"
#include "SecureROM_offsets.h"
#include "structs.h"

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
int earlypongo_usb_interface_request_handler(struct usb_request_packet *req,
        void *bufout){
    uint16_t request = req->wValue;

    if(!is_earlypongo_request(request))
        return ipwndfu_usb_interface_request_handler(req, bufout);

    if(request == earlypongo_LOG_READ){
        size_t len;
        char *log = getlog(&len);

        if(len == 0){
            log = "!NOLOG!";
            len = 7;
        }

        memmove(io_buffer, log, len);
        usb_core_do_io(0x80, io_buffer, len, NULL);
    }

    return 0;
}

static void boot(void){
    /* This is copied from ipwndfu */

    uint64_t block1[8];
    uint64_t block2[8];

    memset(block1, 0, sizeof(block1));
    memset(block2, 0, sizeof(block2));

    uint64_t heap_state = 0x1800086A0;

    block1[3] = heap_state;
    block1[4] = 2;
    block1[5] = 132;
    block1[6] = 128;

    block2[3] = heap_state;
    block2[4] = 2;
    block2[5] = 8;
    block2[6] = 128;

    uint8_t *heap_base = (uint8_t *)0x1801e8000;
    uint64_t heap_write_offset = 0x5000;

    memmove(heap_base + heap_write_offset, block1, sizeof(block1));
    memmove(heap_base + heap_write_offset + 0x80, block2, sizeof(block2));
    memmove(heap_base + heap_write_offset + 0x100, block2, sizeof(block2));
    memmove(heap_base + heap_write_offset + 0x180, block2, sizeof(block2));

    heap_write_hash(heap_base + heap_write_offset);
    heap_write_hash(heap_base + heap_write_offset + 0x80);
    heap_write_hash(heap_base + heap_write_offset + 0x100);
    heap_write_hash(heap_base + heap_write_offset + 0x180);

    heap_check_all();

    uint64_t *bootstrap_task_lr = (uint64_t *)0x180015f88;
    uint8_t *dfu_bool = (uint8_t *)0x1800085b0;

    *bootstrap_task_lr = 0x10000188c;
    *dfu_bool = 1;

    void *dfu_state = (void *)0x1800085e0;

    dfu_notify(dfu_state);
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

static uint32_t assemble_bl(uint64_t from, uint64_t to){
    uint32_t imm26 = ((to - from) >> 2) & 0x3ffffff;
    return (37u << 26) | imm26;
}

static void install_boot_tramp_hook(void){
    extern void boot_tramp_hook(void *, void *);

    uint64_t boot_tramp = 0x180018000;

    *(uint32_t *)boot_tramp = assemble_bl(boot_tramp,
            (uint64_t)boot_tramp_hook);

    asm volatile("dsb sy");
    asm volatile("isb sy");
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
 * 3. Embedded PongoOS ends up somewhere page aligned around 0x1802xxxxx
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
    dcache_clean_and_invalidate_PoC((void *)0x180000000, 0x200000);
    icache_invalidate_PoU((void *)0x180000000, 0x200000);
    /* patchable_rom(); */
    /* install_panic_hook(); */
    loginit();

    void *pcpage;
    asm volatile("adrp %0, 0" : "=r" (pcpage));
    dbglog("%s: alive after relocation execing on page @ %p\n",
            __func__, pcpage);

    /* Force clang to not stick a pointer to our USB handler onto the
     * stack when we are still on AOP SRAM */
    asm volatile(""
            "adrp x8, _earlypongo_usb_interface_request_handler@PAGE\n"
            "add x8, x8, _earlypongo_usb_interface_request_handler@PAGEOFF\n"
            "mov x9, #0x8638\n"
            "movk x9, #0x8000, lsl #16\n"
            "movk x9, #1, lsl #32\n"
            "str x8, [x9]\n"
            "dsb sy\n"
            "isb sy\n" : : : "x8", "x9");

    uint64_t *v = (uint64_t *)0x180200000;
    *v = 0x55555555;

    /* *(uint32_t *)0x180018000 = 0x14000000; */
    /* asm volatile("dsb sy"); */
    /* asm volatile("isb sy"); */

    install_boot_tramp_hook();
    boot();

    /* volatile uint32_t *a=(volatile uint32_t *)0x432188349298; */
    /* *a=0; */

    return *v;
}
