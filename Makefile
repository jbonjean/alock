CC=cc
CFLAGS=-Wall -O2

alock: alock.c
	$(CC) $(CFLAGS) $^ -o $@

all: alock

clean:
	rm -f alock
