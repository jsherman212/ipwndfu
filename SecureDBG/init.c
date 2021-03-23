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

bool init(void){
    /* Set up the logging system before anything else */
    loginit();

    /* Relocate ROM to AOP SRAM so we can patch ROM instructions and
     * hook panic */
    void *rom_start = (void *)0x100000000;

    aop_sram_memcpy((volatile void *)__romreloc_start, rom_start, 0x20000);

    /* dbglog("%s: ROM relocated to %#llx: first insn %#x\n", __func__, */
    /*         __romreloc_start, *(volatile uint32_t *)__romreloc_start); */

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
        /* dbglog("%s: malloc failed\n", __func__); */
        return true;
    }

    while((uintptr_t)relocptesp & 0x3fff)
        relocptesp++;

    volatile uint64_t *relocptesend = relocptesp + (0x20000 / 0x4000);

    /* uint64_t oa = AOP_SRAM_VA_TO_PA(__romreloc_start); */
    uint64_t oa = (uint64_t)__romreloc_start;
    uint64_t newl2entry = ((uint64_t)relocptesp & 0xffffffffc000) |
        (1 << 1) | (1 << 0);

    /* dbglog("%s: new L2 entry will be %#llx\n", __func__, newl2entry); */

    /* dbglog("%s: new ROM PTEs start @ %#llx and end @ %#llx\n", __func__, */
            /* (uint64_t)relocptesp, (uint64_t)relocptesend); */

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

        /* dbglog("%s: New PTE for %#llx: %#llx\n", __func__, */
                /* (uint64_t)relocptesp, pte); */

        *relocptesp = pte;

        asm volatile("dsb sy");
        asm volatile("isb sy");

        oa += 0x4000;
        relocptesp++;
    }

    /* Do the write now, since everything has been setup if we're here */
    *(uint64_t *)0x18000c400 = newl2entry;

    /* Also mark the original TTE for AOP SRAM as rwx, so cpu5
     * doesn't panic when we set the MMU bit in its SCTLR_EL1 */
    /* *(uint64_t *)0x18000c8d0 = 0x234000421; */

    /* XXX For testing with 0x102000000 - 0x104000000 */
    /* *(uint64_t *)0x18000c408 = newl2entry; */

    asm volatile("dsb sy");
    asm volatile("isb sy");
    asm volatile("tlbi vmalle1");
    asm volatile("isb sy");

    /* After this point, ROM instructions are patchable. Let's bring
     * up CPU5 */

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

    /* *(volatile uint64_t *)0x208550000 = AOP_SRAM_VA_TO_PA(cpu5_iorvbar); */
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

     extern volatile uint64_t cpu5_debug[] asm("section$start$__TEXT$__cpu5debug");
    volatile uint8_t *cpu5_debug8 = (volatile uint8_t *)cpu5_debug;
    volatile uint32_t *cpu5_debug32 = (volatile uint32_t *)cpu5_debug;
    volatile uint64_t *cpu5_debug64 = (volatile uint64_t *)cpu5_debug;

    /* dbglog("%s: edscr: %#x\n", __func__, *cpu5_edscr); */

    int lim = 6;
    for(int i=0; i<=lim; i++){
        /* dbglog("%#llx\n", cpu5_debug64[i]); */
    }
    /* dbglog("cpu5 init done: %d\n", cpu5_init_done); */

/*     dbglog("%#llx %#llx %#llx %#llx %#llx %#llx %#llx\n", cpu5_debug64[0], cpu5_debug64[1], */
/*             cpu5_debug64[2], cpu5_debug64[3], cpu5_debug64[4]); */

    return true;
}
