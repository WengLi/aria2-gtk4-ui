#include "aria2_entity.h"

void ptr_array_add(ptr_array ptr_need_free, void *ptr)
{
    if (!ptr_need_free.ptr)
    {
        ptr_need_free.buffer_size = 32;
        ptr_need_free.ptr = (void **)malloc(ptr_need_free.buffer_size * sizeof(void *));
    }
    ptr_need_free.size++;
    if (ptr_need_free.size > ptr_need_free.buffer_size)
    {
        ptr_need_free.buffer_size = ptr_need_free.buffer_size * 1.5;
        ptr_need_free.ptr = (void **)realloc(ptr_need_free.ptr, ptr_need_free.buffer_size * sizeof(void *));
    }
    ptr_need_free.ptr[ptr_need_free.size - 1] = ptr;
}

// string
char *cjson_to_string(cJSON *c, ptr_array ptr_need_free)
{
    if (cJSON_IsString(c))
    {
        char *result = strdup(c->valuestring);
        ptr_array_add(ptr_need_free, result);
        return result;
    }
    return NULL;
}

// string[]
char **cjson_to_string_array(cJSON *c, int *out_size, ptr_array ptr_need_free)
{
    int len = 0;
    if (cJSON_IsArray(c))
    {
        len = cJSON_GetArraySize(c);
    }
    if (out_size)
    {
        *out_size = len;
    }
    if (len == 0)
    {
        return NULL;
    }
    char **result = (char **)malloc(len * sizeof(char *));
    ptr_array_add(ptr_need_free, result);
    for (int i = 0; i < len; i++)
    {
        result[i] = cjson_to_string(cJSON_GetArrayItem(c, i), ptr_need_free);
    }
    return result;
}

// string[][]
char ***cjson_to_string_2d_array(cJSON *c, int *row_size, int **col_size, ptr_array ptr_need_free)
{
    int rlen = 0;
    if (cJSON_IsArray(c))
    {
        rlen = cJSON_GetArraySize(c);
    }
    if (row_size)
    {
        *row_size = rlen;
    }
    if (rlen == 0)
    {
        return NULL;
    }
    char ***result = (char ***)malloc(rlen * sizeof(char **));
    ptr_array_add(ptr_need_free, result);
    if (col_size)
    {
        *col_size = (int *)malloc(rlen * sizeof(int));
        ptr_array_add(ptr_need_free, *col_size);
    }
    for (int i = 0; i < rlen; i++)
    {
        int clen = 0;
        cJSON *array = cJSON_GetArrayItem(c, i);
        if (cJSON_IsArray(array))
        {
            clen = cJSON_GetArraySize(array);
        }
        if (col_size)
        {
            (*col_size)[i] = clen;
        }
        if (clen == 0)
        {
            result[i] = NULL;
            continue;
        }
        result[i] = (char **)malloc(clen * sizeof(char *));
        ptr_array_add(ptr_need_free, result[i]);
        for (int j = 0; j < clen; j++)
        {
            result[i][j] = cjson_to_string(cJSON_GetArrayItem(array, j), ptr_need_free);
        }
    }
    return result;
}

int64_t cjson_to_int64(cJSON *c)
{
    if (cJSON_IsNumber(c))
    {
        return double_to_int64(c->valuedouble);
    }
    if (cJSON_IsString(c))
    {
        return atoll(c->valuestring);
    }
    return 0;
}

int64_t *cjson_to_int64_array(cJSON *c, int *out_size, ptr_array ptr_need_free)
{
    int len = 0;
    if (cJSON_IsArray(c))
    {
        len = cJSON_GetArraySize(c);
    }
    if (out_size)
    {
        *out_size = len;
    }
    if (len == 0)
    {
        return NULL;
    }
    int64_t *result = (int64_t *)malloc(len * sizeof(int64_t));
    ptr_array_add(ptr_need_free, result);
    for (int i = 0; i < len; i++)
    {
        result[i] = cjson_to_int64(cJSON_GetArrayItem(c, i));
    }
    return result;
}

bool cjson_to_bool(cJSON *c)
{
    if (cJSON_IsString(c))
    {
        if (c->valuestring && strcasecmp(c->valuestring, "true") == 0)
        {
            return true;
        }
    }
    return false;
}

