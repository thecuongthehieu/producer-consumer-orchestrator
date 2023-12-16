#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <sys/wait.h>
#include <semaphore.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

#define PAUSE sleep(2);

#define BUFFER_SIZE 256

// Queue
int readIdx;
int writeIdx;
int buffer[BUFFER_SIZE];
sem_t mutexSem;
sem_t dataAvailableSem;
sem_t roomAvailableSem;

int read_count;
int write_count;
int queue_size;

// Current metrics
int cur_read_count;
int cur_write_count;
int cur_queue_size;
int sd;


static int setup_tcp_client() {
    const char *hostname = "127.0.0.1";
    int port = 6873;
    struct sockaddr_in sin;
    struct hostent *hp;

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

    return sd;
}

/* Consumer Code: the passed argument is not used */
static void *consumer(void *arg)
{
    int item;
    while (1)
    {
        /* Wait for availability of at least one data slot */
        sem_wait(&dataAvailableSem);
        /* Enter critical section */
        sem_wait(&mutexSem);
        /* Get data item */
        item = buffer[readIdx];
        /* Update read index */
        readIdx = (readIdx + 1) % BUFFER_SIZE;

        read_count += 1;
        queue_size -= 1;

        /* Signal that a new empty slot is available */
        sem_post(&roomAvailableSem);
        /* Exit critical section */
        sem_post(&mutexSem);
        /* Consume data item and take actions (e.g return)*/
        // ...

        PAUSE
        PAUSE
    }
}

/* Producer code. Passed argument is not used */
static void *producer(void *arg)
{
    int item = 0;
    while (1)
    {
        /* Produce data item and take actions (e.g. return)*/
        // ...
        /* Wait for availability of at least one empty slot */
        sem_wait(&roomAvailableSem);
        /* Enter critical section */
        sem_wait(&mutexSem);
        /* Write data item */
        buffer[writeIdx] = item;
        /* Update write index */
        writeIdx = (writeIdx + 1) % BUFFER_SIZE;

        write_count += 1;
        queue_size += 1;

        /* Signal that a new data slot is available */
        sem_post(&dataAvailableSem);
        /* Exit critical section */
        sem_post(&mutexSem);

        PAUSE
    }
}

static void send_metrics() {
    char msg[256];
    sprintf(msg, "%d:%d:%d%c", cur_write_count, cur_read_count, cur_queue_size, '\n');

    printf("Msg: %s", msg);
    
    // /* Send the msg */
    // if (send(sd, msg, strlen(msg), 0) == -1)
    // {
    //     printf("send failed");
    // } else {
    //     printf("sent successfully\n");
    // }
} 

static void *orchestrator(void *args) {
    while (1) {
        /* Enter critical section */
        sem_wait(&mutexSem);

        cur_read_count = read_count;
        cur_write_count = write_count;
        cur_queue_size = queue_size;

        /* Exit critical section */
        sem_post(&mutexSem);

        /* Send metrics */
        send_metrics();

        PAUSE
    }
}

/* Main program */
int main(int argc, char *args[])
{
    int i;
    sem_init(&mutexSem, 1, 1);
    sem_init(&dataAvailableSem, 1, 0);
    sem_init(&roomAvailableSem, 1, BUFFER_SIZE);

    int sd = 0; // setup_tcp_client();

    pthread_t prod_thread, cons_thread, orch_thread; 

    /* Create producer thread */
    pthread_create(&prod_thread, NULL, producer, NULL);
    /* Create consumer thread */
    pthread_create(&cons_thread, NULL, consumer, NULL);
    /* Create consumer thread */
    pthread_create(&orch_thread, NULL, orchestrator, NULL);
    
    pthread_join(prod_thread, NULL);
    pthread_join(cons_thread, NULL);
    pthread_join(orch_thread, NULL);

    return 0;
}