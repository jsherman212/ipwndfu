#ifndef SECUREROM_OFFSETS
#define SECUREROM_OFFSETS

/* T8015 */
#define usb_interface_request_handler 0x180008638
/* Skip jump_back trampoline */
#define ipwndfu_usb_interface_request_handler ((int (*)(struct usb_request_packet *, void **))0x18001bc18)
#define alloc ((void *(*)(size_t, size_t))0x10000d508)
/* This alloc2 is most likely malloc */
#define alloc2 ((void *(*)(size_t))0x10000d0d4)
#define memset ((void *(*)(void *, int, size_t))0x10000ec00)
#define memmove ((void *(*)(void *, void *, size_t))0x10000e9d0)
#define usb_core_do_io ((void (*)(int, void *, size_t, void *))0x10000b9a8)
#define vsnprintf ((int (*)(char *, size_t, const char *, va_list))0x10000e6a8)


#endif
