#ifndef DEBUGGER_LOG
#define DEBUGGER_LOG

#include <sys/cdefs.h>

__printflike(1, 2) void dbglog(const char *, ...);
char *getlog(size_t *);
uint64_t loginit(void);

#endif
