CC = gcc
CFLAGS = -Wall -Wextra    # enable all common warnings
LDFLAGS = -lm

TARGETS = VK03a.exe VK03b.exe VK03c.exe

all: $(TARGETS)

VK03a.exe: VK03a.c
	$(CC) $(CFLAGS) -o $@ $<
VK03b.exe: VK03b.c
	$(CC) $(CFLAGS) -o $@ $<
VK03c.exe: VK03c.c
	$(CC) $(CFLAGS) -o $@ $<
clean:
	rm -f $(TARGETS)
