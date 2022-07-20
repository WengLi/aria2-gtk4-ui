#ifndef SOCKET_OBJECT_H
#define SOCKET_OBJECT_H

#include <string.h>

#if defined _WIN32 || defined _WIN64
#include <Ws2tcpip.h>
#else
#endif

#define SOCKET_OBJECT_ERROR (-1)

int socket_object_connect(const char *host, const char *port);
int  socket_object_set_blocking (SOCKET fd, int is_blocking);
int socket_object_close(SOCKET s);

#endif