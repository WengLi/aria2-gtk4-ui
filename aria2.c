#include "aria2.h"

// websocket常驻线程
static void *websocket_client_launch_thread(void *ptr);

jsonrpc_request_object *jsonrpc_request_object_create(char *method, cJSON *params, int need_params, int need_token)
{
    char buf[UUID4_LEN];
    uuid4_init();
    uuid4_generate(buf);
    jsonrpc_request_object *object = (jsonrpc_request_object *)malloc(sizeof(jsonrpc_request_object));
    memset(object, 0, sizeof(jsonrpc_request_object));
    object->id = cJSON_CreateString(buf);
    object->method = method;
    object->params = params;
    object->need_params = need_params;
    object->need_token = need_token;
    return object;
}

jsonrpc_response_object *jsonrpc_response_object_create(cJSON *id, cJSON *result, jsonrpc_error_object *error)
{
    jsonrpc_response_object *object = (jsonrpc_response_object *)malloc(sizeof(jsonrpc_response_object));
    memset(object, 0, sizeof(jsonrpc_response_object));
    object->id = id;
    object->result = result;
    object->error = error;
    return object;
}

void jsonrpc_response_object_free(jsonrpc_response_object *object)
{
    if (!object)
    {
        return;
    }
    if (object->error)
    {
        free(object->error->message);
        cJSON_Delete(object->error->data);
        free(object->error);
    }
    cJSON_Delete(object->id);
    cJSON_Delete(object->result);
    free(object);
}

jsonrpc_error_object *jsonrpc_error_object_create(int code, char *message, cJSON *data)
{
    jsonrpc_error_object *object = (jsonrpc_error_object *)malloc(sizeof(jsonrpc_error_object));
    memset(object, 0, sizeof(jsonrpc_error_object));
    object->code = code;
    object->message = message;
    object->data = data;
    return object;
}

jsonrpc_response_object *aria2_request(aria2_object *aria2, jsonrpc_request_object *request)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "id", request->id);
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddStringToObject(root, "method", request->method);
    if (request->need_params)
    {
        if (request->need_token && aria2->token)
        {
            cJSON_InsertItemInArray(request->params, 0, cJSON_CreateString(string_format("token:%s", aria2->token)));
        }
        cJSON_AddItemToObject(root, "params", request->params);
    }

    const char *id = cJSON_GetObjectItem(root, "id")->valuestring;
    hashmap_put(aria2->map, id, strlen(id), NULL);

    char *json = cJSON_Print(root);
    fprintf(stderr, "send request: %s\n", json);
    websocket_client_send(aria2->client, json);

    jsonrpc_response_object *object = NULL;
    int try = aria2->polling_try_count;
    while (object == NULL && try > 0)
    {
        object = hashmap_get(aria2->map, id, strlen(id));
        if (!object)
        {
            Sleep(aria2->polling_interval);
            try--;
            continue;
        }
    }
    cJSON_Delete(root);
    hashmap_remove(aria2->map, id, strlen(id));
    free(request);
    return object;
}

aria2_object *aria2_new(const char *uri, const char *token)
{
    aria2_object *aria2 = (aria2_object *)malloc(sizeof(aria2_object));
    memset(aria2, 0, sizeof(aria2_object));
    aria2->uri = uri;
    aria2->token = token;
    aria2->polling_interval = 500;
    aria2->polling_try_count = 5;
    aria2->init_completed = FALSE;
    aria2->paused = TRUE;
    const unsigned initial_size = 4;
    aria2->map = (hash_map *)malloc(sizeof(hash_map));
    hashmap_create(initial_size, aria2->map);
    return aria2;
}

//命令执行成功返回0，执行失败返回-1。
int aria2_launch(aria2_object *aria2)
{

    char *path = "aria2c";
    char *args = "--conf-path=aria2.conf -D";
    uint16_t *path_utf16 = utf8_to_utf16(path, -1, NULL);
    uint16_t *args_utf16 = utf8_to_utf16(args, -1, NULL);
    //先启动aria2
    // int result = (int)ShellExecuteW(NULL, L"open", path_utf16, args_utf16, NULL, SW_SHOW); // SW_SHOW,SW_HIDE;
    // if (result > 32)
    // {
    //     fprintf(stderr, "start aria2 success: %d\n", result);
    // }
    // else
    // {
    //     fprintf(stderr, "start aria2 failed: %d\n", result);
    // }
    //在启动websocketclient
    pthread_create(&aria2->client_thread, NULL, websocket_client_launch_thread, (void *)aria2);
    return TRUE;
}

