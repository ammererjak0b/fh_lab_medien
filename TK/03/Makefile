CC = gcc
PKG_CONFIG_PATH = ../libs/lib/pkgconfig
LIBS = libavformat libavcodec libavutil

CFLAGS  = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(LIBS))
LDFLAGS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs --static $(LIBS))

all: TK03a.exe TK03b.exe TK03c.exe

TK03a.exe: TK03a.c
	$(CC) $(CFLAGS) TK03a.c $(LDFLAGS) -o TK03a.exe

TK03b.exe: TK03b.c
	$(CC) $(CFLAGS) TK03b.c $(LDFLAGS) -o TK03b.exe

TK03c.exe: TK03c.c
	$(CC) $(CFLAGS) TK03c.c $(LDFLAGS) -o TK03c.exe

clean:    
	rm -f TK03a.exe TK03b.exe TK03c.exe
