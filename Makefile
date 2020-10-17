.POSIX:
all:
	$(CC) -o among-sus $(CFLAGS) main.c

clean:
	rm -f among-sus
