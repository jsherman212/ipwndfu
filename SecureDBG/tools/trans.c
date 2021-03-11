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

static uint64_t ttbr0 = 0;

static void l2_block_describe(uint64_t vaddr){
    uint64_t ttbase = ttbr0 & 0xfffffffffffe;
    uint64_t l2_idx = (vaddr >> ARM_TT_L2_SHIFT) & 0x7ff;
    uint64_t *l2_ttep = (uint64_t *)(ttbase + (0x8 * l2_idx));

    uint64_t l2_tte = *l2_ttep;

    if((l2_tte & 0x2) != 0){
        printf("%s: this L2 TTE is not a block!\n", __func__);
        return;
    }
    
    printf("\tL2 block entry at %p", l2_ttep);

    if(*l2_ttep == 0)
        printf(": no TTE for [%#llx, %#llx)\n", vaddr, vaddr + 0x2000000);
    else{
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
}

int main(int argc, char **argv){
    int fd = open("../../t8015_raw_ttes", O_RDONLY);

    if(fd == -1){
        printf("open: %s\n", strerror(errno));
        return 1;
    }

    /* T8015 */
    void *ttbase = mmap((void *)0x18000c000, 0x8000, PROT_READ | PROT_WRITE,
            MAP_PRIVATE, fd, 0);

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
