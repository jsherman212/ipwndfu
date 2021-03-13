#ifndef DEBUGGER_LOG
#define DEBUGGER_LOG

__printflike(1, 2) void dbglog(const char *, ...);
char *getlogbuf(void);
bool loginit(void);

#endif
