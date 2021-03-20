#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "common.h"
#include "debugger_log.h"
#include "SecureROM_offsets.h"
#include "structs.h"

/* ROM code only takes up 8 pages */
asm(".section __TEXT,__romreloc\n"
    ".align 14\n"
    ".space 0x20000, 0x0\n"
    ".section __TEXT,__text\n");

/* L3 page tables for relocated ROM */
/* XXX the MMU doesn't seem to like reading from AOP SRAM for ptes */
/* asm(".section __TEXT,__romrelocptes\n" */
/*     ".align 14\n" */
/*     ".space 0x4000, 0x0\n" */
/*     ".section __TEXT,__text\n"); */

__attribute__ ((naked)) static uint64_t transtest_read(uint64_t va){
    asm(""
        "at s1e1r, x0\n"
        "isb sy\n"
        "mrs x0, par_el1\n"
        "ret\n");
}

__attribute__ ((naked)) static uint64_t transtest_write(uint64_t va){
    asm(""
        "at s1e1w, x0\n"
        "isb sy\n"
        "mrs x0, par_el1\n"
        "ret\n");
}

bool init(void){
    /* Set up the logging system before anything else */
    loginit();

    /* Relocate ROM to AOP SRAM so we can patch ROM instructions and
     * hook panic */
    void *rom_start = (void *)0x100000000;

    aop_sram_memcpy((volatile void *)__romreloc_start, rom_start, 0x20000);

    dbglog("%s: ROM relocated to %#llx: first insn %#x\n", __func__,
            __romreloc_start, *(volatile uint32_t *)__romreloc_start);

    /* Change the page table hierarchy so translating any VA from
     * 0x100000000 to 0x102000000 translates to PA __romreloc_start to
     * (__romreloc_start + 0x2000000) so we have the device execute from
     * our copy. We can't do this just by changing the original TTE because
     * it represents a 32 MB region and __romreloc_start won't be 32 MB
     * aligned. Instead, we'll create a new L3 table that the original L2
     * entry will point to so we have much finer control over output
     * addresses. */
    /* extern volatile uint64_t __romrelocptes[] asm("section$start$__TEXT$__romrelocptes"); */

    /* New L2 TTE:
     *  NS: 0
     *  AP: priv=read-write, user=no-access
     *  XN: 0
     *  PXN: 0
     *  L3TableOutputAddress: malloc'ed SRAM
     *  Type: table (1)
     *  Valid: 1
     */
    /* The MMU doesn't like reading from AOP SRAM for these PTEs. But
     * malloc doesn't guarentee a page aligned pointer. There are only
     * gonna be 8 PTEs in this L3 table so by allocating a page + more
     * than enough space for those PTEs, we can just move forward until
     * this returned pointer is page aligned. XXX This is really stupid and
     * I'd like to figure this out asap */
    volatile uint64_t *relocptesp = (volatile uint64_t *)malloc(0x4050);

    if(!relocptesp){
        dbglog("%s: malloc failed\n", __func__);
        return true;
    }

    while((uintptr_t)relocptesp & 0x3fff)
        relocptesp++;

    volatile uint64_t *relocptesend = relocptesp + (0x20000 / 0x4000);

    uint64_t oa = AOP_SRAM_VA_TO_PA(__romreloc_start);
    uint64_t newl2entry = ((uint64_t)relocptesp & 0xffffffffc000) |
        (1 << 1) | (1 << 0);

    dbglog("%s: new L2 entry will be %#llx\n", __func__, newl2entry);

    dbglog("%s: new ROM PTEs start @ %#llx and end @ %#llx\n", __func__,
            (uint64_t)relocptesp, (uint64_t)relocptesend);

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

        dbglog("%s: New PTE for %#llx: %#llx\n", __func__,
                (uint64_t)relocptesp, pte);

        *relocptesp = pte;

        asm volatile("dsb sy");
        asm volatile("isb sy");

        oa += 0x4000;
        relocptesp++;
    }

    /* Do the write now, since everything has been setup if we're here */
    *(uint64_t *)0x18000c400 = newl2entry;
    /* XXX For testing with 0x102000000 - 0x104000000 */
    /* *(uint64_t *)0x18000c408 = newl2entry; */

    asm volatile("dsb sy");
    asm volatile("isb sy");
    asm volatile("tlbi vmalle1");
    asm volatile("isb sy");

    /* After this point, ROM instructions are patchable */

    /* uint64_t testva = 0x102000000; */
    uint64_t testva = 0x100000000;
    uint64_t ttr = 0x41424344;
    uint64_t ttw = 0x45464748;
    /* ttr = transtest_read(testva); */
    /* ttw = transtest_write(testva); */

    /* ttr = transtest_read(0x100000000); */
    ttr = transtest_read(testva);
    ttw = transtest_write(testva);

    /* ttr = 0x41424344; */
    /* ttw = 0x45464748; */
    dbglog("%#llx %#llx\n", ttr, ttw);
    /* uint64_t aop_sram_addr = 0x934e18000; */
    /* dbglog("try read for %#llx: %#llx\n", aop_sram_addr, */
    /*         transtest_read(aop_sram_addr)); */

    uint32_t testread = *(uint32_t *)testva;
    dbglog("%#x\n", testread);
    /* mov x0, #0x1234 */
    /* testva = 0x1000187DC; */

    return true;
}
