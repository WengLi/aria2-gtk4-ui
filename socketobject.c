#include "socketobject.h"

#if defined _WIN32 || defined _WIN64
static int global_ref_count = 0;
#endif

void socket_object_init()
{
#if defined _WIN32 || defined _WIN64
	WSADATA WSAData;
	if (global_ref_count == 0)
		WSAStartup(MAKEWORD(2, 2), &WSAData);
	global_ref_count++;
#endif // _WIN32 || _WIN64
}

void socket_object_final()
{
#if defined _WIN32 || defined _WIN64
	global_ref_count--;
	if (global_ref_count == 0)
		WSACleanup();
#endif
}

// IPv4 or IPv6
// return INVALID_SOCKET if error occurred.
SOCKET socket_object_connect(const char *host, const char *port)
{
	SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == INVALID_SOCKET)
	{
		return INVALID_SOCKET;
	}

	struct addrinfo hints;
	struct addrinfo *result;
	struct addrinfo *cur;
	socklen_t len;
	int type;
	len = sizeof(type);

	if (getsockopt(s, SOL_SOCKET, SO_TYPE, (char *)&type, &len) == -1)
	{
		return INVALID_SOCKET;
	}
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = type;
	if (getaddrinfo(host, port, &hints, &result) != 0)
	{
		return INVALID_SOCKET;
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
		return INVALID_SOCKET;
	}
	return s;
}

// socket send
int socket_object_send(SOCKET s, const char *buf, int len)
{
	int n = send(s, buf, len, 0);
	return n;
}

// socket recv
int socket_object_recv(SOCKET s, char *buf, int len)
{
	int n = recv(s, buf, len, 0);
	return n;
}

//返回发送字符串长度，-1 无数据发送,否则返回已发送数据大小
int socket_object_write(SOCKET s, const char *data, int length)
{
	int sent = 0, i = 0;
	while (sent < length && i >= 0)
	{
		i = socket_object_send(s, data + sent, length - sent);
		sent += i;
	}
	if (i < 0)
	{
		return -1;
	}
	return sent;
}

//返回接收字符串，NULL 无数据
char *socket_object_read(SOCKET s, int *out_length)
{
	int length = 0;
	int buffer_size = 1024;
	char buff[buffer_size];
	int allocated = 129;
	char *at = (char *)malloc(allocated * sizeof(char));
	int receive_size = 0;
	do
	{
		length = socket_object_recv(s, buff, buffer_size);
		if (length == -1)
		{
			return NULL;
		}
		for (int i = 0; i < length; i++)
		{
			if (allocated == receive_size)
			{
				allocated *= 2;
				at = realloc(at, allocated * sizeof(char));
			}
			at[receive_size++] = buff[i];
		}

	} while (length == buffer_size);
	if (out_length)
	{
		*out_length = receive_size;
	}
	return at;
}

//设置socket阻塞模式:FALSE不阻塞；TRUE阻塞
int socket_object_set_blocking(SOCKET fd, int is_blocking)
{
#if defined _WIN32 || defined _WIN64
	u_long is_non_blocking = (is_blocking) ? FALSE : TRUE;
	if (ioctlsocket(fd, FIONBIO, &is_non_blocking) == SOCKET_ERROR)
	{
		return FALSE;
	}
#else
// unix system
#endif

	return TRUE;
}

int socket_object_close(SOCKET s)
{
#if defined _WIN32 || defined _WIN64
	shutdown(s, 0);
#endif
	return closesocket(s);
}