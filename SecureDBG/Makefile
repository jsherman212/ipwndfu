SDK = $(shell xcrun --sdk iphoneos --show-sdk-path)
CC = $(shell xcrun --sdk $(SDK) --find clang)
CFLAGS = -isysroot $(SDK) -arch arm64 -ffreestanding
# DO NOT TURN ON ANY OPTIMIZATION
CFLAGS += -fno-stack-protector -DFORTIFY_SOURCE=0 -O0 -nostdlib
# CFLAGS += -mno-unaligned-access
FINAL_CFLAGS = -e _debugger_entryp

# T8015 AOP SRAM base is at 0x234e00000, but we use a TTE that creates
# an rwx view at VA 0x934e00000.
# LDFLAGS = -Wl,-order_file,sym_order -Wl,-image_base,0x934e00000 -Wl,-preload
LDFLAGS = -Wl,-order_file,sym_order -Wl,-image_base,0x234e00000 -Wl,-preload

all : debugger

common.o : common.c common.h
	$(CC) $(CFLAGS) common.c -c

doprnt.o : doprnt.c doprnt.h
	$(CC) $(CFLAGS) doprnt.c -c

dram.o : dram.c dram.h
	$(CC) $(CFLAGS) dram.c -c

debugger_log.o : debugger_log.c debugger_log.h
	$(CC) $(CFLAGS) debugger_log.c -c

init.o : init.c
	$(CC) $(CFLAGS) init.c -c

panic.o : panic.c
	$(CC) $(CFLAGS) panic.c -c

spinlock.o : spinlock.c
	$(CC) $(CFLAGS) spinlock.c -c

debugger : common.o cpu5.s doprnt.o debugger_log.o debugger.c dram.o init.o panic.o spinlock.o sram.s
	$(CC) $(CFLAGS) $(FINAL_CFLAGS) $(LDFLAGS) common.o cpu5.s debugger_log.o doprnt.o debugger.c dram.o init.o panic.o spinlock.o sram.s -o debugger
	dd if=debugger of=debugger.bin bs=1 skip=0x8000
