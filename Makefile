all: server first_gardener second_gardener

server: server.c common.c
	gcc server.c common.c -o server -lrt -lpthread

first_gardener: first_gardener.c common.c
	gcc first_gardener.c common.c -o first_gardener

second_gardener: second_gardener.c common.c
	gcc second_gardener.c common.c -o second_gardener