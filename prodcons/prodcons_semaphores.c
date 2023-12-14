#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#define BUFFER_SIZE 128
/* Shared Buffer, indexes and semaphores are held in shared memory
   readIdx is the index in the buffer of the next item to be retrieved
   writeIdx is the index in the buffer of the next item to be inserted
   Buffer empty condition corresponds to readIdx == writeIdx
   Buffer full condition corresponds to
   (writeIdx + 1)%BUFFER_SIZE == readIdx)
   Semaphores used for synchronization:
   mutexSem is used to protect the critical section
   dataAvailableSem is used to wait for data avilability
   roomAvailableSem is used to wait for room abailable in the buffer */

#define PAUSE sleep(2);

struct BufferData
{
    int readIdx;
    int writeIdx;
    int buffer[BUFFER_SIZE];
    sem_t mutexSem;
    sem_t dataAvailableSem;
    sem_t roomAvailableSem;

    // Metrics
    int read_count;
    int write_count;
    int queue_size;
};

struct BufferData *sharedBuf;

/* Consumer routine */
static void consumer()
{
    int item;
    while (1)
    {
        /* Wait for availability of at least one data slot */
        sem_wait(&sharedBuf->dataAvailableSem);
        /* Enter critical section */
        sem_wait(&sharedBuf->mutexSem);
        /* Get data item */
        item = sharedBuf->buffer[sharedBuf->readIdx];
        /* Update read index */
        sharedBuf->readIdx = (sharedBuf->readIdx + 1) % BUFFER_SIZE;

        sharedBuf->read_count += 1;
        sharedBuf->queue_size -= 1;

        /* Signal that a new empty slot is available */
        sem_post(&sharedBuf->roomAvailableSem);
        /* Exit critical section */
        sem_post(&sharedBuf->mutexSem);
        /* Consume data item and take actions (e.g return)*/
        // ...

        PAUSE
        PAUSE
    }
}

/* Producer routine */
static void producer()
{
    int item = 0;
    while (1)
    {
        /* Produce data item and take actions (e.g. return)*/
        // ...
        /* Wait for availability of at least one empty slot */
        sem_wait(&sharedBuf->roomAvailableSem);
        /* Enter critical section */
        sem_wait(&sharedBuf->mutexSem);
        /* Write data item */
        sharedBuf->buffer[sharedBuf->writeIdx] = item;
        /* Update write index */
        sharedBuf->writeIdx = (sharedBuf->writeIdx + 1) % BUFFER_SIZE;

        sharedBuf->write_count += 1;
        sharedBuf->queue_size += 1;

        /* Signal that a new data slot is available */
        sem_post(&sharedBuf->dataAvailableSem);
        /* Exit critical section */
        sem_post(&sharedBuf->mutexSem);

        PAUSE
    }
}

/* Main program */
int main(int argc, char *args[])
{
    ///////////////////////////////////
    // Setup shared mem
    ///////////////////////////////////

    int memId;
    int i;
    
    /* Set-up shared memory */
    memId = shmget(IPC_PRIVATE, sizeof(struct BufferData), SHM_R | SHM_W);
    if (memId == -1)
    {
        perror("Error in shmget");
        exit(0);
    }
    sharedBuf = shmat(memId, NULL, 0);
    if (sharedBuf == (void *)-1)
    {
        perror("Error in shmat");
        exit(0);
    }
    /* Initialize buffer indexes */
    sharedBuf->readIdx = 0;
    sharedBuf->writeIdx = 0;

    
    sharedBuf->read_count = 0;
    sharedBuf->write_count = 0;
    sharedBuf->queue_size = 0;

    /* Initialize semaphores. Initial value is 1 for mutexSem,
       0 for dataAvailableSem (no filled slots initially available)
       and BUFFER_SIZE for roomAvailableSem (all slots are
       initially free). The second argument specifies
       that the semaphore is shared among processes */
    sem_init(&sharedBuf->mutexSem, 1, 1);
    sem_init(&sharedBuf->dataAvailableSem, 1, 0);
    sem_init(&sharedBuf->roomAvailableSem, 1, BUFFER_SIZE);

    ///////////////////////////////////
    // Setup TCP Client
    ///////////////////////////////////

    const char *hostname = "127.0.0.1";
    int port = 6873;
    int sd;
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


    ///////////////////////////////////
    // Run Prod Cons Orchestrator
    ///////////////////////////////////

    pid_t prod_pid, cons_pid;

    /* Launch producer process */
    prod_pid = fork();
    if (prod_pid == 0)
    {
        /* Child process */
        producer();
        exit(0);
    }
    /* Launch consumer process */
    cons_pid = fork();
    if (cons_pid == 0)
    {
        consumer();
        exit(0);
    }

    /* Launch orchestrator */
    while (1) {
        int cur_read_count;
        int cur_write_count;
        int cur_queue_size;

        /* Enter critical section */
        sem_wait(&sharedBuf->mutexSem);

        cur_read_count = sharedBuf->read_count;
        cur_write_count = sharedBuf->write_count;
        cur_queue_size = sharedBuf->queue_size;

        /* Exit critical section */
        sem_post(&sharedBuf->mutexSem);

        char msg[256];
        sprintf(msg, "%d:%d:%d%c", cur_read_count, cur_write_count, cur_queue_size, '\n');
       
        /* Send the msg */
        if (send(sd, msg, strlen(msg), 0) == -1)
        {
            printf("send failed");
        } else {
            printf("sent successfully\n");
        }

        PAUSE
    }


    // /* Wait process termination */
    // waitpid(prod_pid, NULL, 0);
    // waitpid(cons_pid, NULL, 0);

    return 0;
}