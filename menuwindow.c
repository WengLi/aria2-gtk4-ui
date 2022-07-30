#include "menuwindow.h"
#include "tray_ico_helper.c"

static GArray *column_type_array = NULL;

static GtkWidget *window;
static GtkWidget *aboutdialog1;
static GtkWidget *statusbar1;
static GtkWidget *newdownloadialog1;
static GtkWidget *url_textview;
static GtkWidget *treeview1;
static GtkWidget *scrolledwindow1;
static GtkWidget *newdownloa_grid;
static GtkWidget *data_box;
static GtkWidget *open_btmt_file_btn;
static GtkWidget *locationfilebtn;
static GtkWidget *file_name_text;
static GtkWidget *spin_conn_btn;
static aria2_object *aria2;
static download_type cur_download_type;

static GtkTreeModel *create_items_model();
static void add_columns(GtkTreeView *treeview);
static void start_list_store_fresh_thread();
static void *list_store_fresh_thread(void *ptr);
static int add_or_update_item(aria2_item *foo, GtkTreeModel *model);
static GtkTreePath *get_tree_path_by_gid(GtkTreeView *treeview, gchar *gid);

void show_dialog(const char *title, const char *content)
{
  GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                             GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_MESSAGE_INFO,
                                             GTK_BUTTONS_OK,
                                             title);
  gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), content);
  g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
  gtk_widget_show(dialog);
}

static void location_file_chooser_response(GtkNativeDialog *dialog, int response, location_file_args *args)
{
  gtk_native_dialog_hide(dialog);

  if (response == GTK_RESPONSE_ACCEPT)
  {
    GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
    if (args != NULL)
    {
      if (args->cb != NULL)
      {
        args->cb(args->widget, file, args->data);
      }
      free(args);
    }
    g_object_unref(file);
  }
  gtk_native_dialog_destroy(dialog);
}

void location_file_chooser(GtkWidget *widget,
                           GtkFileChooserAction action,
                           int (*cb)(GtkWidget *, GFile *, gpointer),
                           gpointer user_data,
                           char *pattern)
{
  location_file_args *args = (location_file_args *)malloc(sizeof(location_file_args));
  memset(args, 0, sizeof(location_file_args));
  args->widget = widget;
  args->data = user_data;
  args->cb = cb;

  GtkFileChooserNative *chooser =
      gtk_file_chooser_native_new("Choose a file",
                                  GTK_WINDOW(gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW)),
                                  action,
                                  "_Open",
                                  "_Cancel");
  if (GTK_FILE_CHOOSER_ACTION_OPEN == action && pattern != NULL && strlen(pattern) > 0)
  {
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, pattern);
    gtk_file_chooser_add_filter(chooser, filter);
  }

  g_signal_connect(chooser, "response", G_CALLBACK(location_file_chooser_response), args);
  gtk_native_dialog_show(GTK_NATIVE_DIALOG(chooser));
}

//解析网址格式，迅雷快车qq旋风地址解析
int get_real_uri(char *src, char *target)
{
  int i, z, k;
  char scheme[10] = {0};
  char host[4096] = {0};
  char *p = strstr(src, "://");
  if (p == NULL)
  {
    return 0;
  }
  strncpy(scheme, src, p - src);
  scheme[p - src] = '\0';
  for (i = p - src + 3, z = 0; *(src + i) != '\0'; i++, z++)
  {
    host[z] = *(src + i);
  }
  host[z] = '\0';
  int flag = 0;
  if (strcasecmp(scheme, "thunder") == 0)
  {
    flag = 1;
    k = strlen("AA");
  }
  else if (strcasecmp(scheme, "flashget") == 0)
  {
    flag = 1;
    k = strlen("[flashget]");
  }
  else if (strcasecmp(scheme, "qqdl") == 0)
  {
    flag = 1;
    k = 0;
  }
  if (flag == 1)
  {
    char t[4096] = {0};
    if (base64_decode(host, t, 4096) == -1)
    {
      return 0;
    }
    int j;
    int size = strlen(t) - 2 * k;
    for (j = 0; j < size; j++)
    {
      target[j] = t[j + k];
    }
    target[j] = '\0';
  }
  else
  {
    strncpy(target, src, strlen(src));
  }
  return 1;
}

static void quit_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GtkWidget *window = user_data;
  aria2_shutdown(aria2);
  gtk_window_destroy(GTK_WINDOW(window));
}

static void about_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  if (aria2->init_completed == TRUE)
  {
    char *fs = string_join(",", aria2->version->enabledFeatures, aria2->version->enabledFeatures_size);
    char *tips = string_format("Version:%s\nFeatures:%s", aria2->version->version, fs);
    show_dialog("aria2 info", tips);
    free(fs);
    free(tips);
  }
  else
  {
    show_dialog("tips", "waiting aria2 startup");
  }
  // gtk_window_present(GTK_WINDOW(aboutdialog1));
}

