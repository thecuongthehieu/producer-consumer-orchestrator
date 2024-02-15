#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#define TCP_SERVER_PORT 7368

/* Receive routine: use recv to receive from socket and manage
   the fact that recv may return after having read less bytes than
   the passed buffer size
   In most cases recv will read ALL requested bytes, and the loop body
   will be executed once. This is not however guaranteed and must
   be handled by the user program. The routine returns 0 upon
   successful completion, -1 otherwise */
static int receive(int conn_sd, char *rev_buf, int max_len)
{
    int total_len, cur_len;
    total_len = 0;
    while (total_len < max_len)
    {
        cur_len = recv(conn_sd, &rev_buf[total_len], max_len - total_len, 0);
        if (cur_len <= 0) {
            /* An error occurred (e.g. client disconnected)*/
            printf("Error: %s\n", strerror(errno));
            return -1;
        }

        total_len += cur_len;
        if (rev_buf[total_len - 1] == '\n') {
            rev_buf[total_len - 1] = '\0';
            break;
        }
    }
    return 0;
}

/* Handle an established tcp connection routine */
void default_tcp_connection_handler(int conn_sd)
{
    int max_msg_len = 256;
    char *client_msg, *server_msg;

    for (;;)
    {
        /* Receive the message and respond */
        client_msg = malloc(max_msg_len);
        if (receive(conn_sd, client_msg, max_msg_len) != -1) {
            server_msg = strdup("Server Message\n");

            if (send(conn_sd, server_msg, strlen(server_msg), 0) == -1) {
                break;
            }

            free(client_msg);
            free(server_msg);
        } else {
            break;
        }
    }
    close(conn_sd);
}

/* Start TCP server to receive updates */
void *start_tcp_server(void *arg)
{
    // Casting from void * to handler function pointer
    void (*connection_handler)(int) = (void (*)(int)) arg;

    int sd, conn_sd;
    struct sockaddr_in sin, cli;
    socklen_t cli_len;

    /* Create a new socket */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(1);
    }

    /* set socket options SO_REUSEADDR */
    int reuse = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");
    
    /* set socket options SO_REUSEPORT */
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, (const char *)&reuse, sizeof(reuse)) < 0)
        perror("setsockopt(SO_REUSEPORT) failed");

    /* Initialize the address (struct sokaddr_in) fields */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(TCP_SERVER_PORT);

    /* Bind the socket to the specified port number */
    if (bind(sd, (struct sockaddr *)&sin, sizeof(sin)) == -1)
    {
        perror("bind");
        exit(1);
    }
    /* Set the maximum queue length for clients requesting connection to 1 */
    if (listen(sd, 1) == -1)
    {
        perror("listen");
        exit(1);
    }

    printf("TCP server is running...\n");

    cli_len = sizeof(cli);
    /* Accept and serve all incoming connections in a loop */
    for (;;)
    {
        if ((conn_sd = accept(sd, (struct sockaddr *)&cli, &cli_len)) == -1)
        {
            perror("accept");
            exit(1);
        }
        /* When execution reaches this point a client established the connection.
           The returned socket (conn_sd) is used to communicate with the client */
        // printf("Connection received from %s\n", inet_ntoa(retSin.sin_addr));

        connection_handler(conn_sd);
    }

    printf("TCP server stopped\n");
}

/* Setup TCP client to send metrics */
int setup_tcp_client(const char* hostname, int port) {
    int target_sd;
    struct sockaddr_in sin;
    struct hostent *hp;

    /* Resolve the passed name and store the resulting long representation in the struct hostent variable */
    if ((hp = gethostbyname(hostname)) == 0)
    {
        perror("gethostbyname");
        exit(1);
    }

    /* Fill in the socket structure with host information */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    sin.sin_port = htons(port);

    /* Create a new socket */
    if ((target_sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(1);
    }

    /* Connect the socket to the port and host specified in struct sockaddr_in */
    if (connect(target_sd, (struct sockaddr *)&sin, sizeof(sin)) == -1)
    {
        perror("connect");
        exit(1);
    }

    return target_sd;
}