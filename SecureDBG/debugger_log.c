#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/cdefs.h>

#include "SecureROM_offsets.h"

static char *logbuf = NULL;
static char *logbufcursor = NULL;
static size_t logsz = 2048;

/* Yeah, will overflow if message ends up being over 0x200 bytes. TODO fix */
__printflike(1, 2) void dbglog(const char *fmt, ...){
    va_list args;
    va_start(args, fmt);

    char msg[0x200];
    vsnprintf(msg, sizeof(msg), fmt, args);

    va_end(args);
}

char *getlogbuf(void){
    return logbuf;
}

bool loginit(void){
    if(logbuf)
        return true;

    logbuf = alloc(logsz, 0x40, NULL);

    if(!logbuf)
        return false;

    memset(logbuf, 0, logsz);

    return true;
}
