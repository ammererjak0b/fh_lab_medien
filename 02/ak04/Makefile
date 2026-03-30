CC      ?= gcc
CFLAGS  += $(shell pkg-config --cflags ao) -Wall
LDFLAGS += $(shell pkg-config --libs ao) -lm

all: Release

Debug: CFLAGS += -g
Debug: AK04a.exe AK04b.exe

Release: AK04a.exe AK04b.exe

AK04a.exe: ao_example.c
	$(CC) $(CFLAGS) ao_example.c -o AK04a.exe $(LDFLAGS)

AK04b.exe: ak04b_melody.c
	$(CC) $(CFLAGS) ak04b_melody.c -o AK04b.exe $(LDFLAGS)

clean:
	rm -f AK04a.exe AK04b.exe
