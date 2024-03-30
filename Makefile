CC=gcc
CFLAGS=-w

all: client server

client: client.c
	$(CC) $(CFLAGS) client.c -o client

server: server.c
	$(CC) $(CFLAGS) server.c -o server



package:
	tar -czvf irc-package.tar.gz client server client.conf Makefile README.md

clean:
	rm -f client server