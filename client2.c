#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "common.h"

void handleGardenPlot(int client_socket, struct GardenerTask task) {
    char buffer[256];
    int received;
    do {
        // (char *)(&task) -- сериализация, передаем структуру по байтам
        if (send(client_socket, (char *)(&task), sizeof(task), 0) != sizeof(task)) {
            perror("send() bad");
            exit(-1);
        }

        if ((received = recv(client_socket, buffer, sizeof(buffer), 0)) <= 0) {
            perror("recv() bad");
            exit(-1);
        }
    } while (buffer[0] != 'S');
}

int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_in echoServAddr;
    unsigned short echoServPort;
    char *servIP;
    char *echoString;
    char echoBuffer[1024]; 
    unsigned int echoStringLen;
    int bytesRcvd, totalBytesRcvd;

    if (argc != 3)
    {
       fprintf(stderr, "Usage: %s <Server address> <Server port>\n",
               argv[0]);
       exit(1);
    }

    servIP = argv[1];             /* First arg: server IP address (dotted quad) */

    echoServPort = atoi(argv[2]);

    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("Unable to create client socket");
        exit(-1);
    }

    /* Construct the server address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr));     /* Zero out structure */
    echoServAddr.sin_family      = AF_INET;             /* Internet address family */
    echoServAddr.sin_addr.s_addr = inet_addr(servIP);   /* Server IP address */
    echoServAddr.sin_port        = htons(echoServPort); /* Server port */

    /* Establish the connection to the echo server */
    if (connect(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0) {
        perror("Unable to connect to server");
        exit(-1);
    }

    struct FieldSize field_size;
    if ((bytesRcvd = recv(sock, echoBuffer, sizeof(field_size), 0)) <= 0) {
        perror("recv() bad");
        exit(-1);
    }

    field_size = *((struct FieldSize *)echoBuffer);
    int rows = field_size.rows;
    int columns = field_size.columns;

    struct GardenerTask task;
    task.gardener_id = 2;
    task.working_time = 500;

    int i = rows - 1;
    int j = columns - 1;
    task.status = 0;
    while (j >= 0) {
        while (i >= 0)
        {
            task.plot_i = i;
            task.plot_j = j;
            handleGardenPlot(sock, task);
            --i;
        }

        --j;
        ++i;

        while (i < rows)
        {
            task.plot_i = i;
            task.plot_j = j;
            handleGardenPlot(sock, task);
            ++i;
        }

        --i;
        --j;
    }

    task.status = 1;
    handleGardenPlot(sock, task);

    // /* Send the string to the server */
    

    // /* Receive the same string back from the server */
    // totalBytesRcvd = 0;
    // printf("%d) Received: ", 0);            // Setup to print the echoed string
    // while (totalBytesRcvd < echoStringLen) {
    //     /* Receive up to the buffer size (minus 1 to leave space for
    //     a null terminator) bytes from the sender */
    //     if ((bytesRcvd = recv(sock, echoBuffer, 1024 - 1, 0)) <= 0) {
    //         perror("recv() bad");
    //         exit(-1);
    //     }
    //     totalBytesRcvd += bytesRcvd;   /* Keep tally of total bytes */
    //     echoBuffer[bytesRcvd] = '\0';  /* Terminate the string! */
    //     printf("%s", echoBuffer);      /* Print the echo buffer */
    // }

    // printf("\n");    /* Print a final linefeed */

    close(sock);
    exit(0);
}
