#include "aria2.h"

// websocket常驻线程
static void *websocket_client_launch_thread(void *ptr);
// aria2实例
static aria2_object *aria2 = NULL;

int response_map_compare(const void *a, const void *b, void *udata)
{
    const response_map *ua = a;
    const response_map *ub = b;
    return strcmp(ua->id, ub->id);
}

uint64_t response_map_hash(const void *item, uint64_t seed0, uint64_t seed1)
{
    const response_map *user = item;
    return hashmap_sip(user->id, strlen(user->id), seed0, seed1);
}

bool response_map_iter(const void *item, void *udata)
{
    const response_map *map = item;
    if (map->data)
    {
        cJSON *result = map->data->result;
        jsonrpc_error_object *error = map->data->error;
        fprintf(stderr, "%s (result=%s,error=%s)\n",
                map->id,
                result ? cJSON_Print(result) : "null",
                error ? error->message : "null");
    }
    return true;
}

jsonrpc_request_object *jsonrpc_request_object_create(char *method, cJSON *params, int need_params, int need_token)
{
    char buf[UUID4_LEN];
    uuid4_init();
    uuid4_generate(buf);
    jsonrpc_request_object *object = (jsonrpc_request_object *)malloc(sizeof(jsonrpc_request_object));
    object->id = cJSON_CreateString(strdup(buf));
    object->method = method;
    object->params = params;
    object->need_params = need_params;
    object->need_token = need_token;
    return object;
}

jsonrpc_response_object *jsonrpc_response_object_create(cJSON *id, cJSON *result, jsonrpc_error_object *error)
{
    jsonrpc_response_object *object = (jsonrpc_response_object *)malloc(sizeof(jsonrpc_response_object));
    object->id = id;
    object->result = result;
    object->error = error;
    return object;
}

jsonrpc_error_object *jsonrpc_error_object_create(int code, char *message, cJSON *data)
{
    jsonrpc_error_object *object = (jsonrpc_error_object *)malloc(sizeof(jsonrpc_error_object));
    object->code = code;
    object->message = message;
    object->data = data;
    return object;
}

jsonrpc_response_object *aria2_request(jsonrpc_request_object *request)
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

    char *id = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(root, "id")));
    hashmap_set(aria2->map, &(response_map){.id = id, .data = NULL});

    char *json = cJSON_Print(root);
    fprintf(stderr, "send request: %s\n", json);
    websocket_client_send(aria2->client, json);

    jsonrpc_response_object *object = NULL;
    int try = 5;
    while (object == NULL && try > 0)
    {
        response_map *map = hashmap_get(aria2->map, &(response_map){.id = id});
        object = map->data;
        if (!object)
        {
            Sleep(aria2->polling_interval);
            try--;
            continue;
        }
    }
    // hashmap_scan(aria2->map, response_map_iter, NULL);
    cJSON_Delete(root);
    free(request);
    return object;
}

void aria2_new(char *uri, char *path, char *args, char *token)
{
    aria2 = (aria2_object *)malloc(sizeof(aria2_object));
    memset(aria2, 0, sizeof(aria2_object));
    aria2->ref_count = 0;
    aria2->uri = uri;
    aria2->path = path;
    aria2->args = args;
    aria2->token = token;
    aria2->polling_interval = 500;
    aria2->init_completed = FALSE;
    aria2->paused = TRUE;
    aria2->map = hashmap_new(sizeof(response_map), 0, 0, 0, response_map_hash, response_map_compare, NULL, NULL);
}

//命令执行成功返回0，执行失败返回-1。
int aria2_launch()
{
    char *path = "aria2c";
    char *args = "--conf-path=aria2.conf -D";
    uint16_t *path_utf16 = ug_utf8_to_utf16(path, -1, NULL);
    uint16_t *args_utf16 = ug_utf8_to_utf16(args, -1, NULL);
    //先启动aria2
    int result = (int)ShellExecuteW(NULL, L"open", path_utf16, args_utf16, NULL, SW_SHOW);//SW_HIDE;
    if (result > 32)
    {
        fprintf(stderr, "start aria2 success: %d\n", result);
    }
    else
    {
        fprintf(stderr, "start aria2 failed: %d\n", result);
    }
    //在启动websocketclient
    pthread_create(&aria2->client_thread, NULL, websocket_client_launch_thread, (void *)aria2);
    return TRUE;
}

uint8_t aria2_is_paused()
{
    return aria2->paused;
}

void aria2_set_paused(uint8_t b)
{
    aria2->paused = b;
}

uint8_t aria2_init_completed()
{
    return aria2->init_completed;
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
    cJSON *root = cJSON_Parse(msg->payload);
    cJSON *id = cJSON_GetObjectItem(root, "id");
    if (id == NULL)
    {
        return 0;
    }
    char *id_str = strdup(cJSON_GetStringValue(id));
    cJSON *result = cJSON_GetObjectItem(root, "result");
    cJSON *error = cJSON_GetObjectItem(root, "error");
    response_map *map = hashmap_get(aria2->map, &(response_map){.id = id_str});
    if (map)
    {
        jsonrpc_error_object *er = NULL;

        if (error)
        {
            er = jsonrpc_error_object_create(
                cJSON_GetObjectItem(error, "code")->valueint,
                cJSON_GetObjectItem(error, "message")->valuestring,
                cJSON_GetObjectItem(error, "data"));
        }

        hashmap_set(aria2->map, &(response_map){.id = id_str, .data = jsonrpc_response_object_create(id, result, er)});
    }

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
    websocket_client_onopen(client, client_onopen);
    websocket_client_onmessage(client, client_onmessage);
    websocket_client_onerror(client, client_onerror);
    websocket_client_onclose(client, client_onclose);
    websocket_client_run(client);
    aria2->client = client;
    aria2->init_completed = TRUE;
    websocket_client_finish(client);
    return NULL;
}

