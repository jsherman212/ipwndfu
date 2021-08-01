#ifndef PANIC
#define PANIC

#include <stdint.h>
__attribute__ ((noreturn)) void _panic(const char *, ...);
extern uint32_t esr;
extern uint64_t far;

#endif