aria2_item_file_uri *cjson_to_aria2_item_file_uri(cJSON *c, ptr_array ptr_need_free)
{
    if (!cJSON_IsObject(c))
    {
        return NULL;
    }
    aria2_item_file_uri *result = (aria2_item_file_uri *)malloc(sizeof(aria2_item_file_uri));
    memset(result, 0, sizeof(aria2_item_file_uri));
    ptr_array_add(ptr_need_free, result);
    result->uri = cjson_to_string(cJSON_GetObjectItem(c, "uri"), ptr_need_free);
    result->status = cjson_to_string(cJSON_GetObjectItem(c, "status"), ptr_need_free);
    return result;
}

aria2_item_file_uri **cjson_to_aria2_item_file_uris(cJSON *c, int *out_size, ptr_array ptr_need_free)
{
    int len = 0;
    if (cJSON_IsArray(c))
    {
        len = cJSON_GetArraySize(c);
    }
    if (out_size)
    {
        *out_size = len;
    }
    if (len == 0)
    {
        return NULL;
    }
    aria2_item_file_uri **result = (aria2_item_file_uri **)malloc(len * sizeof(aria2_item_file_uri *));
    ptr_array_add(ptr_need_free, result);
    for (int i = 0; i < len; i++)
    {
        result[i] = cjson_to_aria2_item_file_uri(cJSON_GetArrayItem(c, i), ptr_need_free);
    }
    return result;
}

aria2_item_file *cjson_to_aria2_item_file(cJSON *c, ptr_array ptr_need_free)
{
    if (!cJSON_IsObject(c))
    {
        return NULL;
    }
    aria2_item_file *result = (aria2_item_file *)malloc(sizeof(aria2_item_file));
    memset(result, 0, sizeof(aria2_item_file));
    ptr_array_add(ptr_need_free, result);
    result->index = cjson_to_int64(cJSON_GetObjectItem(c, "index"));
    result->path = cjson_to_string(cJSON_GetObjectItem(c, "path"), ptr_need_free);
    result->length = cjson_to_int64(cJSON_GetObjectItem(c, "length"));
    result->completedLength = cjson_to_int64(cJSON_GetObjectItem(c, "completedLength"));
    result->selected = cjson_to_bool(cJSON_GetObjectItem(c, "selected"));
    result->uris = cjson_to_aria2_item_file_uris(cJSON_GetObjectItem(c, "uris"), &result->uris_size, ptr_need_free);
    return result;
}

aria2_item_file **cjson_to_aria2_item_files(cJSON *c, int *out_size, ptr_array ptr_need_free)
{
    int len = 0;
    if (cJSON_IsArray(c))
    {
        len = cJSON_GetArraySize(c);
    }
    if (out_size)
    {
        *out_size = len;
    }
    if (len == 0)
    {
        return NULL;
    }
    aria2_item_file **files = (aria2_item_file **)malloc(len * sizeof(aria2_item_file *));
    ptr_array_add(ptr_need_free, files);
    for (int i = 0; i < len; i++)
    {
        files[i] = cjson_to_aria2_item_file(cJSON_GetArrayItem(c, i), ptr_need_free);
    }
    return files;
}

aria2_item_bittorrent_info *cjson_to_aria2_item_bittorrent_info(cJSON *c, ptr_array ptr_need_free)
{
    if (!cJSON_IsObject(c))
    {
        return NULL;
    }
    aria2_item_bittorrent_info *result = (aria2_item_bittorrent_info *)malloc(sizeof(aria2_item_bittorrent_info));
    memset(result, 0, sizeof(aria2_item_bittorrent_info));
    ptr_array_add(ptr_need_free, result);
    result->name = cjson_to_string(cJSON_GetObjectItem(c, "name"), ptr_need_free);
    result->name_utf8 = cjson_to_string(cJSON_GetObjectItem(c, "name.utf-8"), ptr_need_free);
    return result;
}

