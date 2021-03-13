#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/cdefs.h>

#include "SecureROM_offsets.h"

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

static size_t strlen(char *s){
    char *p = s;

    while(*p)
        p++;

    return p - s;
}

__printflike(1, 2) void dbglog(const char *fmt, ...){
    if(loglen == logsz)
        return;

    va_list args;
    va_start(args, fmt);

    /* We do not copy nul terminators into the log. msgbuf has been
     * given logsz+1 bytes of memory so messages that are exactly the
     * log size are not truncated. */
    vsnprintf(msgbuf, logsz + 1, fmt, args);

    va_end(args);

    char *msgp = msgbuf;
    size_t msglen = strlen(msgp);

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
            memmove(writep, msgp, msglen);
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
        memmove(writep, msgp, canwrite);

        msgp += canwrite;
        writep += canwrite;
        loglen += canwrite;

        /* Check if we're able to copy the rest if we wrap around */
        if(pending > wrapspace)
            return;

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
        *lenp = outsz;
    }
    else{
        /* First, get the part covered by [readp, logend) */
        size_t firstsz = logend - readp;
        memmove(retbuf, readp, firstsz);

        /* Second, get the part covered by [logbuf, writep) */
        size_t secondsz = writep - logbuf;
        memmove(retbuf + firstsz, logbuf, secondsz);
        *lenp = firstsz + secondsz;
    }

    loglen = 0;
    readp = writep;

    return retbuf;
}

static bool log_inited = false;

uint64_t loginit(void){
    if(log_inited)
        return 0;

    logbuf = alloc2(logsz);

    if(!logbuf)
        return 1;

    msgbuf = alloc2(logsz + 1);

    if(!msgbuf)
        return 2;

    retbuf = alloc2(logsz);

    if(!retbuf)
        return 3;

    logend = logbuf + logsz;
    readp = writep = logbuf;

    log_inited = true;

    return 0;
}
