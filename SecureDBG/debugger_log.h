#ifndef DEBUGGER_LOG
#define DEBUGGER_LOG

#include <sys/cdefs.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

void logtest(void);
__printflike(1, 2) void dbglog(const char *, ...);
void vdbglog(const char *, va_list);
char *getlog(size_t *);
uint64_t loginit(void);

#endif
