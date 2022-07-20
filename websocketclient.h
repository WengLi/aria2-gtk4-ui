#ifndef WEB_SOCKET_CLIENT_H
#define WEB_SOCKET_CLIENT_H

// #define HAVE_LIBSSL

#include <stdint.h>
#include <pthread.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "sha1.h"
#include "base64.h"
#include "utilities.h"
#include "socketobject.h"

#ifdef HAVE_LIBSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#endif

#define FRAME_CHUNK_LENGTH 1024
#define HELPER_RECV_BUF_SIZE 1024

#define CLIENT_IS_SSL (1 << 0)
#define CLIENT_CONNECTING (1 << 1)
#define CLIENT_SHOULD_CLOSE (1 << 2)
#define CLIENT_SENT_CLOSE_FRAME (1 << 3)

#define REQUEST_HAS_CONNECTION (1 << 0)
#define REQUEST_HAS_UPGRADE (1 << 1)
#define REQUEST_VALID_STATUS (1 << 2)
#define REQUEST_VALID_ACCEPT (1 << 3)

#define WS_FRAGMENT_START (1 << 0)
#define WS_FRAGMENT_FIN (1 << 7)

#define WS_FLAGS_SSL_INIT (1 << 0)

#define WS_EXIT_MALLOC -1
#define WS_EXIT_PTHREAD_MUTEX_INIT -2
#define WS_EXIT_PTHREAD_CREATE -3
#define WS_EXIT_BAD_SCHEME -4

#define WS_OPEN_CONNECTION_ADDRINFO_ERR -1
#define WS_OPEN_CONNECTION_ADDRINFO_EXHAUSTED_ERR -2
#define WS_RUN_THREAD_RECV_ERR -3
#define WS_DO_CLOSE_SEND_ERR -4
#define WS_HANDLE_CTL_FRAME_SEND_ERR -5
#define WS_COMPLETE_FRAME_MASKED_ERR -6
#define WS_DISPATCH_MESSAGE_NULL_PTR_ERR -7
#define WS_SEND_AFTER_CLOSE_FRAME_ERR -8
#define WS_SEND_DURING_CONNECT_ERR -9
#define WS_SEND_NULL_DATA_ERR -10
#define WS_SEND_DATA_TOO_LARGE_ERR -11
#define WS_SEND_SEND_ERR -12
#define WS_HANDSHAKE_REMOTE_CLOSED_ERR -13
#define WS_HANDSHAKE_RECV_ERR -14
#define WS_HANDSHAKE_BAD_STATUS_ERR -15
#define WS_HANDSHAKE_NO_UPGRADE_ERR -16
#define WS_HANDSHAKE_NO_CONNECTION_ERR -17
#define WS_HANDSHAKE_BAD_ACCEPT_ERR -18
#define WS_HELPER_ALREADY_BOUND_ERR -19
#define WS_HELPER_CREATE_SOCK_ERR -20
#define WS_HELPER_BIND_ERR -21
#define WS_HELPER_LISTEN_ERR -22

typedef struct _websocket_client_frame
{
    unsigned int fin;
    unsigned int opcode;
    unsigned int mask_offset;
    unsigned int payload_offset;
    unsigned int rawdata_idx;
    unsigned int rawdata_sz;
    unsigned long long payload_len;
    char *rawdata;
    struct _websocket_client_frame *next_frame;
    struct _websocket_client_frame *prev_frame;
    unsigned char mask[4];
} websocket_client_frame;

typedef struct _websocket_client_message
{
    unsigned int opcode;
    unsigned long long payload_len;
    char *payload;
} websocket_client_message;

typedef struct _websocket_client_error
{
    int code;
    int extra_code;
    char *str;
} websocket_client_error;

typedef struct _websocket_client
{
    pthread_t handshake_thread;
    pthread_t run_thread;
    pthread_mutex_t lock;
    pthread_mutex_t send_lock;
    char *URI;
    char *host;
    char *port;
    char *path;
    SOCKET socket;
    int flags;
    int (*onopen)(struct _websocket_client *);
    int (*onclose)(struct _websocket_client *);
    int (*onerror)(struct _websocket_client *, websocket_client_error *err);
    int (*onmessage)(struct _websocket_client *, websocket_client_message *msg);
    websocket_client_frame *current_frame;
#ifdef HAVE_LIBSSL
	SSL_CTX *ssl_ctx;
	SSL *ssl;
#endif
} websocket_client;

//Function defs
websocket_client *websocket_client_new(const char *URI);
websocket_client_error *websocket_client_error_new(int errcode);
int _websocket_client_read(websocket_client *c, void *buf, int length);
int _websocket_client_write(websocket_client *c, const void *buf, int length);
int websocket_client_connect(const char *host, const char *port);
int websocket_client_send(websocket_client *client, char *strdata);
int websocket_client_complete_frame(websocket_client *c, websocket_client_frame *frame);
void websocket_client_handle_control_frame(websocket_client *c, websocket_client_frame *ctl_frame);
void websocket_client_run(websocket_client *c);
void websocket_client_finish(websocket_client *client);
void *websocket_client_run_thread(void *ptr);
void *websocket_client_handshake_thread(void *ptr);
void websocket_client_cleanup_frames(websocket_client_frame *first);
void websocket_client_in_data(websocket_client *c, char in);
void websocket_client_dispatch_message(websocket_client *c, websocket_client_frame *current);
void websocket_client_close(websocket_client *client);
void websocket_client_onclose(websocket_client *client, int (*cb)(websocket_client *c));
void websocket_client_onopen(websocket_client *client, int (*cb)(websocket_client *c));
void websocket_client_onmessage(websocket_client *client, int (*cb)(websocket_client *c, websocket_client_message *msg));
void websocket_client_onerror(websocket_client *client, int (*cb)(websocket_client *c, websocket_client_error *err));

#endif