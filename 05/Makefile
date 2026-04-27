CC      = gcc
CFLAGS  = -Wall -Wextra
LDFLAGS = -lm

TARGETS = vk03a.exe vk03b.exe vk03c.exe

all: $(TARGETS)

vk03a.exe: vk03a.c
	$(CC) $(CFLAGS) -o $@ $<

vk03b.exe: vk03b.c
	$(CC) $(CFLAGS) -o $@ $<

vk03c.exe: vk03c.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)
