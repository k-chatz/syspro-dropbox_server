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
    fd_set set, rset;

    /* Read argument options from command line*/
    readOptions(argc, argv, &portNum);

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
        rset = set;
        activity = select(fd_hwm + 1, &rset, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            perror("select");
        }
        for (fd = 0; fd <= fd_hwm; fd++) {
            if (FD_ISSET(fd, &rset)) {
                if (fd == fd_server) {
                    if ((fd_client = accept(fd_server, client_ptr, &client_len)) < 0) {
                        perror("accept");
                        exit(EXIT_FAILURE);
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
                        printf("Receive %ld bytes from %d: %s\n", bytes, fd, buffer);
                        for (int i = 0; i <= 100000; i++) {
                            printf(".\n");
                        }
                        send(fd, buffer, strlen(buffer), 0);
                    }
                }
            }
        }
    }


    printf("portNum: %d\n", portNum);

    return 0;
}