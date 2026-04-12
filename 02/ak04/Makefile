CC      ?= gcc
CFLAGS  += $(shell pkg-config --cflags ao) -Wall
LDFLAGS += $(shell pkg-config --libs ao) -lm

all: Release

Debug: CFLAGS += -g
Debug: V02a.exe V02b.exe

Release: V02a.exe V02b.exe

V02a.exe: ao_example.c
	$(CC) $(CFLAGS) ao_example.c -o V02a.exe $(LDFLAGS)

V02b.exe: b.c
	$(CC) $(CFLAGS) b.c -o V02b.exe $(LDFLAGS)

clean:
	rm -f V02a.exe V02b.exe
