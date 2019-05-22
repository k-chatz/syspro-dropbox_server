#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <zconf.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <bits/types/sig_atomic_t.h>
#include <signal.h>
#include <netdb.h>

#define COLOR "\x1B[33m"
#define RESET "\x1B[0m"

typedef struct client {
    struct in_addr ip;
    in_port_t port;
} Client;

volatile sig_atomic_t sig_int_quit_hup = 0;
bool quit = false;

void wrongOptionValue(char *opt, char *val) {
    fprintf(stderr, "\nWrong value [%s] for option '%s'\n", val, opt);
    exit(EXIT_FAILURE);
}

/**
 * Read options from command line*/
void readOptions(int argc, char **argv, uint16_t *portNum) {
    int i;
    char *opt, *optVal;
    for (i = 1; i < argc; ++i) {
        opt = argv[i];
        optVal = argv[i + 1];
        if (strcmp(opt, "-p") == 0) {
            if (optVal != NULL && optVal[0] != '-') {
                *portNum = (uint16_t) strtol(optVal, NULL, 10);
            } else {
                wrongOptionValue(opt, optVal);
            }
        }
    }
}

/**
 * @Signal_handler
 * Interupt or quit action*/
void sig_int_quit_action(int signal) {
    sig_int_quit_hup++;
}

int main(int argc, char *argv[]) {
    int opt = 1, fd_server = 0, fd_client = 0, activity = 0, fd_hwm = 0, fd = 0, hostname = 0;
    void *buffer = NULL;
    static struct sigaction quit_action;
    struct sockaddr *server_ptr = NULL, *client_ptr = NULL;
    struct sockaddr_in server, client;
    struct hostent *host_entry;
    char *ip = NULL, hostbuffer[256];
    size_t socket_size = 0;
    socklen_t st_len = 0;
    socklen_t client_len = 0;
    uint16_t portNum = 0;
    ssize_t bytes = 0;
    fd_set set, read_fds;

    /* Read argument options from command line*/
    readOptions(argc, argv, &portNum);

    st_len = sizeof(socket_size);
    client_len = sizeof(struct sockaddr);

    server_ptr = (struct sockaddr *) &server;
    client_ptr = (struct sockaddr *) &client;

    memset(server_ptr, 0, sizeof(struct sockaddr));
    memset(client_ptr, 0, sizeof(struct sockaddr));

    /* Set custom signal handler for SIGINT (^c) & SIGQUIT (^\) signals.*/
    quit_action.sa_handler = sig_int_quit_action;
    sigfillset(&(quit_action.sa_mask));
    //quit_action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &quit_action, NULL);
    sigaction(SIGQUIT, &quit_action, NULL);
    sigaction(SIGHUP, &quit_action, NULL);

    client.sin_family = AF_INET;
    //client.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Initialize address*/
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(portNum);

    /* Create socket*/
    if ((fd_server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) <= 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    hostname = gethostname(hostbuffer, sizeof(hostbuffer));

    host_entry = gethostbyname(hostbuffer);

    ip = inet_ntoa(*((struct in_addr *) host_entry->h_addr_list[0]));

    getsockopt(fd_server, SOL_SOCKET, SO_RCVBUF, (void *) &socket_size, &st_len);

    buffer = malloc(socket_size + 1);

    /* Config*/
    if (setsockopt(fd_server, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    /* Bind socket*/
    if (bind(fd_server, server_ptr, sizeof(server)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    /* Listen*/
    if (listen(fd_server, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    if (fd_server > fd_hwm) {
        fd_hwm = fd_server;
    }

    FD_ZERO(&set);

    FD_SET(fd_server, &set);

    printf("Waiting for connections on %s:%d ... \n", ip, portNum);
    while (!quit) {

        if (sig_int_quit_hup) {
            sig_int_quit_hup--;
            fprintf(stdout, COLOR"C[%d]: exiting ..."RESET"\n", getpid());
            quit = true;
        }

        read_fds = set;

        if ((activity = select(fd_hwm + 1, &read_fds, NULL, NULL, NULL) < 0) && (errno != EINTR)) {
            perror("select");
        }

        for (fd = 0; fd <= fd_hwm; fd++) {
            if (FD_ISSET(fd, &read_fds)) {
                if (fd == fd_server) {
                    if ((fd_client = accept(fd_server, client_ptr, &client_len)) < 0) {
                        perror("accept");
                        break;
                    }
                    printf("::Accept %s:%d on socket %d::\n",
                           inet_ntoa(client.sin_addr),
                           ntohs(client.sin_port),
                           fd_client);

                    FD_SET(fd_client, &set);
                    if (fd_client > fd_hwm) {
                        fd_hwm = fd_client;
                    }
                } else {
                    bytes = recv(fd_client, buffer, socket_size, 0);
                    printf("::Receive %ld bytes from socket %d::\n", bytes, fd_client);
                    if (bytes == 0) {
                        FD_CLR(fd, &set);
                        if (fd == fd_hwm) {
                            fd_hwm--;
                        }
                        close(fd);
                    } else if (bytes > 0) {
                        printf(COLOR"%s"RESET"\n", (char *) buffer);

                        if (strncmp(buffer, "LOG_ON", 7) == 0) {
                            printf("LOG_ON\n");
                        } else if (strncmp(buffer, "GET_CLIENTS", 11) == 0) {
                            printf("GET_CLIENTS\n");
                        } else if (strncmp(buffer, "LOG_OFF", 7) == 0) {
                            printf("LOG_OFF\n");
                        } else {
                            printf("UNKNOWN COMMAND\n");
                        }

                        send(fd, "OK", strlen("OK"), 0);
                    } else {
                        perror("recv");
                    }

                }
            }
        }
    }
}
