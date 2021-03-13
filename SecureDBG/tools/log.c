#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>
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

static size_t logsz = 0x800;
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

    /* char msg[0x200]; */
    /* vsnprintf(msg, sizeof(msg), fmt, args); */

    /* We do not copy nul terminators into the log. msgbuf has been
     * given logsz+1 bytes of memory so messages that are exactly the
     * log size are not truncated. */
    vsnprintf(msgbuf, logsz + 1, fmt, args);

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

/* We never do partial reads here. The returned buffer isn't nul terminated,
 * the size is denoted by lenp. */
char *getlog(size_t *lenp){
    /* Nothing to read */
    if(loglen == 0){
        *lenp = 0;
        return retbuf;
    }

    /* Copy everything between readp and writep. Unfortunately, this range
     * isn't guarenteed to be contiguous, so I'm using another buffer
     * to abstract that away from the caller. If this range is contiguous,
     * then writep will be more than readp. If it isn't, then writep will
     * be less than readp. */
    if(writep > readp){
        size_t outsz = writep - readp;
        memmove(retbuf, readp, outsz);
        /* printf("%s: copied [%p, %p)\n", __func__, readp, logbuf + outsz); */
        *lenp = outsz;
    }
    else{
        /* First, get the part covered by [readp, logend) */
        size_t firstsz = logend - readp;
        memmove(retbuf, readp, firstsz);
        /* printf("%s: copied [%p, %p)\n", __func__, readp, logend); */
        /* Second, get the part covered by [logbuf, writep) */
        size_t secondsz = writep - logbuf;
        memmove(retbuf + firstsz, logbuf, secondsz);
        /* printf("%s: copied [%p, %p)\n", __func__, logbuf, writep); */
        *lenp = firstsz + secondsz;
    }

    loglen = 0;
    readp = writep;

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

    retbuf = malloc(logsz);

    if(!retbuf){
        printf("%s: malloc failed\n", __func__);
        return false;
    }

    memset(logbuf, 0, logsz);
    memset(msgbuf, 0, logsz + 1);
    memset(retbuf, 0, logsz);

    logend = logbuf + logsz;
    readp = writep = logbuf;

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

int main(int argc, char **argv){
    if(!loginit()){
        printf("Could not init log\n");
        return 1;
    }

    /* logtest_readp_before_writep(); */
    /* logtest_writep_before_readp(); */

    logtest_read();

    return 0;
}
