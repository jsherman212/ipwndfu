#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#define ARM_TTE_TABLE_MASK          (0x0000ffffffffc000)

#define ARM_16K_TT_L1_SHIFT         (36)
#define ARM_16K_TT_L2_SHIFT         (25)
#define ARM_16K_TT_L3_SHIFT         (14)

#define ARM_TT_L1_SHIFT             ARM_16K_TT_L1_SHIFT
#define ARM_TT_L2_SHIFT             ARM_16K_TT_L2_SHIFT
#define ARM_TT_L3_SHIFT             ARM_16K_TT_L3_SHIFT

#define ARM_16K_TT_L1_INDEX_MASK    (0x00007ff000000000)
#define ARM_16K_TT_L2_INDEX_MASK    (0x0000000ffe000000)
#define ARM_16K_TT_L3_INDEX_MASK    (0x0000000001ffc000)

#define ARM_TTE_BLOCK_L2_MASK       (0x0000fffffe000000ULL)

#define ARM_TT_L1_INDEX_MASK        ARM_16K_TT_L1_INDEX_MASK
#define ARM_TT_L2_INDEX_MASK        ARM_16K_TT_L2_INDEX_MASK
#define ARM_TT_L3_INDEX_MASK        ARM_16K_TT_L3_INDEX_MASK

#define ARM_PTE_NX                  (0x0040000000000000uLL)
#define ARM_PTE_PNX                 (0x0020000000000000uLL)

#define ARM_PTE_APMASK              (0xc0uLL)
#define ARM_PTE_AP(x)               ((x) << 6)

#define AP_RWNA                     (0x0) /* priv=read-write, user=no-access */
#define AP_RWRW                     (0x1) /* priv=read-write, user=read-write */
#define AP_RONA                     (0x2) /* priv=read-only, user=no-access */
#define AP_RORO                     (0x3) /* priv=read-only, user=read-only */
#define AP_MASK                     (0x3) /* mask to find ap bits */

static char *getline_input(const char *prompt){
    printf("%s", prompt);

    char *got = NULL;
    size_t len = 0;

    ssize_t r = getline(&got, &len, stdin);

    if(r == -1)
        return NULL;

    got[r - 1] = '\0';

    return got;
}

static uint64_t bits(uint64_t number, uint64_t start, uint64_t end){
    uint64_t amount = (end - start) + 1;
    uint64_t mask = ((1ULL << amount) - 1) << start;

    return (uint64_t)((number & mask) >> start);
}

static void tcr_desc(uint64_t tcr){
    uint8_t ds = bits(tcr, 59, 59);
    uint8_t e0pd1 = bits(tcr, 56, 56);
    uint8_t e0pd0 = bits(tcr, 55, 55);
    uint8_t nfd1 = bits(tcr, 54, 54);
    uint8_t nfd0 = bits(tcr, 53, 53);
    uint8_t hwu162 = bits(tcr, 50, 50);
    uint8_t hwu161 = bits(tcr, 49, 49);
    uint8_t hwu160 = bits(tcr, 48, 48);
    uint8_t hwu159 = bits(tcr, 47, 47);
    uint8_t hwu062 = bits(tcr, 46, 46);
    uint8_t hwu061 = bits(tcr, 45, 45);
    uint8_t hwu060 = bits(tcr, 44, 44);
    uint8_t hwu059 = bits(tcr, 43, 43);
    uint8_t hpd1 = bits(tcr, 42, 42);
    uint8_t hpd0 = bits(tcr, 41, 41);
    uint8_t hd = bits(tcr, 40, 40);
    uint8_t ha = bits(tcr, 39, 39);
    uint8_t tbi1 = bits(tcr, 38, 38);
    uint8_t tbi0 = bits(tcr, 37, 37);
    uint8_t as = bits(tcr, 36, 36);
    uint32_t ips = bits(tcr, 32, 34);
    uint32_t tg1 = bits(tcr, 30, 31);
    uint32_t sh1 = bits(tcr, 28, 29);
    uint32_t orgn1 = bits(tcr, 26, 27);
    uint32_t irgn1 = bits(tcr, 24, 25);
    uint8_t epd1 = bits(tcr, 23, 23);
    uint8_t a1 = bits(tcr, 22, 22);
    uint32_t t1sz = bits(tcr, 16, 21);
    uint32_t tg0 = bits(tcr, 14, 15);
    uint32_t sh0 = bits(tcr, 12, 13);
    uint32_t orgn0 = bits(tcr, 10, 11);
    uint32_t irgn0 = bits(tcr, 8, 9);
    uint8_t epd0 = bits(tcr, 7, 7);
    uint32_t t0sz = bits(tcr, 0, 5);

    printf("TCR_EL1: %#llx:\n"
           "\tds:      %d\n"
           "\te0pd1:   %d\n"
           "\te0pd0:   %d\n"
           "\tnfd1:    %d\n"
           "\tnfd0:    %d\n"
           "\thwu162:  %d\n"
           "\thwu161:  %d\n"
           "\thwu160:  %d\n"
           "\thwu159:  %d\n"
           "\thwu062:  %d\n"
           "\thwu061:  %d\n"
           "\thwu060:  %d\n"
           "\thwu059:  %d\n"
           "\tHPD1:    %d\n"
           "\tHPD0:    %d\n"
           "\tHD:      %d\n"
           "\tHA:      %d\n"
           "\tTBI1:    %d\n"
           "\tTBI0:    %d\n"
           "\tAS:      %d\n"
           "\tIPS:     %d\n"
           "\tTG1:     %d\n"
           "\tSH1:     %d\n"
           "\tORGN1:   %d\n"
           "\tIRGN1:   %d\n"
           "\tEPD1:    %d\n"
           "\tA1:      %d\n"
           "\tT1SZ:    %d\n"
           "\tTG0:     %d\n"
           "\tSH0:     %d\n"
           "\tORGN0:   %d\n"
           "\tIRGN0:   %d\n"
           "\tEPD0:    %d\n"
           "\tT0SZ:    %d\n",
        tcr, ds, e0pd1, e0pd0, nfd1, nfd0, hwu162, hwu161, hwu160, hwu159,
        hwu062, hwu061, hwu060, hwu059, hpd1, hpd0, hd, ha, tbi1, tbi0, as,
        ips, tg1, sh1, orgn1, irgn1, epd1, a1, t1sz, tg0, sh0, orgn0,
        irgn0, epd0, t0sz);
}

