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

#define PORT "9000"
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BACKLOG 1

int create_bind_socket();
void handle_connection(int sockfd);
void write_to_file(int connfd);
void write_file_to_sock(int connfd);