CC = gcc
CFLAGS =
all: server client

server:
	$(CC) -o Server server.c -lpthread -lm $(CFLAGS)

client:
	$(CC) -o Client client.c $(CFLAGS)

clean:
	rm -f Client
	rm -f Server

.PHONY: all server client clean 