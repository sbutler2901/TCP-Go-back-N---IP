CC=gcc

client: client.c
	$(CC) -o client client.c

server: server.c
	$(CC) -o server server.c

