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
#include "list.h"

#define COLOR "\x1B[33m"
#define RESET "\x1B[0m"
#define MAX_OPEN_SESSIONS 20

typedef struct session {
    size_t bytes;
    void *buffer;
    int chunks;
} Session;

typedef struct client {
    in_addr_t ip;
    in_port_t port;
} *Client;

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
    static struct sigaction quit_action;
    struct sockaddr *server_ptr = NULL, *client_ptr = NULL;
    struct sockaddr_in server_in_addr, client_in_addr;
    struct hostent *host_entry;
    char *ip = NULL, hostbuffer[256];
    Session sessions[MAX_OPEN_SESSIONS];
    void *rcv_buffer = NULL;
    size_t socket_rcv_size = 0, socket_snd_size = 0;
    socklen_t st_rcv_len = 0, st_snd_len = 0, client_len = 0;
    uint16_t portNum = 0;
    ssize_t bytes = 0;
    fd_set set, read_fds;
    Client new_client = NULL, client = NULL;
    bool duplicate;

    /* Read argument options from command line*/
    readOptions(argc, argv, &portNum);

    st_rcv_len = sizeof(socket_rcv_size);
    st_snd_len = sizeof(socket_snd_size);

    client_len = sizeof(struct sockaddr);

    server_ptr = (struct sockaddr *) &server_in_addr;
    client_ptr = (struct sockaddr *) &client_in_addr;

    memset(server_ptr, 0, sizeof(struct sockaddr));
    memset(client_ptr, 0, sizeof(struct sockaddr));

    /* Set custom signal handler for SIGINT (^c) & SIGQUIT (^\) signals.*/
    quit_action.sa_handler = sig_int_quit_action;
    sigfillset(&(quit_action.sa_mask));
    //quit_action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &quit_action, NULL);
    sigaction(SIGQUIT, &quit_action, NULL);
    sigaction(SIGHUP, &quit_action, NULL);

    client_in_addr.sin_family = AF_INET;
    //client_in_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Initialize address*/
    server_in_addr.sin_family = AF_INET;
    server_in_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_in_addr.sin_port = htons(portNum);

    /* Create socket*/
    if ((fd_server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) <= 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    hostname = gethostname(hostbuffer, sizeof(hostbuffer));

    host_entry = gethostbyname(hostbuffer);

    ip = inet_ntoa(*((struct in_addr *) host_entry->h_addr_list[0]));

    getsockopt(fd_server, SOL_SOCKET, SO_RCVBUF, (void *) &socket_rcv_size, &st_rcv_len);

    getsockopt(fd_server, SOL_SOCKET, SO_SNDBUF, (void *) &socket_snd_size, &st_snd_len);

    rcv_buffer = malloc(socket_rcv_size + 1);

    /* Config*/
    if (setsockopt(fd_server, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(fd_server, SOL_SOCKET, SO_RCVLOWAT, (char *) &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    /* Bind socket*/
    if (bind(fd_server, server_ptr, sizeof(server_in_addr)) < 0) {
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

    for (int i = 0; i < MAX_OPEN_SESSIONS; i++) {
        sessions[i].buffer = NULL;
        sessions[i].bytes = 0;
        sessions[i].chunks = 0;
    }

    List list = NULL;
    listCreate(&list);

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
                    printf("\n::Accept %s:%d on socket %d::\n", inet_ntoa(client_in_addr.sin_addr),
                           ntohs(client_in_addr.sin_port),
                           fd_client);
                    FD_SET(fd_client, &set);
                    if (fd_client > fd_hwm) {
                        fd_hwm = fd_client;
                    }
                    if (fd_client <= MAX_OPEN_SESSIONS) {
                        sessions[fd_client].buffer = malloc(1);
                        sessions[fd_client].bytes = 1;
                        sessions[fd_client].chunks = 0;
                    } else {
                        close(fd_client);
                    }
                } else {
                    bzero(rcv_buffer, socket_rcv_size);
                    bytes = recv(fd, rcv_buffer, socket_rcv_size, MSG_DONTWAIT);
                    if (bytes == 0) {
                        duplicate = false;
                        shutdown(fd_client, SHUT_RD);

                        printf("::%ld bytes were transferred into %d chunks on socket %d::\n", sessions[fd].bytes - 1,
                               sessions[fd].chunks, fd);
                        printf(COLOR"%s\n"RESET"\n", (char *) sessions[fd].buffer);

                        // TODO: Create command handler function.
                        if (strncmp(sessions[fd].buffer, "LOG_ON", 6) == 0) {
                            printf("EXECUTE LOG_ON\n");
                            new_client = malloc(sizeof(struct client));
                            memcpy(new_client, sessions[fd].buffer + 6, sizeof(struct client));
                            while ((client = listNext(list)) != NULL) {
                                if (new_client->ip == client->ip && new_client->port == client->port) {
                                    duplicate = true;
                                }
                            }
                            if (!duplicate && listInsert(list, new_client)) {
                                send(fd, "+", 1, 0);
                            } else {
                                fprintf(stderr, "not inserted!\n");
                                send(fd, "-", 1, 0);
                            }
                            printf("\n::LOG_ON <%d, %d>::\n", ntohl(new_client->ip), ntohs(new_client->port));

                            shutdown(fd_client, SHUT_WR);
                        } else if (strncmp(sessions[fd].buffer, "GET_CLIENTS", 11) == 0) {
                            printf("EXECUTE GET_CLIENTS\n");
                            send(fd, "+", 1, 0);
                            shutdown(fd_client, SHUT_WR);
                        } else if (strncmp(sessions[fd].buffer, "LOG_OFF", 7) == 0) {
                            printf("EXECUTE LOG_OFF\n");
                            send(fd, "+", 1, 0);
                            shutdown(fd_client, SHUT_WR);
                        } else {
                            printf("UNKNOWN COMMAND\n");
                            send(fd, "-", 1, 0);
                            shutdown(fd_client, SHUT_WR);
                        }

                        FD_CLR(fd, &set);
                        if (fd == fd_hwm) {
                            fd_hwm--;
                        }
                        close(fd);
                        free(sessions[fd].buffer);
                        sessions[fd].buffer = NULL;
                    } else if (bytes > 0) {
                        size_t offset = sessions[fd].chunks ? sessions[fd].bytes - 1 : 0;
                        sessions[fd].buffer = realloc(sessions[fd].buffer, sessions[fd].bytes + bytes - 1);
                        strncpy(sessions[fd].buffer + offset, rcv_buffer, (size_t) bytes);
                        sessions[fd].bytes += bytes;
                        sessions[fd].chunks++;
                        printf("::Receive %ld bytes from chunk %d on socket %d::\n", bytes, sessions[fd].chunks, fd);
                        printf(COLOR"%s"RESET"\n", (char *) rcv_buffer);
                    } else {
                        perror("recv");
                    }
                }
            }
        }
    }
}