aria2_item_bittorrent *cjson_to_aria2_item_bittorrent(cJSON *c, ptr_array ptr_need_free)
{
    if (!cJSON_IsObject(c))
    {
        return NULL;
    }
    aria2_item_bittorrent *result = (aria2_item_bittorrent *)malloc(sizeof(aria2_item_bittorrent));
    memset(result, 0, sizeof(aria2_item_bittorrent));
    ptr_array_add(ptr_need_free, result);
    result->announceList = cjson_to_string_2d_array(cJSON_GetObjectItem(c, "announceList"),
                                                    &result->announceList_row_size, &result->announceList_col_size, ptr_need_free);
    result->comment = cjson_to_string(cJSON_GetObjectItem(c, "comment"), ptr_need_free);
    result->creationDate = cjson_to_int64(cJSON_GetObjectItem(c, "creationDate"));
    result->mode = cjson_to_string(cJSON_GetObjectItem(c, "mode"), ptr_need_free);
    result->info = cjson_to_aria2_item_bittorrent_info(cJSON_GetObjectItem(c, "info"), ptr_need_free);
    return result;
}

aria2_item *cjson_to_aria2_item(cJSON *result)
{
    aria2_item *item = (aria2_item *)malloc(sizeof(aria2_item));
    memset(item, 0, sizeof(aria2_item));
    item->gid = cjson_to_string(cJSON_GetObjectItem(result, "gid"), item->ptr_need_free);
    item->status = cjson_to_string(cJSON_GetObjectItem(result, "status"), item->ptr_need_free);
    item->totalLength = cjson_to_int64(cJSON_GetObjectItem(result, "totalLength"));
    item->completedLength = cjson_to_int64(cJSON_GetObjectItem(result, "completedLength"));
    item->uploadLength = cjson_to_int64(cJSON_GetObjectItem(result, "uploadLength"));
    item->bitfield = cjson_to_string(cJSON_GetObjectItem(result, "bitfield"), item->ptr_need_free);
    item->downloadSpeed = cjson_to_int64(cJSON_GetObjectItem(result, "downloadSpeed"));
    item->uploadSpeed = cjson_to_int64(cJSON_GetObjectItem(result, "uploadSpeed"));
    item->infoHash = cjson_to_string(cJSON_GetObjectItem(result, "infoHash"), item->ptr_need_free);
    item->numSeeders = cjson_to_int64(cJSON_GetObjectItem(result, "numSeeders"));
    item->seeder = cjson_to_bool(cJSON_GetObjectItem(result, "seeder"));
    item->pieceLength = cjson_to_int64(cJSON_GetObjectItem(result, "pieceLength"));
    item->numPieces = cjson_to_int64(cJSON_GetObjectItem(result, "numPieces"));
    item->connections = cjson_to_int64(cJSON_GetObjectItem(result, "connections"));
    item->errorCode = cjson_to_string(cJSON_GetObjectItem(result, "errorCode"), item->ptr_need_free);
    item->errorMessage = cjson_to_string(cJSON_GetObjectItem(result, "errorMessage"), item->ptr_need_free);
    item->followedBy = cjson_to_string_array(cJSON_GetObjectItem(result, "followedBy"), &item->followedBy_size, item->ptr_need_free);
    item->following = cjson_to_string(cJSON_GetObjectItem(result, "following"), item->ptr_need_free);
    item->belongsTo = cjson_to_string(cJSON_GetObjectItem(result, "belongsTo"), item->ptr_need_free);
    item->dir = cjson_to_string(cJSON_GetObjectItem(result, "dir"), item->ptr_need_free);
    item->files = cjson_to_aria2_item_files(cJSON_GetObjectItem(result, "files"), &item->files_size, item->ptr_need_free);
    item->bittorrent = cjson_to_aria2_item_bittorrent(cJSON_GetObjectItem(result, "bittorrent"), item->ptr_need_free);
    item->verifiedLength = cjson_to_int64(cJSON_GetObjectItem(result, "verifiedLength"));
    item->verifyIntegrityPending = cjson_to_bool(cJSON_GetObjectItem(result, "verifyIntegrityPending"));
    return item;
}

