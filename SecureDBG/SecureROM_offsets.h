#ifndef SECUREROM_OFFSETS
#define SECUREROM_OFFSETS

#include <stdbool.h>
#include <stdint.h>

/* T8015 */
/* #define panic 0x100008d28 */
#define usb_interface_request_handler 0x180008638
#define io_buffer (*(void **)0x1800085d8)
/* Skip jump_back trampoline */
#define ipwndfu_usb_interface_request_handler ((int (*)(struct usb_request_packet *, void *))0x18001bc18)
#define malloc ((void *(*)(size_t))0x10000d0d4)
#define free ((void (*)(void *))0x10000d2e4)
#define memset ((void *(*)(void *, int, size_t))0x10000ec00)
#define memmove ((void *(*)(void *, void *, size_t))0x10000e9d0)
#define reboot ((void (*)(void))0x100007908)
#define sleep ((void (*)(uint32_t))0x1000097c0)
#define usb_task_entrypoint ((void (*)(void *))0x100004d84)
#define usb_core_do_io ((void (*)(int, void *, size_t, void *))0x10000b9a8)
#define usb_task_init ((void (*)(void))0x100004d14)
#define task_switch ((void (*)(void))0x100009484)
#define vsnprintf ((int (*)(char *, size_t, const char *, va_list))0x10000e6a8)
#define panic ((void (*)(const char *, const char *, ...))0x100008d28)

#define platform_get_boot_device ((bool (*)(int, int *, int *, int *))0x10000796c)
#define platform_enable_boot_interface ((void (*)(int, int, int))0x100007a10)
#define lookup_image_in_bdev ((void *(*)(const char *))0x100001A4C)
#define flash_nand_init ((uint64_t (*)(int))0x100003a84)

#define clock_gate ((void (*)(int, bool))0x1000032C4)
#define identity_map_ro ((void (*)(uint64_t, uint64_t))0x100001628)
#define identity_map_rw ((void (*)(uint64_t, uint64_t))0x100001600)
#define identity_map_rx ((void (*)(uint64_t, uint64_t))0x100001614)

#endif
