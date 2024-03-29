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
#include "handler.h"
#include "options.h"

#define COLOR "\x1B[33m"
#define RESET "\x1B[0m"

typedef struct session {
    size_t bytes;
    void *buffer;
    int chunks;
} Session;

static volatile int quit_request = 0;

List list = NULL;
struct sockaddr_in listen_in_addr, new_client_in_addr, client_in_addr;
struct sockaddr *listen_in_addr_ptr = NULL, *new_client_in_addr_ptr = NULL, *client_in_addr_ptr = NULL;
char *currentHostStrIp = NULL, hostBuffer[256];

/**
 * Signal handler.*/
static void hdl(int sig) {
    quit_request = 1;
}

int main(int argc, char *argv[]) {
    int opt = 1, fd_listen = 0, fd_new_client = 0, activity = 0, lfd = 0, fd_active = 0;
    struct hostent *hostEntry;
    struct sigaction sa;
    struct timespec timeout;
    struct in_addr currentHostAddr;
    Session s[FD_SETSIZE];
    void *rcv_buffer = NULL;
    size_t socket_rcv_size = 0, socket_snd_size = 0;
    socklen_t st_rcv_len = 0, st_snd_len = 0, client_len = 0;
    uint16_t portNum = 0;
    ssize_t bytes = 0;
    fd_set set, read_fds;

    /* Read argument options from command line*/
    readOptions(argc, argv, &portNum);

    timeout.tv_sec = 3600;
    timeout.tv_nsec = 0;

    st_rcv_len = sizeof(socket_rcv_size);
    st_snd_len = sizeof(socket_snd_size);
    client_len = sizeof(struct sockaddr);

    listen_in_addr_ptr = (struct sockaddr *) &listen_in_addr;
    new_client_in_addr_ptr = (struct sockaddr *) &new_client_in_addr;
    client_in_addr_ptr = (struct sockaddr *) &client_in_addr;

    memset(listen_in_addr_ptr, 0, sizeof(struct sockaddr));
    memset(new_client_in_addr_ptr, 0, sizeof(struct sockaddr));
    memset(client_in_addr_ptr, 0, sizeof(struct sockaddr));

    /* Initialize address*/
    listen_in_addr.sin_family = AF_INET;
    listen_in_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    listen_in_addr.sin_port = htons(portNum);

    /* Create socket*/
    if ((fd_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) <= 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    gethostname(hostBuffer, sizeof(hostBuffer));
    hostEntry = gethostbyname(hostBuffer);
    currentHostAddr = *((struct in_addr *) hostEntry->h_addr_list[0]);
    currentHostStrIp = strdup(inet_ntoa(currentHostAddr));

    getsockopt(fd_listen, SOL_SOCKET, SO_RCVBUF, (void *) &socket_rcv_size, &st_rcv_len);
    getsockopt(fd_listen, SOL_SOCKET, SO_SNDBUF, (void *) &socket_snd_size, &st_snd_len);

    rcv_buffer = malloc(socket_rcv_size + 1);

    /* Config*/
    if (setsockopt(fd_listen, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)) < 0) {
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

    if (fd_listen > lfd) {
        lfd = fd_listen;
    }

    for (int i = 0; i < FD_SETSIZE; i++) {
        s[i].buffer = NULL;
        s[i].bytes = 0;
        s[i].chunks = 0;
    }

    listCreate(&list);

    sa.sa_handler = hdl;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    // Block SIGINT.
    sigset_t sigset, oldset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigprocmask(SIG_BLOCK, &sigset, &oldset);

    FD_ZERO(&set);
    FD_SET(fd_listen, &set);

    printf("::Waiting for connections on %s:%d::\n", currentHostStrIp, portNum);
    while (!quit_request) {
        read_fds = set;
        activity = pselect(lfd + 1, &read_fds, NULL, NULL, &timeout, &oldset);
        if (activity < 0 && (errno != EINTR)) {
            perror("select");
            continue;
        } else if (activity == 0) {
            for (int i = 0; i < FD_SETSIZE; i++) {
                if (s[i].buffer != NULL) {
                    fprintf(stdout, "The request timed out, connection on socket %d is about to be closed ...\n", i);
                    printf("::%ld bytes were transferred into %d different chunks on socket %d::\n",
                           s[i].bytes - 1,
                           s[i].chunks, i);
                    printf(COLOR"%s\n"RESET"\n", (char *) s[i].buffer);
                    shutdown(i, SHUT_RD);
                    handler(i, s[i].buffer);
                    shutdown(i, SHUT_WR);
                    FD_CLR(i, &set);
                    if (i == lfd) {
                        lfd--;
                    }
                    close(i);
                    free(s[i].buffer);
                    s[i].buffer = NULL;
                };
            }
            continue;
        }

        if (quit_request) {
            fprintf(stdout, "C[%d]: quiting ...""\n", getpid());
            break;
        }

        for (fd_active = 0; fd_active <= lfd; fd_active++) {
            if (FD_ISSET(fd_active, &read_fds)) {
                if (fd_active == fd_listen) {
                    if ((fd_new_client = accept(fd_active, new_client_in_addr_ptr, &client_len)) < 0) {
                        perror("accept");
                        break;
                    }
                    printf("::Accept new client (%s:%d) on socket %d::\n", inet_ntoa(new_client_in_addr.sin_addr),
                           ntohs(new_client_in_addr.sin_port),
                           fd_new_client);
                    FD_SET(fd_new_client, &set);
                    if (fd_new_client > lfd) {
                        lfd = fd_new_client;
                    }
                    if (fd_new_client <= FD_SETSIZE) {
                        s[fd_new_client].buffer = malloc(1);
                        s[fd_new_client].bytes = 1;
                        s[fd_new_client].chunks = 0;
                    } else {
                        send(fd_new_client, "HOST_IS_TOO_BUSY", 16, 0);
                        close(fd_new_client);
                    }
                } else {
                    bzero(rcv_buffer, socket_rcv_size);
                    bytes = recv(fd_active, rcv_buffer, socket_rcv_size, 0);
                    if (bytes == 0) {
                        printf("::%ld bytes were transferred into %d different chunks on socket %d::\n",
                               s[fd_active].bytes - 1,
                               s[fd_active].chunks, fd_active);
                        printf("::"COLOR"%s"RESET"::\n", (char *) s[fd_active].buffer);
                        shutdown(fd_active, SHUT_RD);
                        handler(fd_active, s[fd_active].buffer);
                        shutdown(fd_active, SHUT_WR);
                        FD_CLR(fd_active, &set);
                        if (fd_active == lfd) {
                            lfd--;
                        }
                        close(fd_active);
                        free(s[fd_active].buffer);
                        s[fd_active].buffer = NULL;
                    } else if (bytes > 0) {
                        size_t offset = s[fd_active].chunks ? s[fd_active].bytes - 1 : 0;
                        s[fd_active].buffer = realloc(s[fd_active].buffer, s[fd_active].bytes + bytes - 1);
                        memcpy(s[fd_active].buffer + offset, rcv_buffer, (size_t) bytes);
                        s[fd_active].bytes += bytes;
                        s[fd_active].chunks++;
                        //printf("::Receive %ld bytes from chunk %d on socket %d::\n", bytes, s[fd_active].chunks, fd_active);
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
    free(rcv_buffer);
    listDestroy(&list);
}