aria2_item **cjson_to_aria2_item_array(cJSON *c, int *out_size)
{
    int len = 0;
    if (cJSON_IsArray(c))
    {
        len = cJSON_GetArraySize(c);
    }
    if (out_size)
    {
        *out_size = len;
    }
    if (len == 0)
    {
        return NULL;
    }
    aria2_item **result = (aria2_item **)malloc(len * sizeof(aria2_item *));
    for (int i = 0; i < len; i++)
    {
        result[i] = cjson_to_aria2_item(cJSON_GetArrayItem(c, i));
    }
    return result;
}

aria2_items *cjson_to_aria2_items(cJSON *c)
{
    aria2_items *result = (aria2_items *)malloc(sizeof(aria2_items));
    memset(result, 0, sizeof(aria2_items));
    result->items = cjson_to_aria2_item_array(c, &result->items_size);
    return result;
}

aria2_peer *cjson_to_aria2_peer(cJSON *c, ptr_array ptr_need_free)
{
    if (!cJSON_IsObject(c))
    {
        return NULL;
    }
    aria2_peer *result = (aria2_peer *)malloc(sizeof(aria2_peer));
    memset(result, 0, sizeof(aria2_peer));
    ptr_array_add(ptr_need_free, result);
    result->peerId = cjson_to_string(cJSON_GetObjectItem(c, "peerId"), ptr_need_free);
    result->ip = cjson_to_string(cJSON_GetObjectItem(c, "ip"), ptr_need_free);
    result->port = cjson_to_int64(cJSON_GetObjectItem(c, "port"));
    result->bitfield = cjson_to_string(cJSON_GetObjectItem(c, "bitfield"), ptr_need_free);
    result->amChoking = cjson_to_bool(cJSON_GetObjectItem(c, "amChoking"));
    result->peerChoking = cjson_to_bool(cJSON_GetObjectItem(c, "peerChoking"));
    result->downloadSpeed = cjson_to_int64(cJSON_GetObjectItem(c, "downloadSpeed"));
    result->uploadSpeed = cjson_to_int64(cJSON_GetObjectItem(c, "uploadSpeed"));
    result->seeder = cjson_to_bool(cJSON_GetObjectItem(c, "seeder"));
    return result;
}

aria2_peer **cjson_to_aria2_peers(cJSON *c, int *out_size, ptr_array ptr_need_free)
{
    int len = 0;
    if (cJSON_IsArray(c))
    {
        len = cJSON_GetArraySize(c);
    }
    if (out_size)
    {
        *out_size = len;
    }
    if (len == 0)
    {
        return NULL;
    }
    aria2_peer **files = (aria2_peer **)malloc(len * sizeof(aria2_peer *));
    ptr_array_add(ptr_need_free, files);
    for (int i = 0; i < len; i++)
    {
        files[i] = cjson_to_aria2_peer(cJSON_GetArrayItem(c, i), ptr_need_free);
    }
    return files;
}

aria2_server *cjson_to_aria2_server(cJSON *c, ptr_array ptr_need_free)
{
    if (!cJSON_IsObject(c))
    {
        return NULL;
    }
    aria2_server *result = (aria2_server *)malloc(sizeof(aria2_server));
    memset(result, 0, sizeof(aria2_server));
    ptr_array_add(ptr_need_free, result);
    result->uri = cjson_to_string(cJSON_GetObjectItem(c, "uri"), ptr_need_free);
    result->currentUri = cjson_to_string(cJSON_GetObjectItem(c, "currentUri"), ptr_need_free);
    result->downloadSpeed = cjson_to_int64(cJSON_GetObjectItem(c, "downloadSpeed"));
    return result;
}

aria2_server **cjson_to_aria2_servers(cJSON *c, int *out_size, ptr_array ptr_need_free)
{
    int len = 0;
    if (cJSON_IsArray(c))
    {
        len = cJSON_GetArraySize(c);
    }
    if (out_size)
    {
        *out_size = len;
    }
    if (len == 0)
    {
        return NULL;
    }
    aria2_server **files = (aria2_server **)malloc(len * sizeof(aria2_server *));
    ptr_array_add(ptr_need_free, files);
    for (int i = 0; i < len; i++)
    {
        files[i] = cjson_to_aria2_server(cJSON_GetArrayItem(c, i), ptr_need_free);
    }
    return files;
}

