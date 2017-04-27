CC=gcc
CFLAGS= -Wall -Wextra -Wshadow -std=gnu11

client: client.c
	$(CC) $(CFLAGS) -o client client.c 

server: server.c
	$(CC) $(CFLAGS) -o server server.c 

c:
	./client 152.1.0.171 12346 cFile 4 500

s:
	./server 99999 sFile 0.01