static void remove_timeout(gpointer data)
{
  guint id = GPOINTER_TO_UINT(data);

  g_source_remove(id);
}

static gboolean pop_status(gpointer data)
{
  gtk_statusbar_pop(GTK_STATUSBAR(data), 0);
  g_object_set_data(G_OBJECT(data), "timeout", NULL);
  return G_SOURCE_REMOVE;
}

static void status_message(GtkStatusbar *status, const char *text)
{
  guint id;

  gtk_statusbar_push(GTK_STATUSBAR(status), 0, text);
  id = g_timeout_add(5000, pop_status, status);

  g_object_set_data_full(G_OBJECT(status), "timeout", GUINT_TO_POINTER(id), remove_timeout);
}

static void add_uri_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  cur_download_type = uri;
  GtkWidget *temp = gtk_grid_get_child_at(GTK_GRID(newdownloa_grid), 0, 0);
  gtk_widget_show(temp);
  temp = gtk_grid_get_child_at(GTK_GRID(newdownloa_grid), 1, 0);
  gtk_widget_show(temp);

  temp = gtk_grid_get_child_at(GTK_GRID(newdownloa_grid), 0, 1);
  gtk_widget_hide(temp);
  temp = gtk_grid_get_child_at(GTK_GRID(newdownloa_grid), 1, 1);
  gtk_widget_hide(temp);

  gtk_widget_show(newdownloadialog1);
}

static void add_torrent_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  cur_download_type = torrent;
  GtkWidget *temp = gtk_grid_get_child_at(GTK_GRID(newdownloa_grid), 0, 0);
  gtk_widget_hide(temp);
  temp = gtk_grid_get_child_at(GTK_GRID(newdownloa_grid), 1, 0);
  gtk_widget_hide(temp);

  temp = gtk_grid_get_child_at(GTK_GRID(newdownloa_grid), 0, 1);
  gtk_widget_show(temp);
  temp = gtk_grid_get_child_at(GTK_GRID(newdownloa_grid), 1, 1);
  gtk_widget_show(temp);

  gtk_widget_show(newdownloadialog1);
}

static void add_metalink_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  cur_download_type = metalink;
  GtkWidget *temp = gtk_grid_get_child_at(GTK_GRID(newdownloa_grid), 0, 0);
  gtk_widget_hide(temp);
  temp = gtk_grid_get_child_at(GTK_GRID(newdownloa_grid), 1, 0);
  gtk_widget_hide(temp);

  temp = gtk_grid_get_child_at(GTK_GRID(newdownloa_grid), 0, 1);
  gtk_widget_show(temp);
  temp = gtk_grid_get_child_at(GTK_GRID(newdownloa_grid), 1, 1);
  gtk_widget_show(temp);

  gtk_widget_show(newdownloadialog1);
}

static void aria2_remove_result(char *gid, GtkTreeIter iter, GtkTreeModel *model, aria2_item *item)
{
  char *af;
  int rval = -1;
  if (item->bittorrent && item->bittorrent->info)
  {
    for (int i = 0; i < item->files_size; i++)
    {
      remove_file(item->files[i]->path);
    }
    af = string_format("%s\\%s", item->dir, item->bittorrent->info->name);
    rval = remove_file(item->bittorrent->info->name);
    free(af);
    af = string_format("%s\\%s.aria2", item->dir, item->bittorrent->info->name);
    rval = remove_file(af);
    free(af);
  }
  else if (item->files_size > 0)
  {
    rval = remove_file(item->files[0]->path);
    af = string_format("%s.aria2", item->files[0]->path);
    rval = remove_file(af);
    free(af);
  }
  jsonrpc_response_result *response = aria2_remove_download_result(aria2, gid);
  if (!response->successed)
  {
    free(response->error_message);
  }
  else
  {
    free(response->data_ptr);
  }
  free(response);
  gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
}

static void remove_task_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview1));
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview1));

  if (gtk_tree_selection_get_selected(selection, NULL, &iter))
  {
    gchar *out_gid;
    gtk_tree_model_get(model, &iter, get_tree_view_column_index_by_type(COLUMN_GID), &out_gid, -1);
    if (!out_gid)
    {
      return;
    }
    jsonrpc_response_result *response = aria2_tell_status(aria2, out_gid);
    if (!response->successed)
    {
      show_dialog("error", response->error_message);
      free(response->error_message);
      free(response);
      return;
    }
    aria2_item *item = response->data_ptr;
    free(response);
    if (strcmp(item->status, "completed") == 0 || strcmp(item->status, "error") == 0 || strcmp(item->status, "removed") == 0)
    {
      aria2_remove_result(out_gid, iter, model, item);
      return;
    }
    response = aria2_force_remove(aria2, out_gid);
    if (!response->successed)
    {
      show_dialog("error", response->error_message);
      free(response->error_message);
    }
    else
    {
      aria2_remove_result(out_gid, iter, model, item);
      start_list_store_fresh_thread();
      free(response->data_ptr);
      aria2_item_free(item);
    }
    free(response);
  }
}

