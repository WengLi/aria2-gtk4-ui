#include "socketobject.h"

// IPv4 or IPv6
// return SOCKET_OBJECT_ERROR(-1) if error occurred.
int socket_object_connect(const char *host, const char *port)
{
	SOCKET s;
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET)
    {
        return SOCKET_OBJECT_ERROR;
    }

	struct addrinfo hints;
	struct addrinfo *result;
	struct addrinfo *cur;
	socklen_t len;
	int type;
	len = sizeof(type);

	if (getsockopt(s, SOL_SOCKET, SO_TYPE, (char *)&type, &len) == -1)
	{
		return SOCKET_OBJECT_ERROR;
	}
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = type;
	if (getaddrinfo(host, port, &hints, &result) != 0)
	{
		return SOCKET_OBJECT_ERROR;
	}
	for (cur = result; cur; cur = cur->ai_next)
	{
		if (connect(s, cur->ai_addr, cur->ai_addrlen) == 0)
		{
			break;
		}
	}
	freeaddrinfo(result);
	if (cur == NULL)
	{
		return SOCKET_OBJECT_ERROR;
	}
	return s;
}

//设置socket阻塞模式:FALSE不阻塞；TRUE阻塞
int  socket_object_set_blocking (SOCKET fd, int is_blocking)
{
#if defined _WIN32 || defined _WIN64
	u_long  is_non_blocking = (is_blocking) ? FALSE : TRUE;
	if (ioctlsocket (fd, FIONBIO, &is_non_blocking) == SOCKET_ERROR)
		return FALSE;
#else
#endif

	return TRUE;
}

int socket_object_close(SOCKET s)
{
	shutdown(s, 0);
	return closesocket(s);
}