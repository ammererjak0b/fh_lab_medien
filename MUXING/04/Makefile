CC = gcc

CFLAGS = -Wall -I../libs/include
LDFLAGS = -L../libs/lib -Wl,-Bstatic -lx264 -Wl,-Bdynamic -ldl -lm -lpthread

all: MTM04a.exe

MTM04a.exe: MTM04a.o
	$(CC) MTM04a.o -o MTM04a.exe $(LDFLAGS)

MTM04a.o: MTM04a.c
	$(CC) $(CFLAGS) -c MTM04a.c -o MTM04a.o

clean:
	rm -f MTM04a.o MTM04a.exe
