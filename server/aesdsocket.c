// Based on https://beej.us/guide/bgnet/html/#a-simple-stream-server

#include "aesdsocket.h"
#include <getopt.h>

int close_server = 0;

void signal_handler(int signum) {
    syslog(LOG_INFO, "Caught signal, exiting");
    close_server = 1;
}

void terminate(int sock_fd) {
    if (sock_fd != -1) {
        close(sock_fd);
    }

    remove(DATA_FILE);

    closelog();
}


int create_bind_socket() {
    int sockfd = -1;  
    struct addrinfo hints, *servinfo, *p;
    int ret;
    int yes = 1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;    
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_flags = AI_PASSIVE;  
 
    if ((ret = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        syslog(LOG_ERR, "getaddrinfo failed with err: %s\n", gai_strerror(ret));
        return -1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            syslog(LOG_ERR, "couldn't set SO_REUSEADDR");
            return -1;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL)  {
        syslog(LOG_ERR, "couldn't bind");
        return -1;
    }

    return sockfd;
}

void handle_connection(int sockfd){
    struct sockaddr conn_addr;
    socklen_t conn_len = sizeof(conn_addr);
    int connfd;

    while (!close_server) {
        connfd = accept(sockfd, &conn_addr, &conn_len);
        if (connfd == -1) {
            syslog(LOG_ERR, "couldnt accept conn");
            continue;
        }
        char s[INET6_ADDRSTRLEN > INET_ADDRSTRLEN ? INET6_ADDRSTRLEN : INET_ADDRSTRLEN];

        switch(conn_addr.sa_family) {
            case AF_INET: {
                struct sockaddr_in *addr_in = ((struct sockaddr_in *)&conn_addr);
                inet_ntop(AF_INET, &(addr_in->sin_addr), s, INET_ADDRSTRLEN);
                break;
            }
            case AF_INET6: {
                struct sockaddr_in6 *addr_in6 = ((struct sockaddr_in6 *)&conn_addr);
                inet_ntop(AF_INET6, &(addr_in6->sin6_addr), s, INET6_ADDRSTRLEN);
                break;
            }
            default:
                break;
        }

        syslog(LOG_INFO, "Accepted connection from %s\n", s);

        write_to_file(connfd);

        close(connfd);
        syslog(LOG_INFO, "Closed connection from %s\n", s);
    }

    terminate(sockfd);
}

void write_to_file(int connfd) {
    char buffer[1024];
    ssize_t bytes_received;

    FILE *data_file = fopen(DATA_FILE, "a");
    if (!data_file) {
        syslog(LOG_ERR, "couldnt open file");
        return;
    }

    while ((bytes_received = recv(connfd, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytes_received, data_file);
        if (memchr(buffer, '\n', bytes_received) != NULL) {
            break; 
        }
    }

    fclose(data_file);

    write_file_to_sock(connfd);
}

void write_file_to_sock(int connfd) {
    char buffer[1024];
    ssize_t bytes_read;

    FILE *data_file = fopen(DATA_FILE, "r");
    if (!data_file) {
        syslog(LOG_ERR, "Failed to open data file");
        return;
    }

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), data_file)) > 0) {
        send(connfd, buffer, bytes_read, 0);
    }

    fclose(data_file);
}

int main(int argc, char *argv[]) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    int opt;
    int daemon_mode = 0;

    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
            case 'd':
                daemon_mode = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-d] (daemon)\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
        syslog(LOG_ERR, "couldnt set signals");
        return -1;
    }

    int sockfd = create_bind_socket();

    if (sockfd != -1 && daemon_mode) {
        pid_t pid, sid;

        pid = fork();
        if (pid < 0) {
            syslog(LOG_ERR, "couldn't fork");
            return -1;
        }
        if (pid > 0) {
            syslog(LOG_INFO, "fork succesful");
            return 0;
        }

        sid = setsid();
        if (sid < 0) {
            syslog(LOG_ERR, "couldn't create session");
            return -1;
        }

        if ((chdir("/")) < 0) {
            syslog(LOG_ERR, "couldnt change process work dir");
            return -1;
        }
    }

    if (listen(sockfd, BACKLOG) == -1) {
            syslog(LOG_ERR, "couldnt listen to socket");
            close(sockfd);
            return -1;
    }

    handle_connection(sockfd);

    return 0; 
}