static void pause_task_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview1));
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview1));

  if (gtk_tree_selection_get_selected(selection, NULL, &iter))
  {
    gchar *status;
    gtk_tree_model_get(model, &iter, get_tree_view_column_index_by_type(COLUMN_STATUS), &status, -1);
    if (strcmp(status, "active") != 0)
    {
      return;
    }
    gchar *out_gid;
    gtk_tree_model_get(model, &iter, get_tree_view_column_index_by_type(COLUMN_GID), &out_gid, -1);
    if (!out_gid)
    {
      return;
    }
    jsonrpc_response_result *response = aria2_pause(aria2, out_gid);
    if (!response->successed)
    {
      show_dialog("error", response->error_message);
      free(response->error_message);
    }
    else
    {
      start_list_store_fresh_thread();
      free(response->data_ptr);
    }
    free(response);
  }
}

static void unpause_task_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview1));
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview1));

  if (gtk_tree_selection_get_selected(selection, NULL, &iter))
  {
    gchar *status;
    gtk_tree_model_get(model, &iter, get_tree_view_column_index_by_type(COLUMN_STATUS), &status, -1);
    if (strcmp(status, "active") == 0)
    {
      return;
    }
    gchar *out_gid;
    gtk_tree_model_get(model, &iter, get_tree_view_column_index_by_type(COLUMN_GID), &out_gid, -1);
    if (!out_gid)
    {
      return;
    }
    jsonrpc_response_result *response = aria2_unpause(aria2, out_gid);
    if (!response->successed)
    {
      show_dialog("error", response->error_message);
      free(response->error_message);
    }
    else
    {
      start_list_store_fresh_thread();
      free(response->data_ptr);
    }
    free(response);
  }
}

static void pause_all_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  jsonrpc_response_result *response = aria2_pause_all(aria2);
  if (!response->successed)
  {
    show_dialog("tips", response->error_message);
    free(response->error_message);
    free(response);
  }
  else
  {
    free(response->data_ptr);
    free(response);
  }
}

static void unpause_all_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  jsonrpc_response_result *response = aria2_unpause_all(aria2);
  if (!response->successed)
  {
    show_dialog("tips", response->error_message);
    free(response->error_message);
    free(response);
  }
  else
  {
    start_list_store_fresh_thread();
    free(response->data_ptr);
    free(response);
  }
}

static void purge_download_result_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  jsonrpc_response_result *response = aria2_purge_download_result(aria2);
  if (!response->successed)
  {
    show_dialog("tips", response->error_message);
    free(response->error_message);
    free(response);
  }
  else
  {
    start_list_store_fresh_thread();
    free(response->data_ptr);
    free(response);
  }
}

static void open_folder_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview1));
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview1));

  if (gtk_tree_selection_get_selected(selection, NULL, &iter))
  {
    gchar *out_gid;
    gtk_tree_model_get(model, &iter, get_tree_view_column_index_by_type(COLUMN_GID), &out_gid, -1);
    if (!out_gid)
    {
      return;
    }
    jsonrpc_response_result *response = aria2_tell_status(aria2, out_gid);
    if (!response->successed)
    {
      show_dialog("error", response->error_message);
      free(response->error_message);
    }
    else
    {
      aria2_item *opts = (aria2_item *)response->data_ptr;
      char *args = string_format("/e,\"%s\"", opts->dir);
      uint16_t *path_utf16 = utf8_to_utf16("explorer", -1, NULL);
      uint16_t *args_utf16 = utf8_to_utf16(args, -1, NULL);
      ShellExecuteW(NULL, L"open", path_utf16, args_utf16, NULL, SW_SHOW);
      free(args_utf16);
      free(path_utf16);
      free(args);
      aria2_item_free(opts);
    }
    free(response);
  }
}

