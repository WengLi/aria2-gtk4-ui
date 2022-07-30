#ifndef ARIA2_ENTITY_H
#define ARIA2_ENTITY_H

#include <stdbool.h>
#include <windows.h>
#include <shellapi.h>
#include "websocketclient.h"
#include "cJSON.h"
#include "uuid4.h"
#include "utilities.h"
#include "hashmap.h"

typedef struct hashmap_s hash_map;

/*
code	            message	                    meaning
-32700	            Parse error	                Invalid JSON was received by the server.
                                                An error occurred on the server while parsing the JSON text.
-32600	            Invalid Request	            The JSON sent is not a valid Request object.
-32601	            Method not found	        The method does not exist / is not available.
-32602	            Invalid params	            Invalid method parameter(s).
-32603	            Internal error	            Internal JSON-RPC error.
-32000 to -32099	Server error	Reserved for implementation-defined server-errors.*/
typedef struct _jsonrpc_error_object
{
    /*A Number that indicates the error type that occurred.
    This MUST be an integer.*/
    int code;
    /*A String providing a short description of the error.
    The message SHOULD be limited to a concise single sentence.*/
    char *message;
    /*A Primitive or Structured value that contains additional information about the error.
    This may be omitted.
    The value of this member is defined by the Server (e.g. detailed error information, nested errors etc.).*/
    cJSON *data;
} jsonrpc_error_object;

// https://www.jsonrpc.org/specification
typedef struct _jsonrpc_request_object
{
    // A String specifying the version of the JSON-RPC protocol. MUST be exactly "2.0".
    // char *jsonrpc;
    // An identifier established by the Client that MUST contain a String, Number, or NULL value if included.
    // If it is not included it is assumed to be a notification.
    cJSON *id;
    // A String containing the name of the method to be invoked.
    // Method names that begin with the word rpc followed by a period character (U+002E or ASCII 46) are reserved for rpc-internal methods
    // and extensions and MUST NOT be used for anything else.
    char *method;
    // A Structured value that holds the parameter values to be used during the invocation of the method. This member MAY be omitted.
    cJSON *params;
    // true:set params,false:no set params,some method no need params;
    int need_params;
    // true:set token,false:no set token,some method no need token;
    int need_token;
} jsonrpc_request_object;

typedef struct _jsonrpc_response_object
{
    // A String specifying the version of the JSON-RPC protocol. MUST be exactly "2.0".
    // char *jsonrpc;
    /*This member is REQUIRED.
    It MUST be the same as the value of the id member in the Request Object.
    If there was an error in detecting the id in the Request object (e.g. Parse error/Invalid Request), it MUST be Null.*/
    cJSON *id;
    /*This member is REQUIRED on success.
    This member MUST NOT exist if there was an error invoking the method.
    The value of this member is determined by the method invoked on the Server.*/
    cJSON *result;
    /*This member is REQUIRED on error.
    This member MUST NOT exist if there was no error triggered during invocation.
    The value for this member MUST be an Object as defined in section 5.1.*/
    jsonrpc_error_object *error;
} jsonrpc_response_object;

// if(error_code>0) error happened
typedef struct jsonrpc_response_result
{
    bool successed;
    int error_code;
    char *error_message;
    void *data_ptr;
    int64_t data_int64;
} jsonrpc_response_result;

typedef struct ptr_array
{
    void **ptr;
    int buffer_size;
    int size;
} ptr_array;

typedef struct
{
    char *version;
    // string[]
    char **enabledFeatures;
    int enabledFeatures_size;
    ptr_array ptr_need_free;
} aria2_version;

typedef struct _aria2_object
{
    websocket_client *client;
    pthread_t client_thread;
    pthread_mutex_t lock;
    char *uri;
    char *token;
    unsigned int polling_interval;
    int polling_try_count;
    uint8_t init_completed : 1;
    uint8_t paused : 1;
    uint8_t closed : 1;
    hash_map *map;
    aria2_version *version;
    char* init_error_message;
} aria2_object;

typedef struct _key_value_pair
{
    char *key;
    cJSON *value;
} key_value_pair;

typedef enum _change_position_how
{
    POS_SET,
    POS_CUR,
    POS_END
} change_position_how;

typedef struct
{
    char *uri;
    char *status;
} aria2_item_file_uri;

typedef struct
{
    aria2_item_file_uri **uris;
    int uris_size;
    ptr_array ptr_need_free;
} aria2_item_file_uris;

typedef struct
{
    int index;
    char *path;
    int64_t length;
    int64_t completedLength;
    bool selected;
    aria2_item_file_uri **uris;
    int uris_size;
} aria2_item_file;

typedef struct
{
    aria2_item_file **files;
    int files_size;
    ptr_array ptr_need_free;
} aria2_item_files;

typedef struct
{
    char *name;
    char *name_utf8;
} aria2_item_bittorrent_info;

typedef struct
{
    // string[][]
    char ***announceList;
    int announceList_row_size;
    int *announceList_col_size; //列数可能不一致的情况，用数组保持每行列数
    char *comment;
    int64_t creationDate;
    char *mode;
    aria2_item_bittorrent_info *info;
} aria2_item_bittorrent;

