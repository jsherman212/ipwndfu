#ifndef SECUREROM_OFFSETS
#define SECUREROM_OFFSETS

/* T8015 */
#define usb_interface_request_handler 0x180008638
#define ipwndfu_usb_interface_request_handler ((int (*)(struct usb_request_packet *, void **))0x18001bc00)
#define alloc ((void *(*)(size_t, uint64_t, void *))0x10000d508)
#define memset ((void *(*)(void *, int, size_t))0x10000ec00)
#define vsnprintf ((int (*)(char *, size_t, const char *, va_list))0x10000e6a8);

#endif
