#include <math.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>
#include <time.h>
#include <unistd.h>

static void DumpMemory(void *data, size_t size){
    char ascii[17];
    size_t i, j;
    ascii[16] = '\0';
    int putloc = 0;
    void *curaddr = data;
    for (i = 0; i < size; ++i) {
        if(!putloc){
            printf("%p: ", curaddr);
            curaddr += 0x10;
            putloc = 1;
        }

        printf("%02X ", ((unsigned char*)data)[i]);
        if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
            ascii[i % 16] = ((unsigned char*)data)[i];
        } else {
            ascii[i % 16] = '.';
        }
        if ((i+1) % 8 == 0 || i+1 == size) {
            printf(" ");
            if ((i+1) % 16 == 0) {
                printf("|  %s \n", ascii);
                putloc = 0;
            } else if (i+1 == size) {
                ascii[(i+1) % 16] = '\0';
                if ((i+1) % 16 <= 8) {
                    printf(" ");
                }
                for (j = (i+1) % 16; j < 16; ++j) {
                    printf("   ");
                }
                printf("|  %s \n", ascii);
                putloc = 0;
            }
        }
    }
}

static char *logbuf = NULL;
static char *logend = NULL;
static char *readp = NULL;
static char *writep = NULL;

static size_t logsz = 0x4000;
static size_t retbufsz = 0x800;
static size_t loglen = 0;

/* We re-use the same dynamically allocated message buffer to construct
 * the format strings since we have limited stack space in SecureROM */
static char *msgbuf = NULL;

/* We re-use the same buffer for returning log contents to the caller. */
static char *retbuf = NULL;

static void ptrdump(void){
    printf("readp: %p\n"
            "writep: %p\n"
            "loglen: %#llx\n",
            readp, writep, loglen);
}

__printflike(1, 2) void dbglog(const char *fmt, ...){
    if(loglen == logsz){
        printf("%s: log is full\n", __func__);
        return;
    }

    va_list args;
    va_start(args, fmt);

    /* We do not copy nul terminators into the log. msgbuf has been
     * given logsz+1 bytes of memory so messages that are exactly the
     * log size are not truncated. */
    /* vsnprintf(msgbuf, logsz + 1, fmt, args); */
    size_t written = vsnprintf(msgbuf, logsz - 1, fmt, args);

    va_end(args);

    char *msgp = msgbuf;
    size_t msglen = strlen(msgp);

    if(msglen == 0)
        return;

    /* If this message isn't the size of the log, we do not want
     * to include the nul terminator */
    /* if(msglen != logsz) */
    /*     msglen--; */

    if(readp <= writep){
        /* We have not wrapped around to the beginning
         * of the log buffer yet. Check if we have enough space
         * to write this message. Note that we may end up wrapping
         * around. There's two things we gotta check:
         *  1. Do we have enough space at where our write pointer is
         *     to just copy the message and be done?
         *  2. If we don't have enough space at where our write pointer
         *     is, is there enough space to wrap around (aka enough bytes
         *     for the rest of the message before the read pointer)
         */
        if(writep + msglen < logend){
            memmove(writep, msgp, msglen);
            writep += msglen;
            loglen += msglen;
            return;
        }

        /* printf("%s: message: '%s'\n", __func__, msgp); */
        /* ptrdump(); */

        /* How many bytes can we write before we hit the end of the buffer? */
        size_t canwrite = logend - writep;

        /* How many bytes are waiting to be written after we wrap around? */
        size_t pending = msglen - canwrite;

        /* How much space do we have when we wrap around? */
        size_t wrapspace = readp - logbuf;

        /* printf("%s: canwrite %zu pending %zu wrapspace %zu\n", __func__, */
        /*         canwrite, pending, wrapspace); */

        /* Copy what we can to the end of the log buffer */
        memmove(writep, msgp, canwrite);

        msgp += canwrite;
        writep += canwrite;
        loglen += canwrite;

        /* Check if we're able to copy the rest if we wrap around */
        if(pending > wrapspace){
            /* printf("%s: not enough space to wrap around. pending %zu" */
            /*         " wrapspace %zu\n", __func__, pending, wrapspace); */
            return;
        }

        /* Wrap around to the beginning */
        memmove(logbuf, msgp, pending);

        writep = logbuf + pending;
        loglen += pending;
    }
    else{
        /* We have wrapped around to the beginning of the log
         * buffer. We don't want to overflow into anything that hasn't
         * been read out yet, so keep that in mind when calculating the
         * length of the message we can copy. */
        size_t canwrite = readp - writep;

        /* printf("%s: we can copy %zu bytes without overflowing into readp\n", */
        /*         __func__, canwrite); */

        /* Make sure we don't write more than we have in the message */
        if(canwrite > msglen)
            canwrite = msglen;

        memmove(writep, msgp, canwrite);

        writep += canwrite;
        loglen += canwrite;
    }
}

