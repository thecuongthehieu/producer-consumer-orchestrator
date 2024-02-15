#include <stdio.h>
#include <pthread.h>

#include "tcp_utils.c"

int main() {
    printf("Test tcp_utils begin\n");

    /* Start TCP server in a separated thread */
    pthread_t tcp_server_thread;
    pthread_create(&tcp_server_thread, NULL, start_tcp_server, &default_tcp_connection_handler);
    pthread_join(tcp_server_thread, NULL);

    printf("Test tcp_utils finished\n");
}