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

#include "rate_limiter.c"

#define PAUSE sleep(2);

#define BUFFER_SIZE 256

#define QUEUE_THRESHOLD 128

// Queue
int readIdx;
int writeIdx;
int buffer[BUFFER_SIZE];
sem_t *mutex_sem;
sem_t *data_available_sem;
sem_t *room_vailable_sem;

RateLimiter *prod_rate_limiter;
RateLimiter *cons_rate_limiter;
RateLimiter *orch_rate_limiter;

int prod_count;
int cons_count;
int queue_size;

// TCP socket descriptor
int sd;

// mode = 1 to send metrics over TCP socket
int mode = 0;

/* Setup TCP client to send metrics */
static int setup_tcp_client() {
    const char *hostname = "127.0.0.1";
    int port = 6873;
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
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(1);
    }

    /* Connect the socket to the port and host specified in struct sockaddr_in */
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
        sem_wait(data_available_sem);

        /* Limit rate */
        acquire(cons_rate_limiter);

        /* Enter critical section */
        sem_wait(mutex_sem);
        /* Get data item */
        item = buffer[readIdx];
        /* Update read index */
        readIdx = (readIdx + 1) % BUFFER_SIZE;

        cons_count += 1;
        queue_size -= 1;

        /* Signal that a new empty slot is available */
        sem_post(room_vailable_sem);
        /* Exit critical section */
        sem_post(mutex_sem);
        
        /* Consume data item and take actions (e.g return) */
    }
}

/* Producer code. Passed argument is not used */
static void *producer(void *arg)
{
    int item = 0;
    while (1)
    {
        /* Wait for availability of at least one empty slot */
        sem_wait(room_vailable_sem);

        /* Limit rate*/
        acquire(prod_rate_limiter);

        /* Enter critical section */
        sem_wait(mutex_sem);
        /* Write data item */
        buffer[writeIdx] = item;
        /* Update write index */
        writeIdx = (writeIdx + 1) % BUFFER_SIZE;

        prod_count += 1;
        queue_size += 1;

        /* Signal that a new data slot is available */
        sem_post(data_available_sem);
        /* Exit critical section */
        sem_post(mutex_sem);

        /* Produce data item and take actions (e.g return) */
    }
}

static void send_metrics(int cur_prod_count, int cur_cons_count, int cur_queue_size) {
    char msg[256];
    sprintf(msg, "%d:%d:%d%c", cur_prod_count, cur_cons_count, cur_queue_size, '\n');

    printf("Msg: %s", msg);
    
    if (mode == 1) {
        /* Send the msg */
        if (send(sd, msg, strlen(msg), 0) == -1)
        {
            printf("Send metrics failed");
        } else {
            printf("Sent metrics successfully\n");
        }
    }
} 

/* Orchestrator Code: the passed argument is not used */
static void *orchestrator(void *args) {
    while (1) {
        // Current metrics
        int cur_prod_count;
        int cur_cons_count;
        int cur_queue_size;

        /* Limit rate*/
        acquire(orch_rate_limiter);

        /* Enter critical section */
        sem_wait(mutex_sem);

        // TODO : Change the rate 
        printf("Info: %d:%d:%d \t", prod_count, cons_count, queue_size);

        /* Take a snapshot of the metrics */
        cur_cons_count = cons_count;
        cur_prod_count = prod_count;
        cur_queue_size = queue_size;

        /* Exit critical section */
        sem_post(mutex_sem);

        /* Send metrics */
        send_metrics(cur_prod_count, cur_cons_count, cur_queue_size);
    }
}

/* Main program */
int main(int argc, char *args[])
{
    /* Get args */ 
    // rate = number of permits per second
    double prod_rate = 10.0;
    double cons_rate = 10.0;
    double orch_rate = 5.0;

    printf("Beginning\n");

    /* Unlink existed named semaphores before opening new ones with the same name */
    if (sem_unlink("/mutex_sem") == -1) {
        perror("Failed to unlink /mutux_sem");
    }
    printf("Unlinked /mutex_sem\n");

    if (sem_unlink("/data_available_sem") == -1) {
        perror("Failed to unlink /data_available_sem");
    }
    printf("Unlinked /data_available_sem\n");

    if (sem_unlink("/room_vailable_sem") == -1) {
        perror("Failed to unlink /room_vailable_sem");
    }
    printf("Unlinked /room_vailable_sem\n");

    /* Use named semaphore */
    mutex_sem = sem_open("/mutex_sem", O_EXCL | O_CREAT, 0644, 1);
    if (mutex_sem == SEM_FAILED) {
        perror("Failed to open semphore for mutex_sem");
        exit(EXIT_FAILURE);
    }
    printf("Obtained /mutex_sem\n");

    data_available_sem = sem_open("/data_available_sem", O_EXCL | O_CREAT, 0644, 0);
    if (data_available_sem == SEM_FAILED) {
        perror("Failed to open semphore for data_available_sem");
        exit(EXIT_FAILURE);
    }
    printf("Obtained /data_available_sem\n");

    room_vailable_sem = sem_open("/room_vailable_sem", O_EXCL | O_CREAT, 0644, BUFFER_SIZE);
    if (room_vailable_sem == SEM_FAILED) {
        perror("Failed to open semphore for room_vailable_sem");
        exit(EXIT_FAILURE);
    }
    printf("Obtained /room_vailable_sem\n");

    /* Init rate limiters */
    prod_rate_limiter = get_rate_limiter(prod_rate);
    cons_rate_limiter = get_rate_limiter(cons_rate);
    orch_rate_limiter = get_rate_limiter(orch_rate);

    /* Init TCP client */
    sd = 0;
    if (mode == 1) {
        sd = setup_tcp_client();
    }

    pthread_t prod_thread, cons_thread, orch_thread; 

    /* Create producer thread */
    pthread_create(&prod_thread, NULL, producer, NULL);
    /* Create consumer thread */
    pthread_create(&cons_thread, NULL, consumer, NULL);
    /* Create consumer thread */
    pthread_create(&orch_thread, NULL, orchestrator, NULL);
    
    printf("Operating...\n");

    /* Wait */
    pthread_join(prod_thread, NULL);
    pthread_join(cons_thread, NULL);
    pthread_join(orch_thread, NULL);

    return 0;
}