typedef struct _aria2_item
{
    char *gid;
    char *status;
    int64_t totalLength;
    int64_t completedLength;
    int64_t uploadLength;
    char *bitfield;
    int64_t downloadSpeed;
    int64_t uploadSpeed;
    char *infoHash;
    int64_t numSeeders;
    bool seeder;
    int64_t pieceLength;
    int64_t numPieces;
    int64_t connections;
    char *errorCode;
    char *errorMessage;
    char **followedBy;
    int followedBy_size;
    char *following;
    char *belongsTo;
    char *dir;
    aria2_item_file **files;
    int files_size;
    aria2_item_bittorrent *bittorrent;
    int64_t verifiedLength;
    bool verifyIntegrityPending;
    ptr_array ptr_need_free;
} aria2_item;

typedef struct
{
    aria2_item **items;
    int items_size;
} aria2_items;

typedef struct aria2_item_all
{
    aria2_items *item_active;

    aria2_items *item_waiting;

    aria2_items *item_stopped;
} aria2_item_all;


typedef struct
{
    char *peerId;
    char *ip;
    int64_t port;
    char *bitfield;
    bool amChoking;
    bool peerChoking;
    int64_t downloadSpeed;
    int64_t uploadSpeed;
    bool seeder;
} aria2_peer;

typedef struct
{
    aria2_peer **peers;
    int peers_size;
    ptr_array ptr_need_free;
} aria2_peers;

typedef struct
{
    char *uri;
    char *currentUri;
    int64_t downloadSpeed;
} aria2_server;

typedef struct
{
    int64_t index;
    aria2_server **servers;
    int servers_size;
    ptr_array ptr_need_free;
} aria2_servers;

typedef struct
{
    char *name;
    char *value;
} aria2_option;

typedef struct
{
    aria2_option **options;
    int options_size;
    ptr_array ptr_need_free;
} aria2_options;

typedef struct
{
    int64_t *values;
    int values_size;
    ptr_array ptr_need_free;
} aria2_int64_values;

typedef struct aria2_globalstat
{
    int64_t downloadSpeed;
    int64_t uploadSpeed;
    int64_t numActive;
    int64_t numWaiting;
    int64_t numStopped;
    int64_t numStoppedTotal;
} aria2_globalstat;

jsonrpc_request_object *jsonrpc_request_object_create(char *method, cJSON *params, int need_params, int need_token);
jsonrpc_response_object *jsonrpc_response_object_create(cJSON *id, cJSON *result, jsonrpc_error_object *error);
jsonrpc_response_result *jsonrpc_response_result_create(jsonrpc_response_object *response);
void jsonrpc_response_object_free(jsonrpc_response_object *object);
jsonrpc_error_object *jsonrpc_error_object_create(int code, char *message, cJSON *data);

void ptr_array_add(ptr_array ptr_need_free, void *ptr);
// string
char *cjson_to_string(cJSON *c, ptr_array ptr_need_free);
// string[]
char **cjson_to_string_array(cJSON *c, int *out_size, ptr_array ptr_need_free);
// string[][]
char ***cjson_to_string_2d_array(cJSON *c, int *row_size, int **col_size, ptr_array ptr_need_free);
int64_t cjson_to_int64(cJSON *c);
// int64[]
int64_t *cjson_to_int64_array(cJSON *c, int *out_size, ptr_array ptr_need_free);
bool cjson_to_bool(cJSON *c);
aria2_item_file_uri *cjson_to_aria2_item_file_uri(cJSON *c, ptr_array ptr_need_free);
aria2_item_file_uri **cjson_to_aria2_item_file_uris(cJSON *c, int *out_size, ptr_array ptr_need_free);
aria2_item_file *cjson_to_aria2_item_file(cJSON *c, ptr_array ptr_need_free);
aria2_item_file **cjson_to_aria2_item_files(cJSON *c, int *out_size, ptr_array ptr_need_free);
aria2_item_bittorrent_info *cjson_to_aria2_item_bittorrent_info(cJSON *c, ptr_array ptr_need_free);
aria2_item_bittorrent *cjson_to_aria2_item_bittorrent(cJSON *c, ptr_array ptr_need_free);
aria2_item *cjson_to_aria2_item(cJSON *c);
aria2_item **cjson_to_aria2_item_array(cJSON *c, int *out_size);
aria2_items *cjson_to_aria2_items(cJSON *c);
aria2_peer *cjson_to_aria2_peer(cJSON *c, ptr_array ptr_need_free);
aria2_peer **cjson_to_aria2_peers(cJSON *c, int *out_size, ptr_array ptr_need_free);
aria2_server *cjson_to_aria2_server(cJSON *c, ptr_array ptr_need_free);
aria2_server **cjson_to_aria2_servers(cJSON *c, int *out_size, ptr_array ptr_need_free);
aria2_option *cjson_to_aria2_option(cJSON *c, ptr_array ptr_need_free);
aria2_option **cjson_to_aria2_options(cJSON *c, int *out_size, ptr_array ptr_need_free);
aria2_globalstat *cjson_to_aria2_globalstat(cJSON *c);
void ptr_array_free(ptr_array arr);
void aria2_item_array_free(aria2_item **items, int row_size);
void aria2_item_all_free(aria2_item_all *c);
void aria2_item_free(aria2_item *item);
#endif