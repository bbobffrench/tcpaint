CC      := gcc
CFLAGS  := -O3 -ansi -Wall -pedantic
LDFLAGS := -lSDL2

tcpaint: client.o canvas.o
	$(CC) $^ $(LDFLAGS) -o $@

canvas.o: canvas.c canvas.h
	$(CC) -c $(CFLAGS) $<

client.o: client.c client.h canvas.h
	$(CC) -c $(CFLAGS) $<

.PHONY: clean
clean:
	rm -rf *.o tcpaint