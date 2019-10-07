UTIL = ../utils/
CC = gcc
CFLAGS = -g -c -Wall -Werror -O3
FILES = pachand.o packhash.o packetsource.o generators.o fingerprint.o crc32.o locks.o paddedprim.o hashgenerator.o hashtable.o seriallist.o
.PHONY: all, clean

all: $(FILES) pachand

clean:
	-rm *.o 

pachand: $(FILES)
	$(CC) -pthread -O3 -o pachand $(FILES) -lm

locks.o: $(UTIL)locks.c
	$(CC) -pthread $(CFLAGS) $(UTIL)locks.c

lockmain.o: lockmain.c
	$(CC) -pthread $(CFLAGS) lockmain.c

paddedprim.o: $(UTIL)paddedprim.c
	$(CC) $(CFLAGS) $(UTIL)paddedprim.c

packetsource.o: $(UTIL)packetsource.c
	$(CC) $(CFLAGS) $(UTIL)packetsource.c

hashgenerator.o: $(UTIL)hashgenerator.c
	$(CC) $(CFLAGS) $(UTIL)hashgenerator.c

hashtable.o: $(UTIL)hashtable.c
	$(CC) $(CFLAGS) $(UTIL)hashtable.c

seriallist.o: $(UTIL)seriallist.c
	$(CC) $(CFLAGS) $(UTIL)seriallist.c

generators.o: $(UTIL)generators.c
	$(CC) $(CFLAGS) $(UTIL)generators.c 

fingerprint.o: $(UTIL)fingerprint.c
	$(CC) $(CFLAGS) $(UTIL)fingerprint.c

pachand.o: pachand.c
	$(CC) $(CFLAGS) -pthread pachand.c

crc32.o: $(UTIL)crc32.c
	$(CC) $(CFLAGS) $(UTIL)crc32.c

packhash.o: packhash.c
	$(CC) $(CFLAGS) packhash.c
