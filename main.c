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
List list = NULL;
struct sockaddr_in listen_in_addr, new_client_in_addr, client_in_addr;
struct sockaddr *listen_in_addr_ptr = NULL, *new_client_in_addr_ptr = NULL, *client_in_addr_ptr = NULL;
char *ip = NULL, hostBuffer[256];

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

bool request_handler(int client_fd, void *buffer) {
    bool found = false;
    Client c = NULL, client = NULL;
    unsigned int clients = 0;
    int fd_client = 0;

    if (strncmp(buffer, "LOG_ON", 6) == 0) {
        printf("EXECUTE LOG_ON\n");

        c = malloc(sizeof(struct client));
        memcpy(c, buffer + 6, sizeof(struct client));

        listSetCurrentToStart(list);
        while ((client = listNext(list)) != NULL) {
            if (c->ip == client->ip && c->port == client->port) {
                found = true;
            }
        }

        if (!found && listInsert(list, c)) {
            listSetCurrentToStart(list);
        } else {
            fprintf(stderr, "not inserted!\n");
            free(c);
        }

        while ((client = listNext(list)) != NULL) {
            if (!(c->ip == client->ip && c->port == client->port)) {

                /* Create socket */
                if ((fd_client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
                    perror("socket");
                }
                client_in_addr.sin_family = AF_INET;
                client_in_addr.sin_addr.s_addr = client->ip;
                client_in_addr.sin_port = client->port;

                printf("Connecting to host %s:%d in order to inform that client %s:%d is logged in ...\n",
                       inet_ntoa(client_in_addr.sin_addr), ntohs(client_in_addr.sin_port),
                       inet_ntoa(new_client_in_addr.sin_addr), ntohs(new_client_in_addr.sin_port)
                );

                /* Initiate connection */
                if (connect(fd_client, client_in_addr_ptr, sizeof(struct sockaddr)) < 0) {
                    perror("connect");
                } else {
                    printf("Connect to client %s:%d successfully!\n", inet_ntoa(client_in_addr.sin_addr),
                           ntohs(client_in_addr.sin_port));
                    send(fd_client, "USER_ON", 7, 0);
                    send(fd_client, c, sizeof(struct client), 0);
                    close(fd_client);
                }
            }
        }
        return true;
    } else if (strncmp(buffer, "GET_CLIENTS", 11) == 0) {
        printf("EXECUTE GET_CLIENTS\n");
        c = malloc(sizeof(struct client));
        memcpy(c, buffer + 11, sizeof(struct client));
        clients = listGetLength(list) - 1;
        send(client_fd, "CLIENT_LIST", 11, 0);
        send(client_fd, &clients, sizeof(unsigned int), 0);
        listSetCurrentToStart(list);
        while ((client = listNext(list)) != NULL) {
            if (!(c->ip == client->ip && c->port == client->port)) {
                send(client_fd, &client, sizeof(struct client), 0);
            }
        }
        free(c);
        return true;
    } else if (strncmp(buffer, "LOG_OFF", 7) == 0) {
        printf("EXECUTE LOG_OFF\n");
        c = malloc(sizeof(struct client));
        memcpy(c, buffer + 7, sizeof(struct client));
        listSetCurrentToStart(list);
        while ((client = listNext(list)) != NULL) {
            if (c->ip == client->ip && c->port == client->port) {
                found = true;
                break;
            }
        }
        if (found) {
            listSetCurrentToStart(list);
            if (listRemove(list, client)) {
                listSetCurrentToStart(list);
                while ((client = listNext(list)) != NULL) {
                    if (!(c->ip == client->ip && c->port == client->port)) {

                        /* Create socket */
                        if ((fd_client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
                            perror("socket");
                        }
                        client_in_addr.sin_family = AF_INET;
                        client_in_addr.sin_addr.s_addr = client->ip;
                        client_in_addr.sin_port = client->port;

                        printf("Connecting to host %s:%d in order to inform that client %s:%d leave ...\n",
                               inet_ntoa(client_in_addr.sin_addr), ntohs(client_in_addr.sin_port),
                               inet_ntoa(new_client_in_addr.sin_addr), ntohs(new_client_in_addr.sin_port)
                        );

                        /* Initiate connection */
                        if (connect(fd_client, client_in_addr_ptr, sizeof(struct sockaddr)) < 0) {
                            perror("connect");
                        } else {
                            printf("Connect to client %s:%d successfully!\n", inet_ntoa(client_in_addr.sin_addr),
                                   ntohs(client_in_addr.sin_port));
                            send(fd_client, "USER_ON", 7, 0);
                            send(fd_client, c, sizeof(struct client), 0);
                            close(fd_client);
                        }
                    }
                }
            } else {
                send(client_fd, "ERROR_NOT_REMOVED", 17, 0);
            }
        } else {
            send(client_fd, "ERROR_IP_PORT_NOT_FOUND_IN_LIST", 31, 0);
        }
        free(c);
        return true;
    } else {
        printf("UNKNOWN COMMAND\n");
        return false;
    }
}

int main(int argc, char *argv[]) {
    int opt = 1, fd_listen = 0, fd_new_client = 0, fd_client = 0, activity = 0, fd_max = 0, fd_active = 0, hostname = 0;
    static struct sigaction quit_action;

    struct hostent *host_entry;

    Session sessions[MAX_OPEN_SESSIONS];
    void *rcv_buffer = NULL;
    size_t socket_rcv_size = 0, socket_snd_size = 0;
    socklen_t st_rcv_len = 0, st_snd_len = 0, client_len = 0;
    uint16_t portNum = 0;
    ssize_t bytes = 0;
    fd_set set, read_fds;

    /* Read argument options from command line*/
    readOptions(argc, argv, &portNum);

    st_rcv_len = sizeof(socket_rcv_size);
    st_snd_len = sizeof(socket_snd_size);

    client_len = sizeof(struct sockaddr);

    listen_in_addr_ptr = (struct sockaddr *) &listen_in_addr;
    new_client_in_addr_ptr = (struct sockaddr *) &new_client_in_addr;
    client_in_addr_ptr = (struct sockaddr *) &client_in_addr;

    memset(listen_in_addr_ptr, 0, sizeof(struct sockaddr));
    memset(new_client_in_addr_ptr, 0, sizeof(struct sockaddr));
    memset(client_in_addr_ptr, 0, sizeof(struct sockaddr));

    /* Set custom signal handler for SIGINT (^c) & SIGQUIT (^\) signals.*/
    quit_action.sa_handler = sig_int_quit_action;
    sigfillset(&(quit_action.sa_mask));
    //quit_action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &quit_action, NULL);
    sigaction(SIGQUIT, &quit_action, NULL);
    sigaction(SIGHUP, &quit_action, NULL);

    /* Initialize address*/
    listen_in_addr.sin_family = AF_INET;
    listen_in_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    listen_in_addr.sin_port = htons(portNum);

    /* Create socket*/
    if ((fd_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) <= 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    hostname = gethostname(hostBuffer, sizeof(hostBuffer));

    host_entry = gethostbyname(hostBuffer);

    ip = inet_ntoa(*((struct in_addr *) host_entry->h_addr_list[0]));

    getsockopt(fd_listen, SOL_SOCKET, SO_RCVBUF, (void *) &socket_rcv_size, &st_rcv_len);

    getsockopt(fd_listen, SOL_SOCKET, SO_SNDBUF, (void *) &socket_snd_size, &st_snd_len);

    rcv_buffer = malloc(socket_rcv_size + 1);

    /* Config*/
    if (setsockopt(fd_listen, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(fd_listen, SOL_SOCKET, SO_RCVLOWAT, (char *) &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    /* Bind socket*/
    if (bind(fd_listen, listen_in_addr_ptr, sizeof(listen_in_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    /* Listen*/
    if (listen(fd_listen, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    if (fd_listen > fd_max) {
        fd_max = fd_listen;
    }

    FD_ZERO(&set);
    FD_SET(fd_listen, &set);

    for (int i = 0; i < MAX_OPEN_SESSIONS; i++) {
        sessions[i].buffer = NULL;
        sessions[i].bytes = 0;
        sessions[i].chunks = 0;
    }

    listCreate(&list);

    printf("Waiting for connections on %s:%d ... \n", ip, portNum);
    while (!quit) {
        if (sig_int_quit_hup) {
            sig_int_quit_hup--;
            fprintf(stdout, COLOR"C[%d]: exiting ..."RESET"\n", getpid());
            quit = true;
        }
        read_fds = set;
        if ((activity = select(fd_max + 1, &read_fds, NULL, NULL, NULL) < 0) && (errno != EINTR)) {
            perror("select");
        }

        for (fd_active = 0; fd_active <= fd_max; fd_active++) {
            if (FD_ISSET(fd_active, &read_fds)) {
                if (fd_active == fd_listen) {
                    if ((fd_new_client = accept(fd_active, new_client_in_addr_ptr, &client_len)) < 0) {
                        perror("accept");
                        break;
                    }
                    printf("\n::Accept new client (%s:%d) on socket %d::\n", inet_ntoa(new_client_in_addr.sin_addr),
                           ntohs(new_client_in_addr.sin_port),
                           fd_new_client);
                    FD_SET(fd_new_client, &set);
                    if (fd_new_client > fd_max) {
                        fd_max = fd_new_client;
                    }
                    if (fd_new_client <= MAX_OPEN_SESSIONS) {
                        sessions[fd_new_client].buffer = malloc(1);
                        sessions[fd_new_client].bytes = 1;
                        sessions[fd_new_client].chunks = 0;
                    } else {
                        close(fd_new_client);
                    }
                } else {
                    bzero(rcv_buffer, socket_rcv_size);
                    bytes = recv(fd_active, rcv_buffer, socket_rcv_size, 0);
                    if (bytes == 0) {
                        printf("::%ld bytes were transferred into %d different chunks on socket %d::\n",
                               sessions[fd_active].bytes - 1,
                               sessions[fd_active].chunks, fd_active);
                        printf(COLOR"%s\n"RESET"\n", (char *) sessions[fd_active].buffer);
                        /* Handle request.*/
                        shutdown(fd_new_client, SHUT_RD);
                        if (request_handler(fd_new_client, sessions[fd_active].buffer)) {
                            printf("Request complete successfully.\n");
                        } else {
                            fprintf(stderr, "Request error!\n");
                            send(fd_active, "-", 1, 0);
                        }
                        shutdown(fd_new_client, SHUT_WR);
                        FD_CLR(fd_active, &set);
                        if (fd_active == fd_max) {
                            fd_max--;
                        }
                        close(fd_active);
                        free(sessions[fd_active].buffer);
                        sessions[fd_active].buffer = NULL;
                    } else if (bytes > 0) {
                        size_t offset = sessions[fd_active].chunks ? sessions[fd_active].bytes - 1 : 0;
                        sessions[fd_active].buffer = realloc(sessions[fd_active].buffer,
                                                             sessions[fd_active].bytes + bytes - 1);
                        strncpy(sessions[fd_active].buffer + offset, rcv_buffer, (size_t) bytes);
                        sessions[fd_active].bytes += bytes;
                        sessions[fd_active].chunks++;
                        //printf("::Receive %ld bytes from chunk %d on socket %d::\n", bytes, sessions[fd_active].chunks, fd_active);
                        //printf(COLOR"%s"RESET"\n", (char *) rcv_buffer);
                    } else {
                        perror("recv");
                        send(fd_active, "-", 1, 0);
                        close(fd_active);
                    }
                }
            }
        }
    }

    listDestroy(&list);
}
