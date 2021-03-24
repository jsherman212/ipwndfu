#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/cdefs.h>

#include "common.h"
#include "SecureROM_offsets.h"

/* Give a page for logbuf, msgbuf, and retbuf */
asm(".section __TEXT,__logs\n"
    ".align 14\n"
    ".space 0xc000, 0x0\n"
    ".section __TEXT,__text\n");

static GLOBAL(char *logbuf) = NULL;
static GLOBAL(char *logend) = NULL;
static GLOBAL(char *readp) = NULL;
static GLOBAL(char *writep) = NULL;

static GLOBAL(size_t logsz) = 0x4000;
/* We are limited to the max USB transfer */
static GLOBAL(size_t retbufsz) = 0x800;
static GLOBAL(size_t loglen) = 0;

static GLOBAL(char *msgbuf) = NULL;

/* We re-use the same buffer for returning log contents to the caller.
 * This is malloc'ed because usb_core_do_io does not like AOP SRAM
 * pointers for whatever reason */
static GLOBAL(char *retbuf) = NULL;

__printflike(1, 2) void dbglog(const char *fmt, ...){
    if(loglen == logsz)
        return;

    va_list args;
    va_start(args, fmt);

    /* Logs of size 0x4000 are gonna have their last char truncated,
     * but when would I ever log a single string of that size? */
    aop_sram_vsnprintf(msgbuf, logsz, fmt, args);

    va_end(args);

    char *msgp = msgbuf;
    size_t msglen = aop_sram_strlen(msgp);

    if(msglen == 0)
        return;
    
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
            aop_sram_memcpy(writep, msgp, msglen);
            writep += msglen;
            loglen += msglen;
            return;
        }

        /* How many bytes can we write before we hit the end of the buffer? */
        size_t canwrite = logend - writep;

        /* How many bytes are waiting to be written after we wrap around? */
        size_t pending = msglen - canwrite;

        /* How much space do we have when we wrap around? */
        size_t wrapspace = readp - logbuf;

        /* Copy what we can to the end of the log buffer */
        aop_sram_memcpy(writep, msgp, canwrite);

        msgp += canwrite;
        writep += canwrite;
        loglen += canwrite;

        /* Check if we're able to copy the rest if we wrap around */
        if(pending > wrapspace)
            return;

        /* Wrap around to the beginning */
        aop_sram_memcpy(logbuf, msgp, pending);

        writep = logbuf + pending;
        loglen += pending;
    }
    else{
        /* We have wrapped around to the beginning of the log
         * buffer. We don't want to overflow into anything that hasn't
         * been read out yet, so keep that in mind when calculating the
         * length of the message we can copy. */
        size_t canwrite = readp - writep;

        /* Make sure we don't write more than we have in the message */
        if(canwrite > msglen)
            canwrite = msglen;

        aop_sram_memcpy(writep, msgp, canwrite);

        writep += canwrite;
        loglen += canwrite;
    }
}

/* Read out the log in 0x800-byte increments. This function is meant to be
 * called from a loop on the host machine until the log is emptied. Weird
 * things may happen to log contents if someone writes to the log while
 * it's getting read out.
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

        aop_sram_memcpy(retbuf, readp, chunk);

        readp += chunk;
        len = chunk;
    }
    else{
        /* First, get the part covered by [readp, logend) */
        size_t firstchunk = logend - readp;
        bool get_secondchunk = true;

        if(firstchunk >= retbufsz){
            firstchunk = retbufsz;
            get_secondchunk = false;
        }

        aop_sram_memcpy(retbuf, readp, firstchunk);

        readp += firstchunk;
        len = firstchunk;

        /* If we can, get the second part covered by [logbuf, writep) */
        if(get_secondchunk){
            /* Figure out how much we can copy from the second part */
            size_t limit = retbufsz - firstchunk;
            size_t secondchunk = writep - logbuf;

            if(secondchunk >= limit)
                secondchunk = limit;

            aop_sram_memcpy(retbuf + firstchunk, logbuf, secondchunk);

            readp = logbuf + secondchunk;
            len += secondchunk;
        }
    }

    loglen -= len;
    *lenp = len;

    return retbuf;
}

static GLOBAL(bool log_inited) = false;

void loginit(void){
    if(log_inited)
        return;

    extern volatile uint64_t __logs_start[] asm("section$start$__TEXT$__logs");

    logbuf = (char *)__logs_start;
    logend = logbuf + logsz;
    msgbuf = logend;
    retbuf = msgbuf + logsz;

    readp = writep = logbuf;

    log_inited = true;

    return;
}