static void move_up_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview1));
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview1));

  if (gtk_tree_selection_get_selected(selection, NULL, &iter))
  {
    gchar *out_gid;
    gtk_tree_model_get(model, &iter, get_tree_view_column_index_by_type(COLUMN_GID), &out_gid, -1);
    if (!out_gid)
    {
      return;
    }
    jsonrpc_response_result *response = aria2_change_position(aria2, out_gid, -1, POS_CUR);
    if (!response->successed)
    {
      show_dialog("error", response->error_message);
      free(response->error_message);
      free(response);
    }
    else
    {
      GtkTreeIter iter2;
      GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
      int i = gtk_tree_path_get_indices(path)[0];
      if (i > 0)
      {
        GtkTreePath *path2 = gtk_tree_path_new_from_indices(i - 1, -1);
        gtk_tree_model_get_iter(model, &iter2, path2);
        gtk_list_store_swap(GTK_LIST_STORE(model), &iter, &iter2);
        gtk_tree_path_free(path2);
      }
      gtk_tree_path_free(path);
      start_list_store_fresh_thread();
      free(response);
    }
  }
}

static void move_down_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview1));
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview1));

  if (gtk_tree_selection_get_selected(selection, NULL, &iter))
  {
    gchar *out_gid;
    gtk_tree_model_get(model, &iter, get_tree_view_column_index_by_type(COLUMN_GID), &out_gid, -1);
    if (!out_gid)
    {
      return;
    }
    jsonrpc_response_result *response = aria2_change_position(aria2, out_gid, 1, POS_CUR);
    if (!response->successed)
    {
      show_dialog("error", response->error_message);
      free(response->error_message);
      free(response);
    }
    else
    {
      GtkTreeIter iter2;
      GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
      int i = gtk_tree_path_get_indices(path)[0];
      GtkTreePath *path2 = gtk_tree_path_new_from_indices(i + 1, -1);
      if (gtk_tree_model_get_iter(model, &iter2, path2))
      {
        gtk_list_store_swap(GTK_LIST_STORE(model), &iter, &iter2);
      }
      gtk_tree_path_free(path2);
      gtk_tree_path_free(path);
      start_list_store_fresh_thread();
      free(response);
    }
  }
}

static void move_top_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview1));
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview1));

  if (gtk_tree_selection_get_selected(selection, NULL, &iter))
  {
    gchar *out_gid;
    gtk_tree_model_get(model, &iter, get_tree_view_column_index_by_type(COLUMN_GID), &out_gid, -1);
    if (!out_gid)
    {
      return;
    }
    jsonrpc_response_result *response = aria2_change_position(aria2, out_gid, 0, POS_SET);
    if (!response->successed)
    {
      show_dialog("error", response->error_message);
      free(response->error_message);
      free(response);
    }
    else
    {
      GtkTreeIter iter2;
      GtkTreePath *path2 = gtk_tree_path_new_from_indices(0, -1);
      if (gtk_tree_model_get_iter(model, &iter2, path2))
      {
        gtk_list_store_move_before(GTK_LIST_STORE(model), &iter, &iter2);
      }
      gtk_tree_path_free(path2);
      start_list_store_fresh_thread();
      free(response);
    }
  }
}

static void move_bottom_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview1));
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview1));

  if (gtk_tree_selection_get_selected(selection, NULL, &iter))
  {
    gchar *out_gid;
    gtk_tree_model_get(model, &iter, get_tree_view_column_index_by_type(COLUMN_GID), &out_gid, -1);
    if (!out_gid)
    {
      return;
    }
    jsonrpc_response_result *response = aria2_change_position(aria2, out_gid, 0, POS_END);
    if (!response->successed)
    {
      show_dialog("error", response->error_message);
      free(response->error_message);
      free(response);
    }
    else
    {
      int num = gtk_tree_model_iter_n_children(model, NULL);
      if (num > 0)
      {
        GtkTreeIter iter2;
        GtkTreePath *path2 = gtk_tree_path_new_from_indices(num - 1, -1);
        if (gtk_tree_model_get_iter(model, &iter2, path2))
        {
          gtk_list_store_move_after(GTK_LIST_STORE(model), &iter, &iter2);
        }
        gtk_tree_path_free(path2);
      }
      start_list_store_fresh_thread();
      free(response);
    }
  }
}

static void not_implemented(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  char *text;
  text = g_strdup_printf("Action “%s” not implemented", g_action_get_name(G_ACTION(action)));
  status_message(GTK_STATUSBAR(statusbar1), text);
  g_free(text);
}

