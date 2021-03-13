#ifndef STRUCTS
#define STRUCTS

struct usb_request_packet {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute ((packed));

#endif
