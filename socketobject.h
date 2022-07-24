#ifndef SOCKET_OBJECT_H
#define SOCKET_OBJECT_H

#if defined _WIN32 || defined _WIN64
#include <Ws2tcpip.h>
#else
// unix system
#endif

void socket_object_init();
void socket_object_final();
SOCKET socket_object_connect(const char *host, const char *port);
int socket_object_send(SOCKET s, const char *buf, int len);
int socket_object_recv(SOCKET s, char *buf, int len);
int socket_object_write(SOCKET s, const char *data, int length);
char *socket_object_read(SOCKET s, int *length);
int socket_object_set_blocking(SOCKET fd, int is_blocking);
int socket_object_close(SOCKET s);

#endif