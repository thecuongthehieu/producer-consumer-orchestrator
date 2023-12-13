#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define FALSE 0
#define TRUE 1

/* Main client program. The IP address and the port number of
   the server are passed in the command line. After establishing
   a connection, the program will read commands from the terminal
   and send them to the server. The returned answer string is
   then printed. */
int main(int argc, char **argv)
{
    char hostname[100];
    char *answer;
    int sd;
    int port;
    int stopped = FALSE;
    int len;
    unsigned int netLen;
    struct sockaddr_in sin;
    struct hostent *hp;
    /* Check number of arguments and get IP address and port */
    if (argc < 3)
    {
        printf("Usage: client <hostname> <port>\n");
        exit(0);
    }
    sscanf(argv[1], "%s", hostname);
    sscanf(argv[2], "%d", &port);

    /* Resolve the passed name and store the resulting long representation
       in the struct hostent variable */
    if ((hp = gethostbyname(hostname)) == 0)
    {
        perror("gethostbyname");
        exit(1);
    }
    /* fill in the socket structure with host information */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    sin.sin_port = htons(port);
    /* create a new socket */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(1);
    }
    /* connect the socket to the port and host
       specified in struct sockaddr_in */
    if (connect(sd, (struct sockaddr *)&sin, sizeof(sin)) == -1)
    {
        perror("connect");
        exit(1);
    }
    while (!stopped)
    {
        char command[256];
        /* Get a string command from terminal */
        printf("Enter command: ");
        scanf("%s", command);
        if (!strcmp(command, "quit"))
            break;
        
        printf("command=%s\n", command);
        
        len = strlen(command);
        command[len] = '\n';

        /* Send the command */
        if (send(sd, command, len + 1, 0) == -1)
        {
            perror("send");
            exit(0);
        } else {
            printf("sent okay\n");
        }
    }
    /* Close the socket */
    close(sd);
    return 0;
}