/* Read out the log in 0x800-byte increments. This function is meant to be
 * called inside a loop until the log is emptied. Weird things may happen
 * if someone writes to the log while it's getting read out.
 *
 * The returned buffer isn't nul terminated, the size is denoted by *lenp. */
char *getlog(size_t *lenp){
    /* Nothing to read */
    if(loglen == 0){
        *lenp = 0;
        return retbuf;
    }

    size_t len;

    /* Copy what we can between readp and writep. Unfortunately, this range
     * isn't guarenteed to be contiguous, so I'm using another buffer
     * to abstract that away from the caller. If this range is contiguous,
     * then writep will be more than readp. If it isn't, then writep will
     * be less than readp. */
    if(writep > readp){
        size_t chunk = writep - readp;

        if(chunk > retbufsz)
            chunk = retbufsz;

        memmove(retbuf, readp, chunk);
        /* printf("%s: copied [%p, %p)\n", __func__, readp, logbuf + chunk); */

        readp += chunk;
        len = chunk;
        /* *lenp = outsz; */
    }
    else{
        /* First, get the part covered by [readp, logend) */
        size_t firstchunk = logend - readp;
        bool get_secondchunk = true;

        if(firstchunk >= retbufsz){
            firstchunk = retbufsz;
            get_secondchunk = false;
        }

        memmove(retbuf, readp, firstchunk);

        /* printf("%s: FIRST CHUNK: copied [%p, %p)\n", __func__, readp, */
        /*         readp + firstchunk); */

        readp += firstchunk;
        len = firstchunk;

        /* If we can, get the second part covered by [logbuf, writep) */
        if(get_secondchunk){
            /* Figure out how much we can copy from the second part */
            size_t limit = retbufsz - firstchunk;
            size_t secondchunk = writep - logbuf;

            if(secondchunk >= limit)
                secondchunk = limit;

            memmove(retbuf + firstchunk, logbuf, secondchunk);

            /* printf("%s: SECOND CHUNK: copied [%p, %p)\n", __func__, logbuf, */
            /*         logbuf + secondchunk); */

            readp = logbuf + secondchunk;
            len += secondchunk;
        }

        /* memmove(retbuf, readp, firstsz); */
        /* printf("%s: copied [%p, %p)\n", __func__, readp, logend); */
        /* Second, get the part covered by [logbuf, writep) */
        /* size_t secondsz = writep - logbuf; */
        /* memmove(retbuf + firstsz, logbuf, secondsz); */
        /* printf("%s: copied [%p, %p)\n", __func__, logbuf, writep); */
        /* *lenp = firstsz + secondsz; */
    }

    loglen -= len;
    *lenp = len;

    /* loglen = 0; */
    /* readp = writep; */

    return retbuf;
}

bool loginit(void){
    logbuf = malloc(logsz);

    if(!logbuf){
        printf("%s: malloc failed\n", __func__);
        return false;
    }

    msgbuf = malloc(logsz + 1);

    if(!msgbuf){
        printf("%s: malloc failed\n", __func__);
        return false;
    }

    retbuf = malloc(retbufsz);

    if(!retbuf){
        printf("%s: malloc failed\n", __func__);
        return false;
    }

    memset(logbuf, '%', logsz);
    memset(msgbuf, '$', logsz + 1);
    memset(retbuf, '9', retbufsz);

    logend = logbuf + logsz;
    readp = writep = logbuf;// + 0x3625;

    return true;
}

static void logtest_readp_before_writep(void){
    /* dbglog("%s: First write\n", __func__); */
    /* ptrdump(); */
    /* DumpMemory(logbuf, logsz); */
    /* dbglog("A"); */

    /* char big[0xf0]; */
    /* memset(big, '%', sizeof(big)); */
    /* big[sizeof(big)-1] = '\0'; */

    /* dbglog("Some pointer: %p", malloc(50)); */
    /* dbglog("Some pointer: %p", malloc(50)); */

    /* dbglog("%sABCDEF", big); */
    /* dbglog("123456789"); */
    dbglog("123");
    dbglog("456");

    /* for(int i=0; i<25; i++){ */
        /* if(i==24) */
        /*     dbglog("write %d!\n", i); */
        /* else */
            /* dbglog("write %d\n", i); */
    /* } */
    /*         dbglog("write %d\n", 24); */
    ptrdump();
    DumpMemory(logbuf, logsz);
}

