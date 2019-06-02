#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include "connection.h"

/**
 * Open TCP connection.*/
int openConnection(in_addr_t ip, in_port_t port) {
    struct sockaddr_in in_addr;
    struct sockaddr *in_addr_ptr = NULL;
    int fd = 0;

    in_addr_ptr = (struct sockaddr *) &in_addr;
    memset(in_addr_ptr, 0, sizeof(struct sockaddr));

    /* Create socket */
    if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("socket");
        return 0;
    }

    in_addr.sin_family = AF_INET;
    in_addr.sin_addr.s_addr = ip;
    in_addr.sin_port = port;

    printf("::Connecting socket %d to remote host %s:%d::\n", fd, inet_ntoa(in_addr.sin_addr), ntohs(in_addr.sin_port));

    /* Initiate connection */
    if (connect(fd, in_addr_ptr, sizeof(struct sockaddr)) < 0) {
        perror("connect");
        return 0;
    }
    printf("::Connection to remote host %s:%d established::\n", inet_ntoa(in_addr.sin_addr),
           ntohs(in_addr.sin_port));
    return fd;
}