static int add_uri_task()
{
  GtkTextIter start_iter;
  GtkTextIter end_iter;
  GtkTextView *tv = GTK_TEXT_VIEW(url_textview);
  GtkTextBuffer *tb = gtk_text_view_get_buffer(tv);
  gtk_text_buffer_get_bounds(tb, &start_iter, &end_iter);
  gchar *uri = gtk_text_buffer_get_text(tb, &start_iter, &end_iter, FALSE);
  string_delete_space(uri);
  if (uri == NULL || strlen(uri) == 0)
  {
    show_dialog("error", "empty url");
    g_free(uri);
    return FALSE;
  }
  gtk_text_buffer_set_text(tb, uri, strlen(uri));
  char target[4096] = {0};
  if (!get_real_uri(uri, &target))
  {
    show_dialog("error", "url format error");
    g_free(uri);
    return FALSE;
  }

  char *uris[1];
  int uri_len = 0;

  uris[0] = target;
  uri_len++;

  char *path = gtk_label_get_label(GTK_LABEL(gtk_button_get_child(locationfilebtn)));
  if (path == NULL || strlen(path) == 0 || strcmp(path, "—") == 0)
  {
    show_dialog("error", "empty file path choosed");
    return FALSE;
  }

  key_value_pair opts[3];
  int opts_len = 0;
  opts[opts_len++] = (key_value_pair){.key = "dir", .value = cJSON_CreateString(path)};

  const char *filename = gtk_editable_get_text(GTK_EDITABLE(file_name_text));
  string_delete_space(filename);
  if (filename != NULL && strlen(filename) != 0)
  {
    opts[opts_len++] = (key_value_pair){.key = "out", .value = cJSON_CreateString(filename)};
  }

  opts[opts_len++] = (key_value_pair){
      .key = "max-connection-per-server",
      .value = cJSON_CreateNumber(gtk_spin_button_get_value(spin_conn_btn))};

  // int pos = INT32_MAX;
  int pos = 0;
  jsonrpc_response_result *response = aria2_add_uri(aria2, uris, uri_len, opts, opts_len, pos);
  g_free(uri);
  if (!response->successed)
  {
    show_dialog("error", response->error_message);
    free(response->error_message);
    free(response);
    return FALSE;
  }
  else
  {
    free(response->data_ptr);
    free(response);
    start_list_store_fresh_thread();
    return TRUE;
  }
}

static int add_btmt_task()
{
  char *file = gtk_label_get_label(GTK_LABEL(gtk_button_get_child(open_btmt_file_btn)));
  if (file == NULL || strlen(file) == 0 || strcmp(file, "—") == 0)
  {
    show_dialog("error", "empty file choosed");
    return FALSE;
  }

  char *path = gtk_label_get_label(GTK_LABEL(gtk_button_get_child(locationfilebtn)));
  if (path == NULL || strlen(path) == 0 || strcmp(path, "—") == 0)
  {
    show_dialog("error", "empty file path choosed");
    return FALSE;
  }

  key_value_pair opts[3];
  int opts_len = 0;
  opts[opts_len++] = (key_value_pair){.key = "dir", .value = cJSON_CreateString(path)};

  const char *filename = gtk_editable_get_text(GTK_EDITABLE(file_name_text));
  string_delete_space(filename);
  if (filename != NULL && strlen(filename) != 0)
  {
    opts[opts_len++] = (key_value_pair){.key = "out", .value = cJSON_CreateString(filename)};
  }

  opts[opts_len++] = (key_value_pair){
      .key = "max-connection-per-server",
      .value = cJSON_CreateNumber(gtk_spin_button_get_value(spin_conn_btn))};

  int pos = 0;
  long target_length = 0;
  char *target = b64_encode_file(file, &target_length);
  jsonrpc_response_result *response = NULL;
  if (cur_download_type == torrent)
  {
    response = aria2_add_torrent(aria2, target, NULL, 0, opts, opts_len, pos);
  }
  else
  {
    response = aria2_add_metalink(aria2, target, opts, opts_len, pos);
  }
  if (!response->successed)
  {
    show_dialog("error", response->error_message);
    free(response->error_message);
    free(response);
    return FALSE;
  }
  else
  {
    free(response->data_ptr);
    free(response);
    start_list_store_fresh_thread();
    return TRUE;
  }
}

void newdownloadialog_response_cb(GtkDialog *dialog, int response_id, gpointer user_data)
{
  gtk_widget_hide(GTK_WIDGET(dialog));
  if (response_id == GTK_RESPONSE_OK)
  {
    switch (cur_download_type)
    {
    case uri:
      add_uri_task();
      break;
    case torrent:
    case metalink:
      add_btmt_task();
      break;
    }
  }
}

static int open_btmt_file_cb_response(GtkWidget *button, GFile *file, gpointer user_data)
{
  gtk_label_set_label(GTK_LABEL(gtk_button_get_child(button)), g_file_peek_path(file));
  g_object_set_data_full(G_OBJECT(button), "file", g_object_ref(file), g_object_unref);
  return 1;
}

void open_btmt_file_cb(GtkWidget *button)
{
  char *pattern = NULL;
  switch (cur_download_type)
  {
  case torrent:
    pattern = "*.torrent";
    break;
  case metalink:
    pattern = "*.metalink";
    break;
  }
  location_file_chooser(button, GTK_FILE_CHOOSER_ACTION_OPEN, open_btmt_file_cb_response, NULL, pattern);
}

