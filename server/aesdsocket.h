#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <syslog.h>
#include <pthread.h>
#include "queue.h"

#define PORT "9000"
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BACKLOG 1

typedef struct thread_data{
    pthread_t thread_id; 
    int connfd;
    pthread_mutex_t *mutex;
    int complete;
    char* connaddr;
    SLIST_ENTRY(thread_data) entries; 
}thread_data_t;

int create_bind_socket();
void handle_connection(int sockfd);
void write_to_file(int connfd, pthread_mutex_t* mutex);
void write_file_to_sock(int connfd, pthread_mutex_t* mutex);