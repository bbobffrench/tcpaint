CC      := gcc
CFLAGS  := -O3 -Wall -pedantic
LDFLAGS := -lSDL2 -lm

.PHONY: all clean

all: tcpaint tcpaintd

tcpaint: tcpaint.o client.o canvas.o
	$(CC) $^ $(LDFLAGS) -o $@

tcpaintd: tcpaintd.o client.o canvas.o
	$(CC) $^ $(LDFLAGS) -o $@

canvas.o: canvas.c canvas.h
	$(CC) -c $(CFLAGS) $<

client.o: client.c client.h canvas.h
	$(CC) -c $(CFLAGS) $<

tcpaint.o: tcpaint.c client.h canvas.h

tcpaintd.o: tcpaintd.c client.h canvas.h
	$(CC) -c $(CFLAGS) $<

clean:
	rm -rf *.o tcpaint tcpaintd