static int open_location_file_cb_response(GtkWidget *button, GFile *file, gpointer user_data)
{
  gtk_label_set_label(GTK_LABEL(gtk_button_get_child(button)), g_file_peek_path(file));
  g_object_set_data_full(G_OBJECT(button), "file", g_object_ref(file), g_object_unref);
  return 1;
}

void open_location_file_cb(GtkWidget *button)
{
  char *pattern = NULL;
  switch (cur_download_type)
  {
  case torrent:
    pattern = "*.torrent";
    break;
  case metalink:
    pattern = "*.metalink";
    break;
  }
  location_file_chooser(button, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, open_location_file_cb_response, NULL, NULL);
}

void set_font_for_display_with_pango_font_desc(GtkWindow *win)
{
  PangoFontDescription *pango_font_desc = pango_font_description_from_string("Monospace");
  const char *family = pango_font_description_get_family(pango_font_desc);
  char *css = g_strdup_printf("window,label{font-family: \"%s\";font-style: %s;font-weight:%s;font-size:%dpt;} treeview.view label{color:black;}",
                              family, "normal", "900", 15);
  GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(win));
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(provider, css, -1);
  gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
  // gtk_style_context_add_provider_for_display(gdk_display_get_default(),GTK_STYLE_PROVIDER(provider),GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

const GActionEntry win_entries[] = {
    {"add_uri", add_uri_activate, NULL, NULL, NULL},
    {"add_torrent", add_torrent_activate, NULL, NULL, NULL},
    {"add_metalink", add_metalink_activate, NULL, NULL, NULL},
    {"pause_all", pause_all_activate, NULL, NULL, NULL},
    {"unpause_all", unpause_all_activate, NULL, NULL, NULL},
    {"purge_download_result", purge_download_result_activate, NULL, NULL, NULL},
    {"remove", remove_task_activate, NULL, NULL, NULL},
    {"open_folder", open_folder_activate, NULL, NULL, NULL},
    {"unpause", unpause_task_activate, NULL, NULL, NULL},
    {"pause", pause_task_activate, NULL, NULL, NULL},
    {"move_up", move_up_activate, NULL, NULL, NULL},
    {"move_down", move_down_activate, NULL, NULL, NULL},
    {"move_top", move_top_activate, NULL, NULL, NULL},
    {"move_bottom", move_bottom_activate, NULL, NULL, NULL},
    {"quit", quit_activate, NULL, NULL, NULL},
    {"about", about_activate, NULL, NULL, NULL}};

void start_list_store_fresh_thread()
{
  if (aria2->paused == TRUE)
  {
    aria2->paused = FALSE;
    pthread_t pthread;
    pthread_create(&pthread, NULL, list_store_fresh_thread, NULL);
  }
}

void *list_store_fresh_thread(void *ptr)
{
  while (aria2->paused == FALSE)
  {
    jsonrpc_response_result *response = aria2_tell_all(aria2);
    if (!response->successed)
    {
      aria2->paused = TRUE;
      show_dialog("error", response->error_message);
      free(response->error_message);
      free(response);
      break;
    }

    aria2_item_all *all_object = (aria2_item_all *)response->data_ptr;
    int row_size = 0;
    if (all_object != NULL)
    {
      row_size += all_object->item_active->items_size;
      // row_size += all_object->item_waiting->items_size;
    }
    if (row_size == 0)
    {
      aria2->paused = TRUE;
    }
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview1));

    for (int i = 0; i < all_object->item_active->items_size; i++)
    {
      add_or_update_item(all_object->item_active->items[i], model);
    }

    for (int i = 0; i < all_object->item_waiting->items_size; i++)
    {
      add_or_update_item(all_object->item_waiting->items[i], model);
    }

    for (int i = 0; i < all_object->item_stopped->items_size; i++)
    {
      add_or_update_item(all_object->item_stopped->items[i], model);
    }
    aria2_item_all_free(all_object);
    Sleep(500);
  }
  return NULL;
}

static void create_column_array()
{
  int column_count = G_N_ELEMENTS(columns);
  column_type_array = g_array_sized_new(FALSE, FALSE, sizeof(GType), 1);
  for (int i = 0; i < column_count; i++)
  {
    GType cur_type;
    tree_view_column_object col = columns[i];
    switch (col.column_type)
    {
    case Progress:
      cur_type = G_TYPE_INT;
      break;
    case Text:
      cur_type = G_TYPE_STRING;
      break;
    case Integer:
      cur_type = G_TYPE_INT64;
      break;
    }
    g_array_append_vals(column_type_array, &cur_type, 1);
  }
}

