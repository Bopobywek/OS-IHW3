#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

    if (argc != 4)    /* Test for correct number of arguments */
    {
       fprintf(stderr, "Usage: %s <Server address> <Server port> <Echo string>\n",
               argv[0]);
       exit(1);
    }

    servIP = argv[1];             /* First arg: server IP address (dotted quad) */
    echoString = argv[3];         /* Second arg: string to echo */

    if (argc == 4)
        echoServPort = atoi(argv[2]); /* Use given port, if any */
    else
        echoServPort = 7;  /* 7 is the well-known port for the echo service */

    /* Create a reliable, stream socket using TCP */
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

    echoStringLen = strlen(echoString);          /* Determine input length */

    for(int i = 0; i < 10; ++i) {
        /* Send the string to the server */
        if (send(sock, echoString, echoStringLen, 0) != echoStringLen) {
                perror("send() bad");
                exit(-1);
            }

        /* Receive the same string back from the server */
        totalBytesRcvd = 0;
        printf("%d) Received: ", i);            // Setup to print the echoed string
        while (totalBytesRcvd < echoStringLen) {
            /* Receive up to the buffer size (minus 1 to leave space for
            a null terminator) bytes from the sender */
            if ((bytesRcvd = recv(sock, echoBuffer, 1024 - 1, 0)) <= 0) {
                perror("recv() bad");
                exit(-1);
            }
            totalBytesRcvd += bytesRcvd;   /* Keep tally of total bytes */
            echoBuffer[bytesRcvd] = '\0';  /* Terminate the string! */
            printf("%s", echoBuffer);      /* Print the echo buffer */
        }

        printf("\n");    /* Print a final linefeed */
        sleep(2);
    }

    close(sock);
    exit(0);
}
