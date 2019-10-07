UTIL = ../utils/
CC = gcc
CFLAGS = -c -Wall -Werror
FILES = pacver.o stopwatch.o packetsource.o generators.o fingerprint.o crc32.o

.PHONY: all, clean

all: $(FILES) pacver

clean:
	-rm *.o 

pacver: $(FILES)
	$(CC) -pthread -o pacver $(FILES) -lm

stopwatch.o: $(UTIL)stopwatch.c
	$(CC) $(CFLAGS) $(UTIL)stopwatch.c

packetsource.o: $(UTIL)packetsource.c
	$(CC) $(CFLAGS) $(UTIL)packetsource.c

generators.o: $(UTIL)generators.c
	$(CC) $(CFLAGS) $(UTIL)generators.c 

fingerprint.o: $(UTIL)fingerprint.c
	$(CC) $(CFLAGS) $(UTIL)fingerprint.c

pacver.o: pacver.c
	$(CC) $(CFLAGS) -pthread pacver.c

crc32.o: $(UTIL)crc32.c
	$(CC) $(CFLAGS) $(UTIL)crc32.c
