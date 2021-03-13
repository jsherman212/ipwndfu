SDK = $(shell xcrun --sdk iphoneos --show-sdk-path)
CC = $(shell xcrun --sdk $(SDK) --find clang)
CFLAGS = -isysroot $(SDK) -arch arm64 -ffreestanding
CFLAGS += -fno-stack-protector -DFORTIFY_SOURCE=0 -Oz -static -nostdlib
FINAL_CFLAGS = -e _debugger_entryp

# On T8015 I used to just stick this code at 0x102018340, but that would
# really screw up a lot of PC-relative addressing since the code was
# generated with the first segment's address at 0x100000000. So instead,
# we place this code around 0x102018340 because I cannot get clang to
# produce an output file without the Mach-O header. So I get
# _debugger_entryp as close to the Mach-O header as possible. This is
# really, really irritating as we end up wasting a good amount of space
# but I can't figure out any other way to deal with this
LDFLAGS = -Wl,-order_file,sym_order -Wl,-image_base,0x2018000
LDFLAGS += -Wl,-segalign,0x1

all : debugger

debugger_log.o : debugger_log.c debugger_log.h
	$(CC) $(CFLAGS) debugger_log.c -c

debugger : debugger_log.o debugger.c
	$(CC) $(CFLAGS) $(FINAL_CFLAGS) $(LDFLAGS) debugger_log.o debugger.c -o debugger
	gobjcopy -O binary -j .text -j .data -j .cstring -j .bss debugger debugger.bin
