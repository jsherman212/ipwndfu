#ifndef DOPRNT
#define DOPRNT

#include <stddef.h>

struct doprnt_info {
    char *buf;
    size_t remaining;
    int written;
};

int doprnt(const char *, void (*)(char, struct doprnt_info *),
        struct doprnt_info *, va_list);

#endif
