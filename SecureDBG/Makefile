SDK = $(shell xcrun --sdk iphoneos --show-sdk-path)
CC = $(shell xcrun --sdk $(SDK) --find clang)
CFLAGS = -isysroot $(SDK) -arch arm64 -ffreestanding -e _debugger_entryp
CFLAGS += -fno-stack-protector -DFORTIFY_SOURCE=0 -nostdlib -static -Oz

debugger : debugger.c
	$(CC) $(CFLAGS) $(LDFLAGS) debugger.c -o debugger
	gobjcopy -O binary -j .text -j .data debugger debugger.bin