aria2_option *cjson_to_aria2_option(cJSON *c, ptr_array ptr_need_free)
{
    if (!cJSON_IsString(c))
    {
        return NULL;
    }
    aria2_option *result = (aria2_option *)malloc(sizeof(aria2_option));
    memset(result, 0, sizeof(aria2_option));
    ptr_array_add(ptr_need_free, result);
    result->name = cjson_to_string(c->string, ptr_need_free);
    result->value = cjson_to_string(c->valuestring, ptr_need_free);
    return result;
}

aria2_option **cjson_to_aria2_options(cJSON *c, int *out_size, ptr_array ptr_need_free)
{
    int len = 0;
    aria2_option **files = NULL;
    if (cJSON_IsObject(c) && c->child != NULL)
    {
        cJSON *cur = c->child;
        while (cur != NULL)
        {
            len++;
            cur = cur->next;
        }

        aria2_option **files = (aria2_option **)malloc(len * sizeof(aria2_option *));
        ptr_array_add(ptr_need_free, files);
        int i = 0;
        cur = c->child;
        while (cur != NULL)
        {
            files[i++] = cjson_to_aria2_option(cur, ptr_need_free);
            cur = cur->next;
        }
    }
    if (out_size)
    {
        *out_size = len;
    }
    return files;
}

aria2_globalstat *cjson_to_aria2_globalstat(cJSON *c)
{
    if (!cJSON_IsObject(c))
    {
        return NULL;
    }
    aria2_globalstat *result = (aria2_globalstat *)malloc(sizeof(aria2_globalstat));
    memset(result, 0, sizeof(aria2_globalstat));
    result->downloadSpeed = cjson_to_int64(cJSON_GetObjectItem(c, "downloadSpeed"));
    result->uploadSpeed = cjson_to_int64(cJSON_GetObjectItem(c, "uploadSpeed"));
    result->numActive = cjson_to_int64(cJSON_GetObjectItem(c, "numActive"));
    result->numWaiting = cjson_to_int64(cJSON_GetObjectItem(c, "numWaiting"));
    result->numStopped = cjson_to_int64(cJSON_GetObjectItem(c, "numStopped"));
    result->numStoppedTotal = cjson_to_int64(cJSON_GetObjectItem(c, "numStoppedTotal"));
    return result;
}

void ptr_array_free(ptr_array arr)
{
    if (arr.ptr)
    {
        for (int i = 0; i < arr.size; i++)
        {
            free(arr.ptr[i]);
        }
        free(arr.ptr);
    }
}

void aria2_item_free(aria2_item *item)
{
    if (item == NULL)
    {
        return;
    }
    ptr_array_free(item->ptr_need_free);
    free(item);
}

void aria2_item_array_free(aria2_item **items, int row_size)
{
    if (row_size == 0)
    {
        return;
    }
    for (int i = 0; i < row_size; i++)
    {
        if (items[i])
        {
            ptr_array_free(items[i]->ptr_need_free);
            free(items[i]);
        }
    }
    free(items);
}

void aria2_item_all_free(aria2_item_all *c)
{
    if (!c)
    {
        return;
    }
    if (c->item_active)
    {
        aria2_item_array_free(c->item_active->items, c->item_active->items_size);
        free(c->item_active);
    }
    if (c->item_waiting)
    {
        aria2_item_array_free(c->item_waiting->items, c->item_waiting->items_size);
        free(c->item_waiting);
    }
    if (c->item_stopped)
    {
        aria2_item_array_free(c->item_stopped->items, c->item_stopped->items_size);
        free(c->item_stopped);
    }
    free(c);
}

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

jsonrpc_response_result *jsonrpc_response_result_create(jsonrpc_response_object *response)
{
    jsonrpc_response_result *object = (jsonrpc_response_result *)malloc(sizeof(jsonrpc_response_result));
    memset(object, 0, sizeof(jsonrpc_response_result));
    if (response == NULL || response->error != NULL)
    {
        object->successed = false;
        if (response && response->error)
        {
            object->error_code = response->error->code;
            object->error_message = strdup(response->error->message);
        }
        else
        {
            object->error_code = -1;
            object->error_message = strdup("aria2 request error");
        }
    }
    else
    {
        object->successed = true;
    }
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