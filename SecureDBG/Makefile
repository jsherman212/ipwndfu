SDK = $(shell xcrun --sdk iphoneos --show-sdk-path)
CC = $(shell xcrun --sdk $(SDK) --find clang)
CFLAGS = -isysroot $(SDK) -arch arm64 -ffreestanding -e _debugger_entryp
CFLAGS += -fno-stack-protector -DFORTIFY_SOURCE=0 -nostdlib -static -Oz

LDFLAGS += -Wl,-order_file,sym_order

all : debugger

debugger_log.o : debugger_log.c debugger_log.h
	$(CC) $(CFLAGS) debugger_log.c -c

debugger : debugger_log.o debugger.c
	$(CC) $(CFLAGS) $(LDFLAGS) debugger_log.o debugger.c -o debugger
	gobjcopy -O binary -j .text -j .data debugger debugger.bin