static void logtest_writep_before_readp(void){
    /* These tests rely on the log buffer starting at +0x50 */
    /* Put writep before readp */
    char big[0xb0];
    memset(big, '%', sizeof(big));
    big[sizeof(big)-1] = '\0';

    dbglog("%sABCDEFGHIJKLMNOPFDSKLFDSJKL", big);

    ptrdump();
    DumpMemory(logbuf, logsz);

    char another[0x36];
    memset(another, '$', sizeof(another));
    another[sizeof(another)-1] = '\0';
    dbglog("%sAB", another);

    ptrdump();
    DumpMemory(logbuf, logsz);
}

/* #define FIRST_READ_TEST */
/* #define SECOND_READ_TEST */
/* #define THIRD_READ_TEST */
#define FOURTH_READ_TEST

static void logtest_read(void){
    size_t len;
    char *log;
#ifdef FIRST_READ_TEST
    /* First test: put 4 chars in and read them out */
    dbglog("ABCD");

    len = 0;
    log = getlog(&len);

    DumpMemory(log, len);
    puts("");

    if(memcmp(log, "ABCD", 4) != 0){
        printf("%s: first test failed\n", __func__);
        abort();
    }

    printf("First test passed\n");

    ptrdump();
    DumpMemory(logbuf, logsz);
#endif

#ifdef SECOND_READ_TEST
    /* Second test: read a full log. Need to inspect visually */
    char big[logsz - 0x10];
    memset(big, '%', sizeof(big));
    big[sizeof(big)-1] = '\0';

    dbglog("%s", big);
    dbglog("ABCDEFGHIJKLMNOPQR");

    ptrdump();
    DumpMemory(logbuf, logsz);
    puts("");
    
    len = 0;
    log = getlog(&len);

    DumpMemory(log, len);
    puts("");
#endif

#ifdef THIRD_READ_TEST
    /* Third test: read an almost-full log, which will fluctuate the
     * read and write pointers */
    char big[logsz - 0x10];
    memset(big, '%', sizeof(big));
    big[sizeof(big)-1] = '\0';

    dbglog("%s", big);
    ptrdump();
    dbglog("ABCDEFG");
    ptrdump();

    /* DumpMemory(logbuf, logsz); */
    /* puts(""); */
    
    len = 0;
    log = getlog(&len);

    /* ptrdump(); */
    /* DumpMemory(logbuf, logsz); */
    /* puts(""); */

    DumpMemory(log, len);
    puts("");
#endif

#ifdef FOURTH_READ_TEST
    /* Fourth test: fluctuate read/write pointers with buffers of
     * random sizes */
    for(int i=0; ; i++){
        uint32_t bufsz = arc4random_uniform(logsz);
        
        if(bufsz == 0)
            bufsz = 2;

        char *buf = malloc(bufsz);

        if(!buf){
            printf("%s: malloc failed\n", __func__);
            abort();
        }

        arc4random_buf(buf, bufsz);
        buf[bufsz - 1] = '\0';

        for(size_t i=0; i<(bufsz - 1); i++){
            if(buf[i] == '\0')
                buf[i]++;
        }

        dbglog("%s", buf);

        len = 0;
        log = getlog(&len);

        /* Ignore the last character, since that's the nul terminator
         * and that's not written to the log */
        if(memcmp(buf, log, bufsz - 1) != 0){
            printf("%d: Written log and read log do not match!\n", i);
            printf("Written to log:\n");
            DumpMemory(buf, bufsz - 1);
            printf("Read back:\n");
            DumpMemory(log, len);
            abort();
        }

        if(i%100000 == 0){
            printf("Good since %d iterations\n", i);
            /* printf("Written to log:\n"); */
            /* DumpMemory(buf, bufsz - 1); */
            /* printf("Read back:\n"); */
            /* DumpMemory(log, len); */
            /* printf("Actual log contents:\n"); */
            /* DumpMemory(logbuf, logsz); */
        }

        free(buf);
    }
#endif
}

#define NEWREAD_1
/* #define NEWREAD_2 */

