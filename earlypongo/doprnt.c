#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "doprnt.h"

static void flagprocess(size_t ndigits, bool left_adjust, bool zeroX,
        int fieldwidth, int prec, void (*putc)(char, struct doprnt_info *),
        struct doprnt_info *di){
    int needed_zeros = prec - ndigits;
    int needed_width = fieldwidth - ndigits;

    if(fieldwidth != -1){
        if(prec != -1){
            /* If the field width is larger than the precision,
             * we need to account for that and print blanks */
            if(!left_adjust && fieldwidth > prec){
                int nspaces = fieldwidth - prec;

                while(--nspaces >= 0)
                    putc(' ', di);
            }

            while(--needed_zeros >= 0)
                putc('0', di);
        }
        else if(!left_adjust){
            while(--needed_width >= 0)
                putc(' ', di);

            if(zeroX){
                putc('0', di);
                putc('x', di);
            }
        }
    }
    else{
        if(prec != -1){
            while(--needed_zeros >= 0)
                putc('0', di);
        }
    }
}

static void prntnum(int64_t num, bool left_adjust, bool zero_pad,
        int fieldwidth, int prec, void (*putc)(char, struct doprnt_info *),
        struct doprnt_info *di){
    if(num < 0){
        putc('-', di);
        /* Make positive */
        num *= -1;
    }

    /* Get digits */
    char digits[20];

    for(int i=0; i<sizeof(digits); i++)
        digits[i] = '\0';

    int thisdig = sizeof(digits) - 1;

    if(num == 0)
        digits[thisdig] = '0';
    else{
        while(num){
            digits[thisdig--] = (char)((int)(num % 10) + '0');
            num /= 10;
        }
    }

    int startdig = 0;

    while(digits[startdig] == '\0')
        startdig++;

    size_t ndigits = sizeof(digits) - startdig;

    flagprocess(ndigits, left_adjust, false, fieldwidth, prec, putc, di);

    if(prec == -1)
        prec = INT_MAX;

    while(startdig < sizeof(digits) && --prec >= 0)
        putc(digits[startdig++], di);
}

static void prnthex(uint8_t *bytes, size_t len, bool left_adjust,
        bool zero_pad, bool zeroX, int fieldwidth, int prec,
        void (*putc)(char, struct doprnt_info *),
        struct doprnt_info *di){
    const char *hex = "0123456789abcdef";
    bool first_nonzero = false;
    size_t ndigits = 0;
    char digits[17];

    for(int i=0; i<sizeof(digits); i++)
        digits[i] = '\0';

    for(size_t i=0; i<len; i++){
        char one = hex[(*bytes >> 4) & 0xf];
        char two = hex[*bytes++ & 0xf];

        if(first_nonzero){
            digits[ndigits++] = one;
            digits[ndigits++] = two;
        }
        else{
            if(one != '0'){
                first_nonzero = true;
                digits[ndigits++] = one;
            }

            if(two != '0' && !first_nonzero){
                first_nonzero = true;
                digits[ndigits++] = two;
            }
            else if(first_nonzero){
                digits[ndigits++] = two;
            }
        }
    }

    /* A single zero is still a digit */
    if(!first_nonzero)
        ndigits = 1;

    flagprocess(ndigits, left_adjust, zeroX, fieldwidth, prec, putc, di);

    /* If there were flags, the 0x was already put */
    if(zeroX && !left_adjust && !zero_pad && prec == -1 &&
            fieldwidth == -1){
        putc('0', di);
        putc('x', di);
    }

    if(prec == -1)
        prec = INT_MAX;

    /* Entire number was zero */
    if(!first_nonzero)
        putc('0', di);
    else{
        for(size_t i=0; i<ndigits; i++){
            if(--prec < 0)
                break;

            putc(digits[i], di);
        }
    }
}

static void prntflt(double num, bool left_adjust, int fieldwidth,
        int prec, void (*putc)(char, struct doprnt_info *),
        struct doprnt_info *di){
    if(num == 0){
        putc('0', di);
        putc('.', di);
        putc('0', di);
        return;
    }

    if(num < 0){
        putc('-', di);
        /* Make positive */
        num *= -1;
    }

    /* Cursed case: the number of digits in the integral part of this
     * number is larger than the number of digits in INT64_MAX. This
     * means it cannot be represented as a 64-bit integer, which makes
     * it impossible to do the divide-by-ten and modulus trick to get
     * the digits. TODO: scientific notation? */
    if(num > (double)INT64_MAX){
        putc('<', di);
        putc('l', di);
        putc('r', di);
        putc('g', di);
        putc('f', di);
        putc('>', di);
        return;
    }

    /* Print the integral part */
    prntnum((int64_t)num, false, false, -1, -1, putc, di);

    /* Print the decimal */
    putc('.', di);

    /* Print the fractional part */
    uint64_t powten;

    if(prec == -1){
        /* If no precision specified, print only 6 digits
         * in the fractional part, padding with zeros */
        powten = 100000;
    }
    else{
        powten = 1;

        /* Gross pow */
        while(--prec >= 0)
            powten *= 10;
    }

    float frac = num - (int64_t)num;
    int64_t ifrac = (frac * powten);

    /* Could this happen? */
    if(ifrac < 0)
        ifrac *= -1;

    /* Already took care of precision here */
    prntnum(ifrac, false, false, -1, -1, putc, di);
}

