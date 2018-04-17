CC=gcc
CFLAGS= -Wall -Wextra -Wshadow -std=gnu11

client: client.c
	$(CC) $(CFLAGS) -o client client.c 

server: server.c
	$(CC) $(CFLAGS) -o server server.c 

c:
	./client localhost 12345 cFile 64 500

s:
	./server 12345 sFile 0.05
