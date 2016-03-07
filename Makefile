

CC=gcc
CFLAGS=-c -Wall

all: receiver sender

receiver: receiver.o
	$(CC) receiver.o -o receiver

receiver.o: receiver.c
	$(CC) $(CFLAGS) receiver.c

sender: sender.o
	$(CC) sender.o -o sender

sender.o: sender.c
	$(CC) $(CFLAGS) sender.c
  
clean:
	rm *o receiver sender

.PHONY: clean