static int64_t getnum(char **s){
    int64_t res = 0;
    bool neg = false;

    if(**s == '-'){
        neg = true;
        (*s)++;
    }

    while(**s && **s >= '0' && **s <= '9'){
        res = (res * 10) + (**s - '0');
        (*s)++;
    }

    if(neg)
        res *= -1;

    return res;
}

static bool is_specifier(char c){
    return c == '%' || c == 'c' || c == 's' || c == 'd' || c == 'x' ||
        c == 'p' || c == 'l' || c == 'f';
}

int doprnt(const char *fmt, void (*putc)(char, struct doprnt_info *),
        struct doprnt_info *info, va_list args){
    if(info && info->remaining == 0)
        return 0;

    char *fmtp = (char *)fmt;
    int printed = 0;

    while(*fmtp){
        /* As long as we don't see a format specifier, just copy
         * the string */
        while(*fmtp && *fmtp != '%')
            putc(*fmtp++, info);

        /* We done? */
        if(*fmtp == '\0')
            break;

        /* Get off the '%' */
        fmtp++;

        bool left_adjust = false;
        bool zero_pad = false;
        bool zeroX = false;
        int prec = -1;
        int fieldwidth = -1;

        /* Parse flags */
        while(*fmtp && !is_specifier(*fmtp)){
            if(*fmtp == '-'){
                fmtp++;
                left_adjust = true;
            }
            else if(*fmtp == '0' && !(fmtp[1] >= '0' && fmtp[1] <= '9')){
                fmtp++;
                zero_pad = true;
            }
            else if(*fmtp == '#'){
                fmtp++;
                zeroX = true;
            }
            else if(*fmtp == '.'){
                fmtp++;
                prec = (int)getnum(&fmtp);
            }
            else if(*fmtp >= '0' && *fmtp <= '9'){
                fieldwidth = (int)getnum(&fmtp);
            }
            else{
                fmtp++;
            }
        }

        /* Hit the end of the string? */
        if(*fmtp == '\0')
            break;

        /* Ignore '0' when '-' is present */
        if(left_adjust)
            zero_pad = false;

        switch(*fmtp){
            case '%':
                {
                    putc('%', info);
                    fmtp++;
                    break;
                }
            case 'c':
                {
                    char ch = va_arg(args, int);
                    putc(ch, info);
                    fmtp++;
                    break;
                }
            case 's':
                {
                    char *arg = va_arg(args, char *);

                    if(prec == -1)
                        prec = INT_MAX;

                    while(*arg && --prec >= 0)
                        putc(*arg++, info);

                    fmtp++;

                    break;
                }
            case 'd':
                {
                    int arg = va_arg(args, int);
                    prntnum(arg, left_adjust, zero_pad, fieldwidth,
                            prec, putc, info);

                    fmtp++;
                    break;
                }
            case 'x':
                {
                    uint32_t arg = __builtin_bswap32(va_arg(args, uint32_t));
                    uint8_t *bytes = (uint8_t *)&arg;
                    prnthex(bytes, sizeof(arg), left_adjust, zero_pad,
                            zeroX, fieldwidth, prec, putc, info);
                    fmtp++;
                    break;
                }
            case 'p':
                {
                    uintptr_t arg = __builtin_bswap64(va_arg(args, uintptr_t));
                    uint8_t *bytes = (uint8_t *)&arg;
                    prnthex(bytes, sizeof(arg), left_adjust, zero_pad,
                            true, fieldwidth, prec, putc, info);
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

                            prnthex(bytes, sizeof(arg), left_adjust,
                                    zero_pad, zeroX, fieldwidth, prec,
                                    putc, info);
                        }
                        else if(fmtp[2] == 'd'){
                            int64_t arg = va_arg(args, int64_t);
                            prntnum(arg, left_adjust, zero_pad, fieldwidth,
                                    prec, putc, info);
                        }

                        fmtp += 3;
                    }

                    break;
                }
                /* Double */
            case 'f':
                {
                    double arg = va_arg(args, double);
                    prntflt(arg, left_adjust, fieldwidth, prec, putc, info);
                    fmtp++;
                    break;
                }
            default:
                break;
        };
    }

    return info->written;
}
