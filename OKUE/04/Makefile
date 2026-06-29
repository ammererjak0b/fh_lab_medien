CC     = gcc
CFLAGS = -Wall -Wextra -I./libs/include
LDFLAGS = -L./libs/lib -Wl,-Bstatic -lx264 -Wl,-Bdynamic -ldl -lm -lpthread

all: OKUEa.exe OKUEb.exe OKUEc.exe

OKUEa.exe: OKUEa.c
	$(CC) $(CFLAGS) OKUEa.c -o OKUEa.exe $(LDFLAGS)

OKUEb.exe: OKUEb.c
	$(CC) $(CFLAGS) OKUEb.c -o OKUEb.exe $(LDFLAGS)

OKUEc.exe: OKUEc.c
	$(CC) $(CFLAGS) OKUEc.c -o OKUEc.exe $(LDFLAGS)

clean:
	rm -f OKUEa.exe OKUEb.exe OKUEc.exe