static void logtest_newread(void){
    size_t len;
    char *log;

#ifdef NEWREAD_1
    /* char letters[] = { 'A', 'B', 'C', 'D', 'E' }; */
    /* for(int i=0; i<sizeof(letters)/sizeof(*letters); i++){ */
    /*     for(int k=0; k<0x800; k++){ */
    /*         dbglog("%c", letters[i]); */
    /*     } */
    /* } */
    dbglog("ABCD");
    DumpMemory(msgbuf, 0x10);
    ptrdump();

    /* do { */
    /* for(int i=0; i<10; i++){ */
    /*     dbglog("%s: out buf %#llx\n", __func__, (uint64_t)letters); */
    /*     len = 0; */
    /*     log = getlog(&len); */
    /*     DumpMemory(log, len); */
    /*     ptrdump(); */
    /* } */
    /* } while (len > 0); */

    /* DumpMemory(logbuf, logsz); */
    /* dbglog("%s: Hello\n", __func__); */
    /* DumpMemory(logbuf, logsz); */
    /* len = 0; */
    /* log = getlog(&len); */
    /* DumpMemory(log, len); */
    /* ptrdump(); */


#endif

#ifdef NEWREAD_2
    /* Fourth test: fluctuate read/write pointers with buffers of
     * random sizes */
    for(int i=0; ; i++){
        uint32_t bufsz = arc4random_uniform(logsz);
        
        if(bufsz == 0)
            bufsz = 2;

        char *buf = malloc(bufsz);

        if(!buf){
            printf("%s: malloc failed\n", __func__);
            abort();
        }

        arc4random_buf(buf, bufsz);
        buf[bufsz - 1] = '\0';

        for(size_t i=0; i<(bufsz - 1); i++){
            if(buf[i] == '\0')
                buf[i]++;
        }

        dbglog("%s", buf);
        char *bufp = buf;
        char *logp = logbuf;

        do {
            len = 0;
            log = getlog(&len);
            if(memcmp(bufp, log, len) != 0){
                printf("%d: Written log and read log do not match!\n", i);
                printf("Written to log:\n");
                DumpMemory(buf, bufsz - 1);
                printf("Read back:\n");
                DumpMemory(log, len);
                abort();
            }
            bufp += len;
        } while (len > 0);

        free(buf);
    }
#endif
}

static size_t my_strlen(char *s){
    char *p = s;

    while(*p)
        p++;

    return p - s;
}

static void prntnum(char **bufp, int64_t num, size_t *leftp){
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

        printf("%s: digit %d char %c\n", __func__, digit,
                digit + '0');
        digits[thisdig--] = (char)(digit + '0');
        num /= 10;
    }

    int startdig = 0;

    while(digits[startdig] == '\0')
        startdig++;

    for(int i=startdig; i<sizeof(digits); i++)
        printf("!!! %c\n", digits[i]);

    while(*leftp > 1 && startdig < sizeof(digits)){
        printf("%s: cur digit %c\n", __func__, digits[startdig]);
        *(*bufp)++ = digits[startdig++];
        (*leftp)--;
    }
}

static void prnthex(char **bufp, uint8_t *bytes, size_t len,
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

/* Format specifiers supported:
 *  %%, %c, %s, %d, %x, %p, %llx, %lld
 *
 * No width, padding, etc, only 0x ('#')
 */
static void my_vsnprintf(char *buf, size_t n, const char *fmt,
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

    while(*fmtp){// && left > 1){
        printf("%s: first: '%s'\n", __func__, fmtp);
        /* As long as we don't see a format specifier, just copy
         * the string */
        while(*fmtp && *fmtp != '%'){
            if(left > 1){
                *bufp++ = *fmtp;
                printf("%s: left: %zu\n", __func__, left);
                left--;
            }

            fmtp++;
        }

        printf("'%s'\n", fmtp);

        /* We done? */
        if(*fmtp == '\0')
            break;

        /* printf("%s\n", fmtp); */
        /* abort(); */

        /* Get off the '%' */
        fmtp++;

        printf("'%s'\n", fmtp);
        bool zeroX = false;

        if(*fmtp == '#'){
            zeroX = true;
            fmtp++;
        }

        printf("'%s'\n", fmtp);

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
                printf("%s: unhandled case '%s'\n", __func__, fmtp);
                abort();
        };
    }

    printf("%s: left %zu\n", __func__, left);

    if(left > 0)
        *bufp = '\0';
}

 __printflike(3, 4) static void my_snprintf(char *str, size_t n,
         const char *fmt, ...){
     va_list args;
     va_start(args, fmt);

     my_vsnprintf(str, n, fmt, args);

     va_end(args);
 }

