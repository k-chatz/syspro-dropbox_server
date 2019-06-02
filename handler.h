#ifndef HANDLER_H
#define HANDLER_H

#include "list.h"

extern List list;

void handler(int client_fd, void *buffer);

#endif
