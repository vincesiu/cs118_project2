

CC=gcc
CFLAGS=-g -c -Wall
LDFLAGS=-g -Wall

all: receiver sender

receiver: receiver.o
	$(CC) $(LDFLAGS) receiver.o -o receiver

receiver.o: receiver.c structs.h
	$(CC) $(CFLAGS) receiver.c

sender: sender.o
	$(CC) $(LDFLAGS) sender.o -o sender

sender.o: sender.c structs.h
	$(CC) $(CFLAGS) sender.c
  
clean:
	rm *o receiver sender

.PHONY: clean