int client_onclose(websocket_client *c)
{
    fprintf(stderr, "onclose called: %d\n", (int)c->socket);
    return 0;
}

int client_onerror(websocket_client *c, websocket_client_error *err)
{
    fprintf(stderr, "onerror: (%d): %s\n", err->code, err->str);
    if (err->extra_code)
    {
        errno = err->extra_code;
        perror("recv");
    }
    return 0;
}

int client_onmessage(websocket_client *c, websocket_client_message *msg)
{
    aria2_object *aria2 = (aria2_object *)c->onmessage_in_data;
    cJSON *root = cJSON_Parse(msg->payload);
    cJSON *id = cJSON_GetObjectItem(root, "id");
    if (id == NULL)
    {
        return 0;
    }
    const char *id_str = strdup(cJSON_GetStringValue(id));
    cJSON *result = cJSON_GetObjectItem(root, "result");
    const char *result_str = cJSON_Print(result);
    cJSON *error = cJSON_GetObjectItem(root, "error");
    if (1 == hashmap_contains_key(aria2->map, id_str, strlen(id_str)))
    {
        jsonrpc_error_object *er = NULL;

        if (error)
        {
            cJSON *error_code = cJSON_GetObjectItem(error, "code");
            cJSON *error_msg = cJSON_GetObjectItem(error, "message");
            cJSON *error_data = cJSON_GetObjectItem(error, "data");
            const char *error_data_str = cJSON_Print(error_data);
            er = jsonrpc_error_object_create(error_code->valueint, strdup(error_msg->valuestring), cJSON_CreateString(error_data_str));
        }

        jsonrpc_response_object *response_object =
            jsonrpc_response_object_create(cJSON_CreateString(id_str), cJSON_Parse(result_str), er);
        hashmap_put(aria2->map, id_str, strlen(id_str), response_object);
    }
    cJSON_Delete(root);
    fprintf(stderr, "onmessage: (%llu): %s\n", msg->payload_len, msg->payload);
    return 0;
}

int client_onopen(websocket_client *c)
{
    fprintf(stderr, "onopen called: %d\n", (int)c->socket);
    return 0;
}

void *websocket_client_launch_thread(void *ptr)
{
    aria2_object *aria2 = (aria2_object *)ptr;
    websocket_client *client = websocket_client_new(aria2->uri);
    websocket_client_onopen(client, client_onopen, aria2);
    websocket_client_onmessage(client, client_onmessage, aria2);
    websocket_client_onerror(client, client_onerror, aria2);
    websocket_client_onclose(client, client_onclose, aria2);
    websocket_client_run(client);
    aria2->client = client;
    aria2->init_completed = TRUE;
    websocket_client_finish(client);
    return NULL;
}

void aria2_shutdown(aria2_object *aria2)
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.shutdown", cJSON_CreateArray(), TRUE, TRUE);
    jsonrpc_response_object *object = aria2_request(aria2, request);
    jsonrpc_response_object_free(object);
}

void aria2_force_shutdown(aria2_object *aria2)
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.forceShutdown", cJSON_CreateArray(), TRUE, TRUE);
    jsonrpc_response_object *object = aria2_request(aria2, request);
    jsonrpc_response_object_free(object);
}

