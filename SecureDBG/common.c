#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

void aop_sram_memcpy(volatile void *dst, volatile void *src, size_t n){
    volatile uint8_t *dstp = (volatile uint8_t *)dst;
    volatile uint8_t *srcp = (volatile uint8_t *)src;

    for(int i=0; i<n; i++)
        dstp[i] = srcp[i];
}

void aop_sram_strcpy(volatile char *dst, const char *src){
    volatile char *src0 = (volatile char *)src;
    while((*dst++ = *src0++));
    *dst = '\0';
}

size_t aop_sram_strlen(volatile char *src){
    volatile char *p = src;

    while(*p)
        p++;

    return p - src;
}

static void prntnum(volatile char **bufp, int64_t num, size_t *leftp){
    if(num < 0 && *leftp > 1){
        *(*bufp)++ = '-';
        (*leftp)--;
        /* Make positive */
        num *= -1;
    }

    if(*leftp <= 1)
        return;

    /* Get digits */
    char digits[20];

    for(int i=0; i<sizeof(digits); i++)
        digits[i] = '\0';

    int thisdig = sizeof(digits) - 1;

    while(num){
        int digit = num % 10;

        if(digit < 0)
            digit *= -1;

        digits[thisdig--] = (char)(digit + '0');
        num /= 10;
    }

    int startdig = 0;

    while(digits[startdig] == '\0')
        startdig++;

    while(*leftp > 1 && startdig < sizeof(digits)){
        *(*bufp)++ = digits[startdig++];
        (*leftp)--;
    }
}

static void prnthex(volatile char **bufp, uint8_t *bytes, size_t len,
        size_t *leftp, bool zeroX){
    if(zeroX){
        if(*leftp > 1){
            *(*bufp)++ = '0';
            (*leftp)--;
        }

        if(*leftp > 1){
            *(*bufp)++ = 'x';
            (*leftp)--;
        }
    }

    if(*leftp <= 1)
        return;

    const char *hex = "0123456789abcdef";
    bool first_nonzero = false;

    for(size_t i=0; i<len; i++){
        if(*leftp > 1){
            char byte = hex[(*bytes >> 4) & 0xf];

            if(byte != '0' && !first_nonzero)
                first_nonzero = true;

            if(first_nonzero)
                *(*bufp)++ = byte;

            (*leftp)--;
        }

        if(*leftp > 1){
            char byte = hex[*bytes++ & 0xf];

            if(byte != '0' && !first_nonzero)
                first_nonzero = true;

            if(first_nonzero)
                *(*bufp)++ = byte;

            (*leftp)--;
        }
    }

    /* Entire number was zero */
    if(!first_nonzero && *leftp > 1){
        *(*bufp)++ = '0';
        (*leftp)--;
    }
}

/* Format specifiers currently supported:
 *  %%, %c, %s, %d, %x, %p, %llx, %lld
 *
 * No width, padding, etc, only 0x
 */
