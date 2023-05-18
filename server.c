#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAXPENDING 5 

int createServerSocket(in_addr_t sin_addr, int port) {
    int server_socket;
    struct sockaddr_in server_address;

    if ((server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("Unable to create server socket");
        exit(-1);
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        perror("Unable to bind address");
        exit(-1);
    }

    if (listen(server_socket, MAXPENDING) < 0) {
        perror("Unable to listen server socket");
        exit(-1);
    }

    return server_socket;
}

int acceptClientConnection(int server_socket) {
    int client_socket;
    struct sockaddr_in client_address;
    unsigned int address_length;

    address_length = sizeof(client_address);

    if ((client_socket = accept(server_socket, (struct sockaddr *) &client_address, &address_length)) < 0) {
        perror("Unable to accept client connection");
        exit(-1);
    }

    printf("Handling client %s:%d\n", inet_ntoa(client_address.sin_addr), client_address.sin_port);

    return client_socket;
}

void handle(int client_socket) {
    char echoBuffer[1024];
    int recvMsgSize;

    if ((recvMsgSize = recv(client_socket, echoBuffer, 1024, 0)) < 0) {
        perror("recv() bad");
        exit(-1);
    }

    while (recvMsgSize > 0)
    {
        if (send(client_socket, echoBuffer, recvMsgSize, 0) != recvMsgSize) {
            perror("send() bad");
            exit(-1);
        }

        if ((recvMsgSize = recv(client_socket, echoBuffer, 1024, 0)) < 0) {
            perror("recv() bad");
            exit(-1);
        }
    }

    close(client_socket); 
}

int server_socket;
int children_counter = 0;

void waitChildProcessess() {
    while (children_counter > 0)
    {
        int child_id = waitpid((pid_t) -1, NULL, 0);
        if (child_id < 0) {
            perror("Unable to wait child proccess");
            exit(-1);
        } else {
            children_counter--;
        }
    }
}

void sigint_handler(int signum) {
    printf("Server stopped\n");
    waitChildProcessess();
    close(server_socket);
    exit(0);
}

int main(int argc, char *argv[]) {

    if (argc != 3) {
        fprintf(stderr, "Usage:  %s <Server Address> <Server Port>\n", argv[0]);
        exit(1);
    }

    in_addr_t server_address;
    if ((server_address = inet_addr(argv[1])) < 0) {
        perror("Invalid server address");
        exit(-1);
    }

    int server_port = atoi(argv[2]);
    if (server_port < 0) {
        perror("Invalid server port");
        exit(-1);
    }

    server_socket = createServerSocket(server_address, server_port);
    signal(SIGINT, sigint_handler);

    while (1)
    {
        int client_socket = acceptClientConnection(server_socket);

        pid_t child_id;
        if ((child_id = fork()) < 0) {
            perror("Unable to create child for handling new connection");
            exit(-1);
        }
        else if (child_id == 0)
        {
            signal(SIGINT, SIG_DFL);
            close(server_socket);
            handle(client_socket);
            exit(0);
        }

        printf("with child process: %d\n", (int) child_id);
        close(client_socket);
        children_counter++;
    }
    
    return 0;
}