jsonrpc_response_object *aria2_add_uri(aria2_object *aria2, char **uris, int uris_len,
                                       key_value_pair options[], int options_len, int position)
{
    cJSON *params = cJSON_CreateArray();

    cJSON *uri_array = cJSON_CreateArray();
    for (int i = 0; i < uris_len; i++)
    {
        fprintf(stderr, "aria2_add_uri:%s\n", uris[i]);
        cJSON_AddItemToArray(uri_array, cJSON_CreateString(uris[i]));
    }
    cJSON_AddItemToArray(params, uri_array);

    cJSON *options_object = cJSON_CreateObject();
    for (int i = 0; i < options_len; i++)
    {
        cJSON_AddItemToObject(options_object, options[i].key, options[i].value);
    }
    cJSON_AddItemToArray(params, options_object);

    cJSON_AddItemToArray(params, cJSON_CreateNumber(position));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.addUri", params, TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_add_torrent(aria2_object *aria2, unsigned char *torrent, int torrent_length,
                                           char **uris, int uris_len, key_value_pair options[], int options_len, int position)
{
    cJSON *params = cJSON_CreateArray();

    int targetlen = (torrent_length + 2) / 3 * 4;
    char target[targetlen];
    base64_encode(torrent, torrent_length, target, targetlen);
    cJSON_AddItemToArray(params, cJSON_CreateString(target));

    cJSON *uri_array = cJSON_CreateArray();
    for (int i = 0; i < uris_len; i++)
    {
        cJSON_AddItemToArray(uri_array, cJSON_CreateString(uris[i]));
    }
    cJSON_AddItemToArray(params, uri_array);

    cJSON *options_object = cJSON_CreateObject();
    for (int i = 0; i < options_len; i++)
    {
        cJSON_AddItemToObject(options_object, options[i].key, options[i].value);
    }
    cJSON_AddItemToArray(params, options_object);

    cJSON_AddItemToArray(params, cJSON_CreateNumber(position));

    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.addTorrent", params, TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_add_metalink(aria2_object *aria2, unsigned char *torrent, int torrent_length,
                                            key_value_pair options[], int options_len, int position)
{
    cJSON *params = cJSON_CreateArray();

    int targetlen = (torrent_length + 2) / 3 * 4;
    char target[targetlen];
    base64_encode(torrent, torrent_length, target, targetlen);
    cJSON_AddItemToArray(params, cJSON_CreateString(target));

    cJSON *options_object = cJSON_CreateObject();
    for (int i = 0; i < options_len; i++)
    {
        cJSON_AddItemToObject(options_object, options[i].key, options[i].value);
    }
    cJSON_AddItemToArray(params, options_object);

    cJSON_AddItemToArray(params, cJSON_CreateNumber(position));

    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.addMetalink", params, TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_get_version(aria2_object *aria2)
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.getVersion", cJSON_CreateArray(), TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_remove(aria2_object *aria2, char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.remove", params, TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_force_remove(aria2_object *aria2, char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.forceRemove", cJSON_CreateArray(), TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_pause(aria2_object *aria2, char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.pause", params, TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_pause_all(aria2_object *aria2)
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.pauseAll", cJSON_CreateArray(), TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_force_pause(aria2_object *aria2, char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.forcePause", params, TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_force_pause_all(aria2_object *aria2)
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.forcePauseAll", cJSON_CreateArray(), TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_unpause(aria2_object *aria2, char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.unpause", params, TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_unpause_all(aria2_object *aria2)
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.unpauseAll", cJSON_CreateArray(), TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_tell_status(aria2_object *aria2, char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.tellStatus", params, TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_get_uris(aria2_object *aria2, char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.getUris", params, TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_get_files(aria2_object *aria2, char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.getFiles", params, TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_get_peers(aria2_object *aria2, char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.getPeers", params, TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_get_servers(aria2_object *aria2, char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.getServers", params, TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_change_position(aria2_object *aria2, char *gid, int pos, change_position_how how)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    cJSON_AddItemToArray(params, cJSON_CreateNumber(pos));
    switch (how)
    {
    case POS_SET:
        cJSON_AddItemToArray(params, cJSON_CreateString("POS_SET"));
        break;
    case POS_CUR:
        cJSON_AddItemToArray(params, cJSON_CreateString("POS_CUR"));
        break;
    case POS_END:
        cJSON_AddItemToArray(params, cJSON_CreateString("POS_END"));
        break;
    }
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.changePosition", params, TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_tell_active(aria2_object *aria2)
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.tellActive", cJSON_CreateArray(), TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_tell_waiting(aria2_object *aria2, int offset, int num)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateNumber(offset));
    cJSON_AddItemToArray(params, cJSON_CreateNumber(num));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.tellWaiting", params, TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_tell_stopped(aria2_object *aria2, int offset, int num)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateNumber(offset));
    cJSON_AddItemToArray(params, cJSON_CreateNumber(num));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.tellStopped", params, TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_tell_all(aria2_object *aria2)
{
    cJSON *params = cJSON_CreateArray();

    cJSON *tellStopped = cJSON_CreateObject();
    cJSON_AddItemToObject(tellStopped, "methodName", cJSON_CreateString("aria2.tellStopped"));
    cJSON *tellStoppedParams = cJSON_CreateArray();
    if (aria2->token)
    {
        cJSON_AddItemToArray(tellStoppedParams, cJSON_CreateString(string_format("token:%s", aria2->token)));
    }
    cJSON_AddItemToArray(tellStoppedParams, cJSON_CreateNumber(0));
    cJSON_AddItemToArray(tellStoppedParams, cJSON_CreateNumber(1000));
    cJSON_AddItemToObject(tellStopped, "params", tellStoppedParams);
    cJSON_AddItemToArray(params, tellStopped);

    cJSON *tellWaiting = cJSON_CreateObject();
    cJSON_AddItemToObject(tellWaiting, "methodName", cJSON_CreateString("aria2.tellWaiting"));
    cJSON *tellWaitingParams = cJSON_CreateArray();
    if (aria2->token)
    {
        cJSON_AddItemToArray(tellWaitingParams, cJSON_CreateString(string_format("token:%s", aria2->token)));
    }
    cJSON_AddItemToArray(tellWaitingParams, cJSON_CreateNumber(0));
    cJSON_AddItemToArray(tellWaitingParams, cJSON_CreateNumber(1000));
    cJSON_AddItemToObject(tellWaiting, "params", tellWaitingParams);
    cJSON_AddItemToArray(params, tellWaiting);

    cJSON *tellActive = cJSON_CreateObject();
    cJSON_AddItemToObject(tellActive, "methodName", cJSON_CreateString("aria2.tellActive"));
    cJSON *tellActiveParams = cJSON_CreateArray();
    if (aria2->token)
    {
        cJSON_AddItemToArray(tellActiveParams, cJSON_CreateString(string_format("token:%s", aria2->token)));
    }
    cJSON_AddItemToObject(tellActive, "params", tellActiveParams);
    cJSON_AddItemToArray(params, tellActive);

    jsonrpc_request_object *request = jsonrpc_request_object_create("system.multicall", params, TRUE, FALSE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_get_option(aria2_object *aria2, char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.getOption", params, TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_change_uri(aria2_object *aria2, char *gid, int fileindex,
                                          char **delUris, int delUris_len, char **addUris, int addUris_len, int position)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    cJSON_AddItemToArray(params, cJSON_CreateNumber(fileindex));

    cJSON *uri_array = cJSON_CreateArray();
    for (int i = 0; i < delUris_len; i++)
    {
        cJSON_AddItemToArray(uri_array, cJSON_CreateString(delUris[i]));
    }
    cJSON_AddItemToArray(params, uri_array);

    uri_array = cJSON_CreateArray();
    for (int i = 0; i < addUris_len; i++)
    {
        cJSON_AddItemToArray(uri_array, cJSON_CreateString(addUris[i]));
    }
    cJSON_AddItemToArray(params, uri_array);

    cJSON_AddItemToArray(params, cJSON_CreateNumber(position));

    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.changeUri", params, TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_change_option(aria2_object *aria2, char *gid, key_value_pair options[], int options_len)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));

    cJSON *options_object = cJSON_CreateObject();
    for (int i = 0; i < options_len; i++)
    {
        cJSON_AddItemToObject(options_object, options[i].key, options[i].value);
    }
    cJSON_AddItemToArray(params, options_object);

    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.changeOption", params, TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_get_global_option(aria2_object *aria2)
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.getGlobalOption", cJSON_CreateArray(), TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_change_global_option(aria2_object *aria2, key_value_pair options[], int options_len)
{
    cJSON *params = cJSON_CreateArray();

    cJSON *options_object = cJSON_CreateObject();
    for (int i = 0; i < options_len; i++)
    {
        cJSON_AddItemToObject(options_object, options[i].key, options[i].value);
    }
    cJSON_AddItemToArray(params, options_object);

    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.changeGlobalOption", params, TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_purge_download_result(aria2_object *aria2)
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.purgeDownloadResult", cJSON_CreateArray(), TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_remove_download_result(aria2_object *aria2, char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.removeDownloadResult", params, TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_get_session_info(aria2_object *aria2)
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.getSessionInfo", cJSON_CreateArray(), TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_get_global_stat(aria2_object *aria2)
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.getGlobalStat", cJSON_CreateArray(), TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}

jsonrpc_response_object *aria2_save_session(aria2_object *aria2)
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.saveSession", cJSON_CreateArray(), TRUE, TRUE);
    jsonrpc_response_object *result = aria2_request(aria2, request);
    return result;
}