static uint64_t ttbr0 = 0;

static void l2_tte_describe(uint64_t vaddr, uint64_t *l2_ttep){
    uint64_t l2_tte = *l2_ttep;

    printf("\tL2 TTE at %p: %#llx\n", l2_ttep, l2_tte);

    uint64_t l3_table = l2_tte & ARM_TTE_TABLE_MASK;
    uint64_t l3_idx = (vaddr >> ARM_TT_L3_SHIFT) & 0x7ff;
    uint64_t *l3_ptep = (uint64_t *)(l3_table + (0x8 * l3_idx));

    printf("\tL3 table is at %#llx, index: %#llx, pte @ %p\n", l3_table,
            l3_idx, l3_ptep);
}

static void l2_block_describe(uint64_t vaddr){
    uint64_t ttbase = ttbr0 & 0xfffffffffffe;
    uint64_t l2_idx = (vaddr >> ARM_TT_L2_SHIFT) & 0x7ff;
    uint64_t *l2_ttep = (uint64_t *)(ttbase + (0x8 * l2_idx));

    uint64_t l2_tte = *l2_ttep;

    if((l2_tte & 0x2) != 0){
        /* printf("%s: this L2 TTE is not a block!\n", __func__); */
        l2_tte_describe(vaddr, l2_ttep);
        return;
    }
    
    printf("\tL2 block entry at %p", l2_ttep);

    if(*l2_ttep == 0){
        printf(": no TTE for [%#llx, %#llx)\n", vaddr & ARM_TTE_BLOCK_L2_MASK,
                (vaddr + 0x2000000) & ARM_TTE_BLOCK_L2_MASK);
        return;
    }

    /* 32 MB block mapping */
    uint64_t phys = l2_tte & ARM_TTE_BLOCK_L2_MASK;
    uint64_t phys_end = phys + 0x2000000;

    char perms[4];
    strcpy(perms, "---");

    uint32_t ap = l2_tte & ARM_PTE_APMASK;

    /* No EL0 yet */
    if(ap == AP_RWNA)
        strcpy(perms, "rw");
    else
        strcpy(perms, "r-");

    if(!(l2_tte & ARM_PTE_PNX))
        perms[2] = 'x';

    printf(" for [%#llx, %#llx): %#llx %s\n", phys, phys_end,
            l2_tte, perms);
}

int main(int argc, char **argv){
    tcr_desc(0x1659ca51c);
    /* int fd = open("../../t8015_raw_ttes", O_RDONLY); */
    /* int fd = open("../../t8015_raw_ttes_after_map_rw", O_RDONLY); */
    /* int fd = open("../../t8015_raw_ttes_after_mapping_102000000", O_RDONLY); */
    /* int fd = open("../../t8015_raw_ttes_after_msgbuf_alloc", O_RDONLY); */
    int fd = open("../../t8015_raw_ttes_after_remap", O_RDONLY);

    if(fd == -1){
        printf("open: %s\n", strerror(errno));
        return 1;
    }

    /* T8015 */
    void *ttbase = mmap((void *)0x18000c000, 0x8000, PROT_READ | PROT_WRITE,
            MAP_PRIVATE, fd, 0);
    /* void *ttbase = mmap((void *)0x180000000, 0x8000, PROT_READ | PROT_WRITE, */
    /*         MAP_PRIVATE, fd, 0); */

    if(ttbase == MAP_FAILED){
        printf("Failed mmap: %s\n", strerror(errno));
        return 1;
    }

    printf("ttbase @ %p\n", ttbase);

    ttbr0 = (uint64_t)ttbase;

    for(;;){
        char *vaddr_s = getline_input("Virtual address: ");
        char *p = NULL;
        uint64_t vaddr = strtoull(vaddr_s, &p, 0);

        if(*p){
            printf("bad input '%s'\n", vaddr_s);
            return 1;
        }

        l2_block_describe(vaddr);
        free(vaddr_s);
    }

    return 0;
}
