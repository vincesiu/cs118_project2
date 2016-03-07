

CC=gcc
CFLAGS=-c -Wall

all: client server

client: client.o
	$(CC) client.o -o client

client.o: client.c
	$(CC) $(CFLAGS) client.c

server: server.o
	$(CC) server.o -o server

server.o: server.c
	$(CC) $(CFLAGS) server.c
  
clean:
	rm *o client server

.PHONY: clean
