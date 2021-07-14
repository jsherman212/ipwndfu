SDK = $(shell xcrun --sdk iphoneos --show-sdk-path)
CC = $(shell xcrun --sdk $(SDK) --find clang)
CFLAGS = -isysroot $(SDK) -arch arm64 -ffreestanding
# DO NOT TURN ON ANY OPTIMIZATION
CFLAGS += -fno-stack-protector -DFORTIFY_SOURCE=0 -O0 -nostdlib
FINAL_CFLAGS = -e _earlypongo_bootstrap

# T8015 AOP SRAM base is at 0x234e00000
LDFLAGS = -Wl,-order_file,sym_order -Wl,-image_base,0x234e00000 -Wl,-preload

all : earlypongo

common.o : common.c common.h
	$(CC) $(CFLAGS) common.c -c

doprnt.o : doprnt.c doprnt.h
	$(CC) $(CFLAGS) doprnt.c -c

debugger_log.o : debugger_log.c debugger_log.h
	$(CC) $(CFLAGS) debugger_log.c -c

panic.o : panic.c
	$(CC) $(CFLAGS) panic.c -c

earlypongo : asm.s common.o doprnt.o debugger_log.o bootstrap.c panic.o
	$(CC) $(CFLAGS) $(FINAL_CFLAGS) $(LDFLAGS) asm.s common.o debugger_log.o doprnt.o bootstrap.c panic.o -o earlypongo
	# This does not work for some dd's TODO
	dd if=earlypongo of=earlypongo.bin bs=1 skip=0x8000
