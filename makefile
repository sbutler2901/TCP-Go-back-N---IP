CC=gcc
CFLAGS= -Wall -Wextra -Wshadow -O

client: client.c
	$(CC) $(CFLAGS) -o client client.c 

server: server.c
	$(CC) $(CFLAGS) -o server server.c 
