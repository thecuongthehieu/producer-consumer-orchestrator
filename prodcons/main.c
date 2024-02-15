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
#include "tcp_utils.c"

// Address and Port of Metric Server
#define METRIC_SERVER_HOSTNAME "localhost"
#define METRIC_SERVER_PORT 6873

#define PAUSE sleep(1);

#define BUFFER_SIZE 256

// Set mode = 1 to send metrics to Metric Server over TCP socket
int mode = 1;

// TCP socket descriptor of Metric Server
int metric_server_sd;

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

// Init rate values
// rate = number of permits per second
double initial_prod_rate = 10.0;
double initial_cons_rate = 10.0;
double initial_orch_rate = 10.0;
int queue_size_threshold = 64;

double rate_change_step = 0.5;

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

        /* Update metrics */
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

        /* Limit rate */
        acquire(prod_rate_limiter);

        /* Enter critical section */
        sem_wait(mutex_sem);

        /* Write data item */
        buffer[writeIdx] = item;
        /* Update write index */
        writeIdx = (writeIdx + 1) % BUFFER_SIZE;

        /* Update metrics */
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

    int cur_prod_rate = (int) get_rate(prod_rate_limiter);
    sprintf(msg, "%d:%d:%d:%d:%d%c", cur_prod_rate, cur_prod_count, cur_cons_count, cur_queue_size, queue_size_threshold, '\n');
    
    // For debugging
    // printf("Msg: %s", msg);
    
    if (mode == 1) {
        /* Send the msg */
        if (send(metric_server_sd, msg, strlen(msg), 0) == -1)
        {
            printf("ERROR: Send metrics failed");
        }
    }
}

static void control_flow(int cur_queue_size) {
    double cur_prod_rate = get_rate(prod_rate_limiter);
    double cur_cons_rate = get_rate(cons_rate_limiter);
    double updated_prod_rate = cur_prod_rate;
    if (cur_queue_size > queue_size_threshold) {
        // decreasing
        updated_prod_rate -= rate_change_step;

        // min rate = cur_cons_rate / 2
        if (updated_prod_rate < cur_cons_rate / 2) {
            updated_prod_rate = cur_cons_rate / 2;
        }
    } else {
        // increasing
        updated_prod_rate += rate_change_step;

        // max rate = 2 * cons_rate
        if (updated_prod_rate > cur_cons_rate * 2) {
            updated_prod_rate = cur_cons_rate * 2;
        }
    }

    // update rate of producer
    set_rate(prod_rate_limiter, updated_prod_rate);
}

/* Orchestrator Code: the passed argument is not used */
static void *orchestrator(void *args) {
    while (1) {
        // Current metrics
        int cur_prod_count;
        int cur_cons_count;
        int cur_queue_size;

        /* Limit rate */
        acquire(orch_rate_limiter);

        /* Enter critical section */
        sem_wait(mutex_sem);

        /* Take a snapshot of the metrics */
        cur_cons_count = cons_count;
        cur_prod_count = prod_count;
        cur_queue_size = queue_size;

        /* Exit critical section */
        sem_post(mutex_sem);

        /* Send metrics */
        send_metrics(cur_prod_count, cur_cons_count, cur_queue_size);

        // Control flow based on the queue size
        control_flow(cur_queue_size);
    }
}

/* Handle TCP connection to update metrics */
static void handle_metric_update_request(int conn_sd) {
    int max_msg_len = 256;
    char *client_msg, *server_msg;

    server_msg = strdup("Message Format = new_cons_rate:new_orch_rate:new_queue_size_threshold (Use '_' to keep the current values)\n");
    /* Send the format of update message */
    if (send(conn_sd, server_msg, strlen(server_msg), 0) == -1) {
        return;
    }
    free(server_msg);

    for (;;)
    {
        client_msg = malloc(max_msg_len);
        
        /* Get the command and write terminator */
        if (receive(conn_sd, client_msg, max_msg_len) != -1) {

            /* Extract metrics values */
            char *token = strtok(client_msg, ":");
            if (strcmp(token, "_") != 0) {
                double new_cons_rate = (double) atoi(token);
                // printf("new_cons_rate=%f\n", new_cons_rate);
                set_rate(cons_rate_limiter, new_cons_rate);
            }

            token = strtok(NULL, ":");
            if (strcmp(token, "_") != 0) {
                double new_orch_rate = (double) atoi(token);
                // printf("new_orch_rate=%f\n", new_orch_rate);
                set_rate(orch_rate_limiter, new_orch_rate);
            }

            token = strtok(NULL, ":");
            if (strcmp(token, "_") != 0) {
                int new_queue_size_threshold = atoi(token);
                // printf("new_queue_size_threshold=%d\n", new_queue_size_threshold);
                queue_size_threshold = new_queue_size_threshold;
            }            

            /* get the answer character string */
            server_msg = strdup("Metrics Updated\n");

            /* Send answer characters */
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

/* Main program */
int main(int argc, char *args[])
{
    /* Get args */
    // TODO

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
    prod_rate_limiter = get_rate_limiter(initial_prod_rate);
    cons_rate_limiter = get_rate_limiter(initial_cons_rate);
    orch_rate_limiter = get_rate_limiter(initial_orch_rate);

    /* Init TCP client */
    metric_server_sd = 0;
    if (mode == 1) {
        metric_server_sd = setup_tcp_client(METRIC_SERVER_HOSTNAME, METRIC_SERVER_PORT);
    }

    /* Start TCP server in a separated thread */
    pthread_t tcp_server_thread;
    pthread_create(&tcp_server_thread, NULL, start_tcp_server, &handle_metric_update_request);

    /* Create producer, consumer, orchestrator threads */
    pthread_t prod_thread, cons_thread, orch_thread; 
    pthread_create(&prod_thread, NULL, producer, NULL);
    pthread_create(&cons_thread, NULL, consumer, NULL);
    pthread_create(&orch_thread, NULL, orchestrator, NULL);
    
    printf("Operating...\n");

    /* Wait */
    pthread_join(tcp_server_thread, NULL);
    pthread_join(prod_thread, NULL);
    pthread_join(cons_thread, NULL);
    pthread_join(orch_thread, NULL);

    return 0;
}