void aop_sram_vsnprintf(volatile char *buf, size_t n, const char *fmt,
        va_list args){
    if(n == 0)
        return;

    if(n == 1){
        *buf = '\0';
        return;
    }

    size_t left = n;
    size_t printed = 0;
    char *fmtp = (char *)fmt;
    char *bufp = buf;

    while(*fmtp){
        /* As long as we don't see a format specifier, just copy
         * the string */
        while(*fmtp && *fmtp != '%'){
            if(left > 1){
                *bufp++ = *fmtp;
                left--;
            }

            fmtp++;
        }

        /* We done? */
        if(*fmtp == '\0')
            break;

        /* Get off the '%' */
        fmtp++;

        bool zeroX = false;

        if(*fmtp == '#'){
            zeroX = true;
            fmtp++;
        }

        switch(*fmtp){
            case '%':
                {
                    if(left > 1){
                        *bufp++ = '%';
                        left--;
                    }

                    fmtp++;

                    break;
                }
            case 'c':
                {
                    char ch = va_arg(args, int);

                    if(left > 1){
                        *bufp++ = ch;
                        left--;
                    }

                    fmtp++;

                    break;
                }
            case 's':
                {
                    char *arg = va_arg(args, char *);

                    while(*arg && left > 1){
                        *bufp++ = *arg++;
                        left--;
                    }

                    fmtp++;

                    break;
                }
            case 'd':
                {
                    int arg = va_arg(args, int);

                    prntnum(&bufp, (int64_t)arg, &left);

                    fmtp++;

                    break;
                }
            case 'x':
                {
                    uint32_t arg = __builtin_bswap32(va_arg(args, uint32_t));
                    uint8_t *bytes = (uint8_t *)&arg;

                    prnthex(&bufp, bytes, sizeof(arg), &left, zeroX);

                    fmtp++;

                    break;
                }
            case 'p':
                {
                    zeroX = true;

                    uintptr_t arg = __builtin_bswap64(va_arg(args, uintptr_t));
                    uint8_t *bytes = (uint8_t *)&arg;

                    prnthex(&bufp, bytes, sizeof(arg), &left, zeroX);

                    fmtp++;

                    break;
                }
                /* llx or lld */
            case 'l':
                {
                    if(fmtp[1] && fmtp[1] == 'l' && fmtp[2]){
                        if(fmtp[2] == 'x'){
                            uint64_t arg = __builtin_bswap64(va_arg(args, uint64_t));
                            uint8_t *bytes = (uint8_t *)&arg;

                            prnthex(&bufp, bytes, sizeof(arg), &left, zeroX);
                        }
                        else if(fmtp[2] == 'd'){
                            int64_t arg = va_arg(args, int64_t);

                            prntnum(&bufp, arg, &left);
                        }

                        fmtp += 3;
                    }

                    break;
                }
            default:
                break;
        };
    }

    if(left > 0)
        *bufp = '\0';
}

__attribute__ ((naked)) uint64_t at_s1e1r(void *addr){
    asm(""
        "at s1e1r, x0\n"
        "isb sy\n"
        "mrs x0, par_el1\n"
        "ret\n"
       );
}

__attribute__ ((naked)) uint64_t at_s1e1w(void *addr){
    asm(""
        "at s1e1w, x0\n"
        "isb sy\n"
        "mrs x0, par_el1\n"
        "ret\n"
       );
}

void dcache_clean_PoU(void *address, size_t size){
    const size_t cache_line_size = 64;

    size = (size + cache_line_size) & ~(cache_line_size - 1);

    uint64_t start = ((uint64_t)address & ~(cache_line_size - 1));
    uint64_t end = start + size;

    asm volatile("isb sy");

    do {
        asm volatile(""
                "dc cvau, %0\n"
                "dsb ish\n"
                "isb sy\n"
                : : "r" (start));

        start += cache_line_size;
    } while (start < end);
}

void dcache_clean_and_invalidate_PoC(void *address, size_t size){
    const size_t cache_line_size = 64;

    if(size & (cache_line_size - 1))
        size = (size + cache_line_size) & ~(cache_line_size - 1);

    uint64_t start = ((uint64_t)address & ~(cache_line_size - 1));
    uint64_t end = start + size;

    asm volatile("isb sy");

    do {
        asm volatile(""
                "dc civac, %0\n"
                "dsb ish\n"
                "isb sy\n"
                : : "r" (start));

        start += cache_line_size;
    } while (start < end);
}

void icache_invalidate_PoU(void *address, size_t size){
    const size_t cache_line_size = 64;

    size = (size + cache_line_size) & ~(cache_line_size - 1);

    uint64_t start = ((uint64_t)address & ~(cache_line_size - 1));
    uint64_t end = start + size;

    asm volatile("isb sy");

    do {
        asm volatile(""
                "ic ivau, %0\n"
                "dsb ish\n"
                "isb sy\n"
                : : "r" (start));

        start += cache_line_size;
    } while (start < end);
}