static GtkTreeModel *create_items_model()
{
  /* create list store */
  GtkListStore *model = gtk_list_store_newv(NUM_COLUMNS, (GType *)(column_type_array->data));
  return GTK_TREE_MODEL(model);
}

static int add_or_update_item(aria2_item *foo, GtkTreeModel *model)
{
  GtkTreeIter iter;
  GtkTreeView *treeview = GTK_TREE_VIEW(treeview1);
  GtkTreePath *path = get_tree_path_by_gid(treeview, foo->gid);
  if (path)
  {
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_path_free(path);
  }
  else
  {
    gtk_list_store_insert(GTK_LIST_STORE(model), &iter, -1);
  }

  gint progress = 0;
  if (foo->totalLength > 0)
  {
    progress = (double)foo->completedLength / foo->totalLength * 100;
  }
  char *downloadSpeedText = string_format("%lldb/s", foo->downloadSpeed);
  char *name = foo->gid;
  if (foo->bittorrent != NULL && foo->bittorrent->info)
  {
    name = foo->bittorrent->info->name;
  }
  else if (foo->files_size > 0)
  {
    name = get_file_name(foo->files[0]->path);
  }
  /* Set the data for the new row */
  gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                     get_tree_view_column_index_by_type(COLUMN_GID),
                     foo->gid,
                     get_tree_view_column_index_by_type(COLUMN_NAME),
                     name,
                     get_tree_view_column_index_by_type(COLUMN_COMPLETEED_LENGTH),
                     foo->completedLength,
                     get_tree_view_column_index_by_type(COLUMN_TOTAL_LENGTH),
                     foo->totalLength,
                     get_tree_view_column_index_by_type(COLUMN_COMPLETED_PROGRESS),
                     progress,
                     get_tree_view_column_index_by_type(COLUMN_DOWNLOAD_SPEED),
                     downloadSpeedText,
                     get_tree_view_column_index_by_type(COLUMN_STATUS),
                     foo->status,
                     get_tree_view_column_index_by_type(COLUMN_ERRORMSG),
                     foo->errorMessage,
                     -1);
  return TRUE;
}

static GtkTreePath *get_tree_path_by_gid(GtkTreeView *treeview, gchar *gid)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model(treeview);
  gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
  while (valid)
  {
    gchar *out_gid;
    gtk_tree_model_get(model, &iter, get_tree_view_column_index_by_type(COLUMN_GID), &out_gid, -1);
    if (out_gid && strcmp(out_gid, gid) == 0)
    {
      GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
      return path;
    }
    valid = gtk_tree_model_iter_next(model, &iter);
  }
  return NULL;
}

static void add_columns(GtkTreeView *treeview)
{
  GtkCellRenderer *renderer;

  int column_count = G_N_ELEMENTS(columns);
  for (int i = 0; i < column_count; i++)
  {
    tree_view_column_object col = columns[i];
    GtkTreeViewColumn *column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, col.title);
    gtk_tree_view_column_set_min_width(column, col.min_width);
    gtk_tree_view_column_set_visible(column, col.visible);
    gtk_tree_view_column_set_resizable(column, col.resizable);

    switch (col.column_type)
    {
    case Progress:
      renderer = gtk_cell_renderer_progress_new();
      gtk_tree_view_column_pack_start(column, renderer, TRUE);
      gtk_tree_view_column_set_attributes(column, renderer, "value", i, NULL);
      break;
    case Text:
      renderer = gtk_cell_renderer_text_new();
      gtk_tree_view_column_pack_start(column, renderer, TRUE);
      gtk_tree_view_column_set_attributes(column, renderer, "text", i, NULL);
      break;
    case Integer:
      renderer = gtk_cell_renderer_text_new();
      gtk_tree_view_column_pack_start(column, renderer, TRUE);
      gtk_tree_view_column_set_attributes(column, renderer, "text", i, NULL);
      break;
    }
    gtk_tree_view_append_column(treeview, column);
  }
}

static void on_tree_view_right_btn_released(GtkGestureClick *gesture,
                                            int n_press,
                                            double x,
                                            double y,
                                            GtkWidget *widget)
{
  g_print("on_tree_view_right_btn_released() called\n");

  GMenu *menu = g_menu_new();
  g_menu_append(menu, "暂停所有任务", "win.pause_all");
  g_menu_append(menu, "继续所有任务", "win.unpause_all");
  g_menu_append(menu, "删除已完成或者错误的任务", "win.purge_download_result");
  g_menu_append(menu, "删除条目", "win.remove");
  g_menu_append(menu, "打开保存文件夹", "win.open_folder");
  g_menu_append(menu, "开始", "win.unpause");
  g_menu_append(menu, "暂停", "win.pause");
  g_menu_append(menu, "向上", "win.move_up");
  g_menu_append(menu, "向下", "win.move_down");
  g_menu_append(menu, "移到顶部", "win.move_top");
  g_menu_append(menu, "移到底部", "win.move_bottom");

  GtkWidget *popmenu = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
  gtk_popover_set_has_arrow(GTK_POPOVER(popmenu), FALSE);
  gtk_widget_set_parent(popmenu, data_box);
  gtk_popover_set_pointing_to(GTK_POPOVER(popmenu), &(const GdkRectangle){x, y, 1, 1});
  gtk_popover_popup(GTK_POPOVER(popmenu));
}

