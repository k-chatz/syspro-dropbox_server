#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <zconf.h>
#include <arpa/inet.h>

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

int main(int argc, char *argv[]) {
    int opt = 1, fd_server = 0, fd_client = 0, activity = 0, fd_hwm = 0, fd = 0;
    struct sockaddr_in server, client;
    struct sockaddr *server_ptr = (struct sockaddr *) &server;
    struct sockaddr *client_ptr = (struct sockaddr *) &client;
    socklen_t client_len;
    uint16_t portNum = 0;
    char buffer[1025];
    ssize_t bytes = 0;
    fd_set set, read_fds;

    /* Read argument options from command line*/
    readOptions(argc, argv, &portNum);

    memset(server_ptr, 0, sizeof(struct sockaddr));
    memset(client_ptr, 0, sizeof(struct sockaddr));

    client.sin_family = AF_INET;
    client.sin_addr.s_addr = INADDR_ANY;

    /* Initialize address*/
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(portNum);

    /* Create socket*/
    if ((fd_server = socket(AF_INET, SOCK_STREAM, 0)) <= 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

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

    printf("Listener on port %d \n", portNum);

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

    puts("Waiting for connections ...");
    for (;;) {
        read_fds = set;
        activity = select(fd_hwm + 1, &read_fds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            perror("select");
        }
        for (fd = 0; fd <= fd_hwm; fd++) {
            if (FD_ISSET(fd, &read_fds)) {
                if (fd == fd_server) {
                    if ((fd_client = accept(fd_server, client_ptr, &client_len)) < 0) {
                        perror("accept");
                        printf("New client [%d] %s:%d\n",
                               fd_client, inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                        break;
                    }
                    printf("New client [%d] %s:%d\n",
                           fd_client, inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                    FD_SET(fd_client, &set);
                    if (fd_client > fd_hwm) {
                        fd_hwm = fd_client;
                    }
                } else {
                    if ((bytes = read(fd, buffer, sizeof(buffer))) == 0) {
                        FD_CLR(fd, &set);
                        if (fd == fd_hwm) {
                            fd_hwm--;
                        }
                        close(fd);
                    } else {
                        buffer[bytes] = '\0';
                        printf("Receive %ld bytes from %d:\n%s\n", bytes, fd, buffer);
                        send(fd, "OK", strlen("OK"), 0);
                    }
                }
            }
        }
    }
}
