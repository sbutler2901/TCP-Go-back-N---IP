CC=gcc
CFLAGS= -Wall -Wextra -Wshadow -std=gnu11

client: client.c
	$(CC) $(CFLAGS) -o client client.c 

server: server.c
	$(CC) $(CFLAGS) -o server server.c 

c:
	./client localhost 9999 cFile 4 500

s:
	./server 9999 sFile 0.5
