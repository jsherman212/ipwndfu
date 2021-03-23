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

/* Format specifiers currently supported:
 *  %s, %d
 *
 * No width, padding, etc, only 0x
 */
void aop_sram_vsnprintf(char *buf, size_t n, const char *fmt,
        va_list args){
    if(n == 0)
        return;

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

                    if(arg < 0){
                        *bufp++ = '-';
                        left--;

                        /* Make positive */
                        arg *= -1;
                    }

                    /* Get digits */
                    char digits[10];

                    for(int i=0; i<sizeof(digits); i++)
                        digits[i] = '\0';

                    int thisdig = sizeof(digits) - 1;

                    while(arg){
                        int digit = arg % 10;
                        digits[thisdig--] = (char)(digit + '0');
                        arg /= 10;
                    }

                    int startdig = 0;

                    while(digits[startdig] == '\0')
                        startdig++;

                    while(left > 1 && startdig < sizeof(digits)){
                        *bufp++ = digits[startdig++];
                        left--;
                    }

                    fmtp++;
                    break;
                }
            default:
                break;
        };
    }

    if(left > 0)
        *bufp = '\0';
}
