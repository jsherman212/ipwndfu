CC = clang
CFLAGS = -g -fsanitize=address

all : log trans

log : log.c
	$(CC) $(CFLAGS) log.c -o log

trans : trans.c
	$(CC) $(CFLAGS) trans.c -o trans