static void vsnprintf_tests(void){
    char buf[0x200];
    /* char buf[0x3]; */

    /* my_snprintf(buf, sizeof(buf), "%s%#x%%%d %p Hello world! %llx A%c %x", */
    /*         __func__, -2147483647, 45004, buf, buf, 'U', (uint32_t)buf); */

    my_snprintf(buf, sizeof(buf), "%s%#x%%%d %p Hello world! %llx A%c %x %lld %lld",
            __func__, -2147483647, -45004, (void *)buf,
            (uint64_t)buf, 'U', 84943, LONG_MAX, LONG_MAX+1);
    /* snprintf(buf, sizeof(buf), "%#x", 2147483647); */
    /* snprintf(buf, sizeof(buf), "%p", buf); */
    printf("'%s'\n", buf);
    
    /* Normal string, no format specifiers */
    /* my_snprintf(buf, sizeof(buf), "Hello!"); */
    /* printf("'%s'\n", buf); */

    /* my_snprintf(buf, sizeof(buf), "Hello %s!!!", "World"); */
    /* printf("'%s'\n", buf); */

    /* int num = rand() % 500000; */
    /* printf("num: %d\n", num); */
    /* num = -2147483647; */
    /* my_snprintf(buf, sizeof(buf), "Hello %d!", num); */
    /* printf("'%s'\n", buf); */
}

struct doprnt_info {
    char *buf;
    size_t remaining;
};