void aria2_shutdown()
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.shutdown", cJSON_CreateArray(), true, true);
    aria2_request(request);
}

void aria2_force_shutdown()
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.forceShutdown", cJSON_CreateArray(), true, true);
    aria2_request(request);
}

jsonrpc_response_object *aria2_add_uri(char **uris, int uris_len, key_value_pair options[], int options_len, int position)
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
    fprintf(stderr, "aria2_add_uri:jsonrpc_request_object_create\n");
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.addUri", params, true, true);
    fprintf(stderr, "aria2_add_uri:aria2_request\n");
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_add_torrent(unsigned char *torrent, char **uris, int uris_len, key_value_pair options[], int options_len, int position)
{
    cJSON *params = cJSON_CreateArray();

    int sourcelen = strlen(torrent);
    int targetlen = (sourcelen + 2) / 3 * 4;
    char target[targetlen];
    base64_encode(torrent, sourcelen, target, targetlen);
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

    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.addTorrent", params, true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_add_metalink(unsigned char *torrent, key_value_pair options[], int options_len, int position)
{
    cJSON *params = cJSON_CreateArray();

    int sourcelen = strlen(torrent);
    int targetlen = (sourcelen + 2) / 3 * 4;
    char target[targetlen];
    base64_encode(torrent, sourcelen, target, targetlen);
    cJSON_AddItemToArray(params, cJSON_CreateString(target));

    cJSON *options_object = cJSON_CreateObject();
    for (int i = 0; i < options_len; i++)
    {
        cJSON_AddItemToObject(options_object, options[i].key, options[i].value);
    }
    cJSON_AddItemToArray(params, options_object);

    cJSON_AddItemToArray(params, cJSON_CreateNumber(position));

    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.addMetalink", params, true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_get_version()
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.getVersion", cJSON_CreateArray(), true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_remove(char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.remove", params, true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_force_remove(char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.forceRemove", cJSON_CreateArray(), true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_pause(char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.pause", params, true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_pause_all()
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.pauseAll", cJSON_CreateArray(), true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_force_pause(char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.forcePause", params, true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_force_pause_all()
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.forcePauseAll", cJSON_CreateArray(), true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_unpause(char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.unpause", params, true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_unpause_all()
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.unpauseAll", cJSON_CreateArray(), true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_tell_status(char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.tellStatus", params, true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_get_uris(char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.getUris", params, true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_get_peers(char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.getPeers", params, true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_get_files(char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.getFiles", params, true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_get_servers(char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.getServers", params, true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_change_position(char *gid, int pos, change_position_how how)
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
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.changePosition", params, true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_tell_active()
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.tellActive", cJSON_CreateArray(), true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_tell_waiting(int offset, int num)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateNumber(offset));
    cJSON_AddItemToArray(params, cJSON_CreateNumber(num));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.tellWaiting", params, true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_tell_stopped(int offset, int num)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateNumber(offset));
    cJSON_AddItemToArray(params, cJSON_CreateNumber(num));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.tellStopped", params, true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_tell_all()
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

    jsonrpc_request_object *request = jsonrpc_request_object_create("system.multicall", params, true, false);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_get_option(char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.getOption", params, true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_change_uri(char *gid, int fileindex, char **delUris, int delUris_len, char **addUris, int addUris_len, int position)
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

    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.changeUri", params, true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_change_option(char *gid, key_value_pair options[], int options_len)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));

    cJSON *options_object = cJSON_CreateObject();
    for (int i = 0; i < options_len; i++)
    {
        cJSON_AddItemToObject(options_object, options[i].key, options[i].value);
    }
    cJSON_AddItemToArray(params, options_object);

    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.changeOption", params, true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_get_global_option()
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.getGlobalOption", cJSON_CreateArray(), true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_change_global_option(key_value_pair options[], int options_len)
{
    cJSON *params = cJSON_CreateArray();

    cJSON *options_object = cJSON_CreateObject();
    for (int i = 0; i < options_len; i++)
    {
        cJSON_AddItemToObject(options_object, options[i].key, options[i].value);
    }
    cJSON_AddItemToArray(params, options_object);

    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.changeGlobalOption", params, true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_purge_download_result()
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.purgeDownloadResult", cJSON_CreateArray(), true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_remove_download_result(char *gid)
{
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(gid));
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.removeDownloadResult", params, true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_get_session_info()
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.getSessionInfo", cJSON_CreateArray(), true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_get_global_stat()
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.getGlobalStat", cJSON_CreateArray(), true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}

jsonrpc_response_object *aria2_save_session()
{
    jsonrpc_request_object *request = jsonrpc_request_object_create("aria2.saveSession", cJSON_CreateArray(), true, true);
    jsonrpc_response_object *result = aria2_request(request);
    return result;
}