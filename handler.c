#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <zconf.h>
#include <arpa/inet.h>
#include "handler.h"
#include "client.h"
#include "list.h"
#include "connection.h"

/**
 * Handle requests.*/
void handler(int client_fd, void *buffer) {
    bool found = false;
    Client c = NULL, client = NULL;
    unsigned int clients = 0;
    int fd_client = 0;

    if (strncmp(buffer, "LOG_ON", 6) == 0) {
        printf("REQUEST: LOG_ON ");
        c = malloc(sizeof(struct client));
        if (c == NULL) {
            perror("malloc");
        }
        memcpy(c, buffer + 6, sizeof(struct client));
        printClientTuple(c);
        listSetCurrentToStart(list);
        while ((client = listNext(list)) != NULL) {
            if (c->ip == client->ip && c->port == client->port) {
                found = true;
            }
        }
        if (!found) {
            if (listInsert(list, c)) {
                listSetCurrentToStart(list);
                while ((client = listNext(list)) != NULL) {
                    if (!(c->ip == client->ip && c->port == client->port)) {
                        if ((fd_client = openConnection(client->ip, client->port)) > 0) {
                            send(fd_client, "USER_ON", 7, 0);
                            fprintf(stdout, "USER_ON ");
                            send(fd_client, c, sizeof(struct client), 0);
                            printClientTuple(c);
                            fprintf(stdout, "\n");
                            close(fd_client);
                        }
                    }
                }
                send(client_fd, "LOG_ON_SUCCESS", 14, 0);
                fprintf(stdout, "LOG_ON_SUCCESS\n");
            } else {
                send(client_fd, "ERROR_LOG_ON", 12, 0);
                fprintf(stderr, "ERROR_LOG_ON\n");
                free(c);
            }
        } else {
            send(client_fd, "ALREADY_LOGGED_IN", 17, 0);
            fprintf(stderr, "ALREADY_LOGGED_IN\n");
            free(c);
        }
        fprintf(stdout, "\n");
    } else if (strncmp(buffer, "GET_CLIENTS", 11) == 0) {
        printf("REQUEST: GET_CLIENTS ");
        c = malloc(sizeof(struct client));
        memcpy(c, buffer + 11, sizeof(struct client));
        printClientTuple(c);
        clients = listGetLength(list) - 1;
        send(client_fd, "CLIENT_LIST", 11, 0);
        fprintf(stdout, "CLIENT_LIST ");
        send(client_fd, &clients, sizeof(unsigned int), 0);
        fprintf(stdout, "%d ", clients);
        listSetCurrentToStart(list);
        while ((client = listNext(list)) != NULL) {
            if (!(c->ip == client->ip && c->port == client->port)) {
                send(client_fd, client, sizeof(struct client), 0);
                struct sockaddr_in addr;
                addr.sin_family = AF_INET;
                addr.sin_addr.s_addr = client->ip;
                addr.sin_port = client->port;
                fprintf(stdout, "<%s, %d> ", inet_ntoa(addr.sin_addr), ntohs(client->port));
            }
        }
        fprintf(stdout, "\n");
        free(c);
    } else if (strncmp(buffer, "LOG_OFF", 7) == 0) {
        printf("REQUEST: LOG_OFF\n");
        c = malloc(sizeof(struct client));
        memcpy(c, buffer + 7, sizeof(struct client));
        printClientTuple(c);
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
                        if ((fd_client = openConnection(client->ip, client->port)) > 0) {
                            send(fd_client, "USER_OFF", 8, 0);
                            fprintf(stdout, "USER_OFF ");
                            send(fd_client, c, sizeof(struct client), 0);
                            struct sockaddr_in addr;
                            addr.sin_family = AF_INET;
                            addr.sin_addr.s_addr = client->ip;
                            addr.sin_port = client->port;
                            fprintf(stdout, "<%s, %d>\n", inet_ntoa(addr.sin_addr), ntohs(c->port));
                            close(fd_client);
                        }
                    }
                }
                send(fd_client, "LOG_OFF_SUCCESS", 15, 0);
                fprintf(stdout, "LOG_OFF_SUCCESS\n");
            } else {
                send(client_fd, "ERROR_NOT_REMOVED", 17, 0);
                fprintf(stderr, "ERROR_NOT_REMOVED\n");
            }
        } else {
            send(client_fd, "ERROR_IP_PORT_NOT_FOUND_IN_LIST", 31, 0);
            fprintf(stderr, "ERROR_IP_PORT_NOT_FOUND_IN_LIST\n");
        }
        free(c);
    } else {
        send(client_fd, "UNKNOWN_COMMAND", 15, 0);
        fprintf(stderr, "UNKNOWN_COMMAND\n");
    }
}
