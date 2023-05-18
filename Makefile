all: server client client2

server: server.c
	gcc server.c -o server -lrt -lpthread

client: client.c
	gcc client.c -o client

client2: client2.c
	gcc client2.c -o client2