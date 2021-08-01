#ifndef SPINLOCK
#define SPINLOCK

#include <stdint.h>

/* A simple spinlock. Safe in any context */
struct splck {
    uint32_t val;
};

#define SPLCK_UNLOCKED      (0)
#define SPLCK_LOCKED        (1)

#define SPLCK_INITIALIZER { .val = SPLCK_UNLOCKED }

typedef struct splck splck_t;

void splck_init(splck_t *);
void splck_lck(splck_t *);
void splck_done(splck_t *);

#endif
