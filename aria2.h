#ifndef ARIA2_H
#define ARIA2_H

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
    hash_map *map;
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

//free jsonrpc_response_object
void jsonrpc_response_object_free(jsonrpc_response_object *object);
/*
https://aria2.github.io/manual/en/html/aria2c.html#rpc-interface
*/
// new aria2 instance
aria2_object *aria2_new(const char *uri, const char *token);
// launch aria2
int aria2_launch(aria2_object *aria2);
// aria2.shutdown
void aria2_shutdown(aria2_object *aria2);
// aria2.forceShutdown
void aria2_force_shutdown(aria2_object *aria2);
// aria2.addUri
jsonrpc_response_object *aria2_add_uri(aria2_object *aria2, char **uris, int uris_len,
                                       key_value_pair options[], int options_len, int position);
// aria2.addTorrent
jsonrpc_response_object *aria2_add_torrent(aria2_object *aria2, unsigned char *torrent, int torrent_length,
                                           char **uris, int uris_len, key_value_pair options[], int options_len, int position);
// aria2.addMetalink
jsonrpc_response_object *aria2_add_metalink(aria2_object *aria2, unsigned char *torrent, int torrent_length,
                                            key_value_pair options[], int options_len, int position);
// aria2.getVersion
jsonrpc_response_object *aria2_get_version(aria2_object *aria2);
// aria2.remove
jsonrpc_response_object *aria2_remove(aria2_object *aria2, char *gid);
// aria2.forceRemove
jsonrpc_response_object *aria2_force_remove(aria2_object *aria2, char *gid);
// aria2.pause
jsonrpc_response_object *aria2_pause(aria2_object *aria2, char *gid);
// aria2.pauseAll
jsonrpc_response_object *aria2_pause_all(aria2_object *aria2);
// aria2.forcePause
jsonrpc_response_object *aria2_force_pause(aria2_object *aria2, char *gid);
// aria2.forcePauseAll
jsonrpc_response_object *aria2_force_pause_all(aria2_object *aria2);
// aria2.unpause
jsonrpc_response_object *aria2_unpause(aria2_object *aria2, char *gid);
// aria2.unpauseAll
jsonrpc_response_object *aria2_unpause_all(aria2_object *aria2);
// aria2.tellStatus
jsonrpc_response_object *aria2_tell_status(aria2_object *aria2, char *gid);
// aria2.getUris
jsonrpc_response_object *aria2_get_uris(aria2_object *aria2, char *gid);
// aria2.getFiles
jsonrpc_response_object *aria2_get_files(aria2_object *aria2, char *gid);
// aria2.getPeers
jsonrpc_response_object *aria2_get_peers(aria2_object *aria2, char *gid);
// aria2.getServers
jsonrpc_response_object *aria2_get_servers(aria2_object *aria2, char *gid);
// aria2.changePosition
jsonrpc_response_object *aria2_change_position(aria2_object *aria2, char *gid, int pos, change_position_how how);
// aria2.tellActive
jsonrpc_response_object *aria2_tell_active(aria2_object *aria2);
// aria2.tellWaiting
jsonrpc_response_object *aria2_tell_waiting(aria2_object *aria2, int offset, int num);
// aria2.tellStopped
jsonrpc_response_object *aria2_tell_stopped(aria2_object *aria2, int offset, int num);
// system.multicall[ { "aria2.tellStopped", 0, 1000 },{ "aria2.tellWaiting", 0, 1000 },{ "aria2.tellActive" }]
jsonrpc_response_object *aria2_tell_all(aria2_object *aria2);
// aria2.getOption
jsonrpc_response_object *aria2_get_option(aria2_object *aria2, char *gid);
// aria2.changeUri
jsonrpc_response_object *aria2_change_uri(aria2_object *aria2, char *gid, int fileindex,
                                          char **delUris, int delUris_len, char **addUris, int addUris_len, int position);
// aria2.changeOption
jsonrpc_response_object *aria2_change_option(aria2_object *aria2, char *gid, key_value_pair options[], int options_len);
// aria2.getGlobalOption
jsonrpc_response_object *aria2_get_global_option(aria2_object *aria2);
// aria2.changeGlobalOption
jsonrpc_response_object *aria2_change_global_option(aria2_object *aria2, key_value_pair options[], int options_len);
// aria2.purgeDownloadResult
jsonrpc_response_object *aria2_purge_download_result(aria2_object *aria2);
// aria2.removeDownloadResult
jsonrpc_response_object *aria2_remove_download_result(aria2_object *aria2, char *gid);
// aria2.getSessionInfo
jsonrpc_response_object *aria2_get_session_info(aria2_object *aria2);
// aria2.getGlobalStat
jsonrpc_response_object *aria2_get_global_stat(aria2_object *aria2);
// aria2.saveSession
jsonrpc_response_object *aria2_save_session(aria2_object *aria2);
#endif