GtkWidget *do_builder(GtkApplication *app)
{

  GActionGroup *actions;
  if (!window)
  {
    GtkBuilder *builder;
    GtkTreeModel *items_model;

    builder = gtk_builder_new_from_resource("/com/henry/app/menuwindow.ui");

    window = GTK_WIDGET(gtk_builder_get_object(builder, "window1"));
    set_font_for_display_with_pango_font_desc(GTK_WINDOW(window));
    gtk_window_set_application(GTK_WINDOW(window), app);

    g_object_add_weak_pointer(G_OBJECT(window), (gpointer *)&window);
    actions = (GActionGroup *)g_simple_action_group_new();
    g_action_map_add_action_entries(G_ACTION_MAP(actions),
                                    win_entries, G_N_ELEMENTS(win_entries),
                                    window);
    gtk_widget_insert_action_group(window, "win", actions);

    struct
    {
      const char *action;
      const char *accels[2];
    } action_accels[] = {
        {"win.add_uri", {"<Control>n", NULL}},
        {"win.quit", {"<Control>q", NULL}}};
    for (int i = 0; i < G_N_ELEMENTS(action_accels); i++)
    {
      gtk_application_set_accels_for_action(GTK_APPLICATION(app), action_accels[i].action, action_accels[i].accels);
    }

    aboutdialog1 = GTK_WIDGET(gtk_builder_get_object(builder, "aboutdialog1"));
    statusbar1 = GTK_WIDGET(gtk_builder_get_object(builder, "statusbar1"));
    newdownloadialog1 = GTK_WIDGET(gtk_builder_get_object(builder, "newdownloadialog1"));
    data_box = GTK_WIDGET(gtk_builder_get_object(builder, "data_box"));
    scrolledwindow1 = GTK_WIDGET(gtk_builder_get_object(builder, "scrolledwindow1"));
    newdownloa_grid = GTK_WIDGET(gtk_builder_get_object(builder, "newdownloa_grid"));
    gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW(scrolledwindow1), TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow1),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    url_textview = GTK_WIDGET(gtk_builder_get_object(builder, "url_textview"));
    gtk_widget_set_size_request(url_textview, 800, 200);
    open_btmt_file_btn = GTK_WIDGET(gtk_builder_get_object(builder, "open_btmt_file_btn"));
    gtk_widget_set_size_request(open_btmt_file_btn, 800, 50);
    locationfilebtn = GTK_WIDGET(gtk_builder_get_object(builder, "location_file"));
    gtk_widget_set_size_request(locationfilebtn, 800, 50);
    file_name_text = GTK_WIDGET(gtk_builder_get_object(builder, "file_name_text"));
    gtk_widget_set_size_request(file_name_text, 800, 50);
    spin_conn_btn = GTK_WIDGET(gtk_builder_get_object(builder, "spin_conn_btn"));
    gtk_widget_set_size_request(spin_conn_btn, 800, 50);

    /* create array */
    create_column_array();
    /* create models */
    items_model = create_items_model();
    /* create tree view */
    treeview1 = gtk_tree_view_new_with_model(items_model);
    gtk_widget_set_vexpand(treeview1, TRUE);
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview1)), GTK_SELECTION_SINGLE);
    add_columns(GTK_TREE_VIEW(treeview1));
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolledwindow1), treeview1);
    g_object_unref(items_model);

    GtkGesture *gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), GDK_BUTTON_SECONDARY);
    g_signal_connect(gesture, "released", G_CALLBACK(on_tree_view_right_btn_released), treeview1);
    gtk_widget_add_controller(treeview1, GTK_EVENT_CONTROLLER(gesture));
    g_object_unref(builder);
  }

  aria2 = aria2_new("ws://127.0.0.1:6800/jsonrpc", NULL);
  aria2_launch(aria2);

  int try = 10;
  while (try > 0 && aria2->init_completed == FALSE)
  {
    try--;
    Sleep(100);
  }

  start_list_store_fresh_thread();

  gtk_tray_ico_thread(window, aria2);

  gtk_widget_show(window);
  return window;
}