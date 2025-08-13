CC      := gcc
CFLAGS  := -O3 -ansi
LDFLAGS := -lSDL2

tcpaint: client.o canvas.o
	$(CC) $^ $(LDFLAGS) -o $@

canvas.o: canvas.c canvas.h
	$(CC) -c $(CFLAGS) $<

client.o: client.c canvas.h
	$(CC) -c $(CFLAGS) $<

.PHONY: clean
clean:
	rm -rf *.o tcpaint