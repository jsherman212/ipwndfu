#ifndef STRUCTS
#define STRUCTS

#include <stdint.h>

struct usb_request_packet {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute ((packed));

struct rstate {
    uint64_t x[29];
    uint64_t fp;
    uint64_t lr;
    uint64_t sp;
    uint32_t spsr;
    uint32_t pad0;
    uint64_t pc;
    uint64_t far;
    uint32_t esr;
};

#endif
