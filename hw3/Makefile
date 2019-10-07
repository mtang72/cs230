UTIL = ../utils/
CC = gcc
CFLAGS = -c -Wall -Werror -O3
FILES = pacver.o stopwatch.o packetsource.o generators.o fingerprint.o crc32.o locks.o paddedprim.o
LOCFILES = locks.o stopwatch.o lockmain.o paddedprim.o
.PHONY: all, clean

all: $(FILES) pacver

clean:
	-rm *.o 

a: $(LOCFILES)
	$(CC) -pthread -o locks $(LOCFILES)

pacver: $(FILES)
	$(CC) -pthread -o pacver $(FILES) -lm

locks.o: locks.c
	$(CC) -pthread $(CFLAGS) locks.c

lockmain.o: lockmain.c
	$(CC) -pthread $(CFLAGS) lockmain.c

stopwatch.o: $(UTIL)stopwatch.c
	$(CC) $(CFLAGS) $(UTIL)stopwatch.c

paddedprim.o: $(UTIL)paddedprim.c
	$(CC) $(CFLAGS) $(UTIL)paddedprim.c

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