static void flagprocess(size_t ndigits, bool left_adjust, bool zeroX,
        int fieldwidth, int prec, void (*putc)(char, void *),
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

static void prntnum2(int64_t num, bool left_adjust, bool zero_pad,
        int fieldwidth, int prec, void (*putc)(char, void *),
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
        digits[sizeof(digits) - 1] = '0';
    else{
        while(num){
            int digit = num % 10;

            /* XXX Needed? */
            if(digit < 0)
                digit *= -1;

            /* printf("%s: digit %d char %c\n", __func__, digit, */
            /*         digit + '0'); */
            digits[thisdig--] = (char)(digit + '0');
            num /= 10;
        }
    }

    int startdig = 0;

    while(digits[startdig] == '\0')
        startdig++;

    size_t ndigits = sizeof(digits) - startdig;
    /* printf("%s: there are %lld digits\n", __func__, ndigits); */

    flagprocess(ndigits, left_adjust, false, fieldwidth, prec, putc, di);

    if(prec == -1)
        prec = INT_MAX;

    while(startdig < sizeof(digits) && --prec >= 0)
        putc(digits[startdig++], di);
}

static void prnthex2(uint8_t *bytes, size_t len, bool left_adjust,
        bool zero_pad, bool zeroX, int fieldwidth, int prec,
        void (*putc)(char, void *), struct doprnt_info *di){
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
        int prec, void (*putc)(char, void *), struct doprnt_info *di){
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
    prntnum2((int64_t)num, false, false, -1, -1, putc, di);

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
    prntnum2(ifrac, false, false, -1, -1, putc, di);
}

static void printf_putc(char c, void *arg){
    /* Not really needed, but just for consistency */
    struct doprnt_info *di = arg;

    if(di->remaining > 1){
        putchar(c);
        di->remaining--;
    }
}

static void snprintf_putc(char c, void *arg){
    struct doprnt_info *di = arg;

    if(di->remaining > 1){
        *di->buf++ = c;
        di->remaining--;
    }
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
        /* printf("%s: res %lld\n", __func__, res); */
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

static int _doprnt(const char *fmt, void (*putc)(char, void *),
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

        /* printf("'%s'\n", fmtp); */

        /* We done? */
        if(*fmtp == '\0')
            break;

        /* Get off the '%' */
        fmtp++;

        /* printf("'%s'\n", fmtp); */
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

        /* printf("left_adjust: %d\n", left_adjust); */
        /* printf("zero_pad: %d\n", zero_pad); */
        /* printf("zeroX: %d\n", zeroX); */
        /* printf("prec: %d\n", prec); */
        /* printf("field width: %d\n", fieldwidth); */

        /* printf("'%s'\n", fmtp); */

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
                    /* prntnum2(arg, left_adjust, fieldwidth, prec, putc, info); */
                    prntnum2(arg, left_adjust, zero_pad, fieldwidth,
                            prec, putc, info);

                    fmtp++;
                    break;
                }
            case 'x':
                {
                    uint32_t arg = __builtin_bswap32(va_arg(args, uint32_t));
                    uint8_t *bytes = (uint8_t *)&arg;
                    /* prnthex2(bytes, sizeof(arg), zeroX, prec, putc, info); */
                    prnthex2(bytes, sizeof(arg), left_adjust, zero_pad,
                            zeroX, fieldwidth, prec, putc, info);
                    fmtp++;
                    break;
                }
            case 'p':
                {
                    uintptr_t arg = __builtin_bswap64(va_arg(args, uintptr_t));
                    uint8_t *bytes = (uint8_t *)&arg;
                    /* prnthex2(bytes, sizeof(arg), true, prec, putc, info); */
                    prnthex2(bytes, sizeof(arg), left_adjust, zero_pad,
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

                            prnthex2(bytes, sizeof(arg), left_adjust,
                                    zero_pad, zeroX, fieldwidth, prec,
                                    putc, info);
                        }
                        else if(fmtp[2] == 'd'){
                            int64_t arg = va_arg(args, int64_t);
                            prntnum2(arg, left_adjust, zero_pad, fieldwidth,
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
                    /* printf("arg: %f\n", arg); */
                    prntflt(arg, left_adjust, fieldwidth, prec, putc, info);
                    fmtp++;
                    break;
                }
            default:
                printf("%s: unhandled case '%s'\n", __func__, fmtp);
                abort();
        };
    }

    return printed;
}

static int doprnt_printf_wrapper(const char *fmt, ...){
    va_list args;
    va_start(args, fmt);

    struct doprnt_info di;
    di.buf = NULL;
    di.remaining = 999999999;

    int w = _doprnt(fmt, printf_putc, &di, args);

    va_end(args);

    return w;
}

static int doprnt_snprintf_wrapper(char *buf, size_t n,
        const char *fmt, ...){
    va_list args;
    va_start(args, fmt);

    struct doprnt_info di;
    di.buf = buf;
    di.remaining = n;

    int w = _doprnt(fmt, snprintf_putc, &di, args);

    va_end(args);

    if(di.remaining > 0)
        *di.buf = '\0';

    return w;
}

static void doprnt_tests(void){
    printf("%lld\n", INT64_MAX);
    doprnt_printf_wrapper("%% Hello world! %%\n");
    doprnt_printf_wrapper("%% %c Hello%cworld! %%\n", 'A', '_');
    doprnt_printf_wrapper("%% %c Hello%cworld! %s %%\n", 'A',
            '_', "Justin");
    doprnt_printf_wrapper("%% %d %c Hello%cworld! %s %d %%\n", INT64_MAX+1,
            'A', '_', "Justin", 414243);
    doprnt_printf_wrapper("%% %d %#x %c Hello%cworld! %s %d %%\n",
            INT64_MAX+1, 0x12345678, 'A', '_', "Justin", 414243);
    doprnt_printf_wrapper("%% %p %lld %#llx %d %#x %c Hello%cworld! %s %d %%\n",
            doprnt_tests, INT64_MAX, INT64_MAX, -1111, 0x11022, 'A', '_',
            "Justin", 414243);

    char testbuf[0x100];
    doprnt_snprintf_wrapper(testbuf, sizeof(testbuf),
            "%% Hello world! %%\n");
    printf("testbuf: '%s'\n", testbuf);
    snprintf(testbuf, sizeof(testbuf),
            "%% Hello world! %%\n");
    printf("testbuf: '%s'\n", testbuf);
    doprnt_snprintf_wrapper(testbuf, sizeof(testbuf),
            "%% %c Hello%cworld! %%\n_", 'A', '_');
    printf("testbuf: '%s'\n", testbuf);

    doprnt_snprintf_wrapper(testbuf, sizeof(testbuf),
            "%% %c Hello%cworld! %s %%\n", 'A', '_', "Justin");
    printf("testbuf: '%s'\n", testbuf);
    snprintf(testbuf, sizeof(testbuf),
            "%% %c Hello%cworld! %s %%\n", 'A', '_', "Justin");
    printf("testbuf: '%s'\n", testbuf);

    doprnt_snprintf_wrapper(testbuf, sizeof(testbuf),
            "%s: %% %d %c Hello%cworld! %s %d %%\n", __func__, INT_MAX,
            'A', '_', "Justin", -414243);
    printf("testbuf: '%s'\n", testbuf);

    doprnt_snprintf_wrapper(testbuf, sizeof(testbuf),
            "%% %p %d %#x %c Hello%cworld! %s %d %%\n", doprnt_tests,
            INT64_MAX+1, 0x11022, 'A', '_', "Justin", 414243);
    printf("testbuf: '%s'\n", testbuf);

    doprnt_snprintf_wrapper(testbuf, sizeof(testbuf),
            "%s%% %p %lld %#llx %d %#x %c Hello%cworld! %s %d %%\n",
            __func__, doprnt_tests, 0, 0x987654321abcde, -1111, 0x11022,
            'A', '_', "Justin", 414243);
    printf("testbuf: '%s'\n", testbuf);
    float f = 3.147;

    /* doprnt_snprintf_wrapper(testbuf, sizeof(testbuf), */
    /*         "%s%% %f %p %lld %#llx %d %#x %c Hello%cworld! %s %d %%\n", */
    /*         __func__, 8932983289432894298489248932.4039290, */
    /*         doprnt_tests, 0, 0x987654321abcde, -1111, 0x11022, */
    /*         'A', '_', "Justin", 414243); */
    /* printf("testbuf: '%s'\n", testbuf); */
    double huge = 1111222233334444555566667777.88889999;
    huge = 33232.4445;
    /* printf("%.55f\n", huge); */

    doprnt_snprintf_wrapper(testbuf, sizeof(testbuf),
            "%s%% %.5f %p %lld %#llx %d %#x %c Hello%cworld! %.1s %.3d %%\n",
            __func__, huge,
            doprnt_tests, 0, 0x987654321abcde, -1111, 0x11022,
            'A', '_', "Justin", 414243);
    printf("testbuf: '%s'\n", testbuf);
    printf("huge: %f\n", huge);

    uint64_t val = (uint64_t)doprnt_tests;
    uint64_t val2 = (uint64_t)prntflt;
    /* printf("0x%16.16llx 0x%16llx\n", val, val2); */
    printf("0x%16.16p 0x%16p\n", val, val2);

    /* printf("0. %-9.9llx\n", 0x55uLL); */
    /* printf("1. %10llx\n", 0x55uLL); */
    /* printf("2. %10llx\n", 0x555uLL); */
    /* printf("3. %-10llx\n", 0x55uLL); */
    /* printf("4. %-10.10llx\n", 0x55uLL); */
    /* printf("5. %10.10llx\n", 0x55uLL); */
    /* printf("6. %15.10llx\n", 0x55uLL); */
    /* printf("7. %11.10llx\n", 0x55uLL); */
    /* printf("8. %-.19llx\n", 0x445566uLL); */
    /* printf("9. %0.5llx\n", 0x11uLL); */
    /* printf("10. %-.5llx\n", 0x11uLL); */
    /* printf("11. %.5llx\n", 0x11uLL); */
    /* printf("12. %-15.10llx\n", 0x55uLL); */
    /* printf("13. %-.19llx\n", 0uLL); */
    /* printf("14. %llx\n", 0uLL); */

    printf("1. %10lld\n", 55uLL);
    printf("2. %10lld\n", 555uLL);
    printf("3. %-10lld\n", 55uLL);
    printf("4. %-10.10lld\n", 55uLL);
    printf("5. %10.10lld\n", 55uLL);
    printf("6. %15.10lld\n", 55uLL);
    printf("7. %11.10lld\n", 55uLL);
    printf("8. %-.19lld\n", 445566uLL);
    printf("9. %0.5lld\n", 11uLL);
    printf("10. %-.5lld\n", 11uLL);
    printf("11. %.5lld\n", 11uLL);
    printf("12. %-15.10lld\n", 55uLL);
    printf("13. %-15.10lld\n", 0uLL);
    printf("14. %#-15.10lld\n", 55uLL);

    printf("%f\n", 55.0);
    printf("%15f\n", 55.0);
    printf("%15f\n", 555.0);
    printf("%-15f\n", 55.0);
    printf("%-15.10f\n", 55.0);
    printf("%15.10f\n", 55.0);

    puts("-------");

    doprnt_printf_wrapper("0x%16.16p 0x%16p\n", val, val2);
    /* doprnt_printf_wrapper("0. %-9.9llx\n", 0x55uLL); */
    /* doprnt_printf_wrapper("1. %10llx\n", 0x55uLL); */
    /* doprnt_printf_wrapper("2. %10llx\n", 0x555uLL); */
    /* doprnt_printf_wrapper("3. %-10llx\n", 0x55uLL); */
    /* doprnt_printf_wrapper("4. %-10.10llx\n", 0x55uLL); */
    /* doprnt_printf_wrapper("5. %10.10llx\n", 0x55uLL); */
    /* doprnt_printf_wrapper("6. %15.10llx\n", 0x55uLL); */
    /* doprnt_printf_wrapper("7. %11.10llx\n", 0x55uLL); */
    /* doprnt_printf_wrapper("8. %-.19llx\n", 0x425566uLL); */
    /* doprnt_printf_wrapper("9. %0.5llx\n", 0x11uLL); */
    /* doprnt_printf_wrapper("10. %-.5llx\n", 0x11uLL); */
    /* doprnt_printf_wrapper("11. %.5llx\n", 0x11uLL); */
    /* doprnt_printf_wrapper("12. %-15.10llx\n", 0x55uLL); */
    /* doprnt_printf_wrapper("13. %-.19llx\n", 0uLL); */
    /* doprnt_printf_wrapper("14. %llx\n", 0uLL); */

    doprnt_printf_wrapper("1. %10lld\n", 55uLL);
    doprnt_printf_wrapper("2. %10lld\n", 555uLL);
    doprnt_printf_wrapper("3. %-10lld\n", 55uLL);
    doprnt_printf_wrapper("4. %-10.10lld\n", 55uLL);
    doprnt_printf_wrapper("5. %10.10lld\n", 55uLL);
    doprnt_printf_wrapper("6. %15.10lld\n", 55uLL);
    doprnt_printf_wrapper("7. %11.10lld\n", 55uLL);
    doprnt_printf_wrapper("8. %-.19lld\n", 445566uLL);
    doprnt_printf_wrapper("9. %0.5lld\n", 11uLL);
    doprnt_printf_wrapper("10. %-.5lld\n", 11uLL);
    doprnt_printf_wrapper("11. %.5lld\n", 11uLL);
    doprnt_printf_wrapper("12. %-15.10lld\n", 55uLL);
    doprnt_printf_wrapper("13. %-15.10lld\n", 0uLL);
    doprnt_printf_wrapper("14. %#llx\n", 0xc8180cuLL);
    doprnt_printf_wrapper("15. %#llx\n", 0x300000uLL);


    /* doprnt_printf_wrapper("1. %10llx\n", 0x55uLL); */
    /* doprnt_printf_wrapper("2. %10llx\n", 0x555uLL); */
    /* /1* doprnt_printf_wrapper("%-10llx\n", 0x5566778899aabbuLL); *1/ */
    /* doprnt_printf_wrapper("3. %-10llx\n", 0x55uLL); */
    /* doprnt_printf_wrapper("4. %-10.10llx\n", 0x55uLL); */
    /* doprnt_printf_wrapper("5. %10.10llx\n", 0x55uLL); */
    /* doprnt_printf_wrapper("6. %15.10llx\n", 0x55uLL); */

    doprnt_printf_wrapper("%f\n", 55.0);
    doprnt_printf_wrapper("%15f\n", 55.0);
    doprnt_printf_wrapper("%15f\n", 555.0);
    doprnt_printf_wrapper("%-15f\n", 55.0);
    doprnt_printf_wrapper("%-15.10f\n", 55.0);
    doprnt_printf_wrapper("%15.10f\n", 55.0);

    /* doprnt_printf_wrapper("%10llx\n", 0x55uLL); */
    /* doprnt_printf_wrapper("%10llx\n", 0x555uLL); */
    /* /1* doprnt_printf_wrapper("%-10llx\n", 0x5566778899aabbuLL); *1/ */
    /* doprnt_printf_wrapper("%-10llx\n", 0x55uLL); */
    /* doprnt_printf_wrapper("%-10.10llx\n", 0x55uLL); */

    /* doprnt_printf_wrapper("%f\n", 55.0); */
    /* doprnt_printf_wrapper("%10f\n", 55.0); */
    /* doprnt_printf_wrapper("%10f\n", 555.0); */
    /* doprnt_printf_wrapper("%-10f\n", 55.0); */
    /* doprnt_printf_wrapper("%-10.10f\n", 55.0); */
}

int main(int argc, char **argv){
    srand(time(NULL));
    if(!loginit()){
        printf("Could not init log\n");
        return 1;
    }

    /* logtest_readp_before_writep(); */
    /* logtest_writep_before_readp(); */

    /* logtest_read(); */
    /* logtest_newread(); */

    /* const char *s = "Hello my name is Justin"; */
    /* size_t sl = strlen(s); */
    /* size_t my_sl = my_strlen(s); */
    /* printf("%zu %zu\n", sl, my_sl); */

    /* vsnprintf_tests(); */
    doprnt_tests();

    return 0;
}
