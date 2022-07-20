#include "menuwindow.h"
#include "aria2.h"

static GtkWidget *aboutdialog1;
static GtkWidget *statusbar1;
static GtkWidget *newdownloadialog1;
static GtkWidget *url_textview;
static GtkWidget *treeview1;

static void start_list_store_fresh_thread();
static void *list_store_fresh_thread(void *ptr);
static void list_store_init(GtkTreeView *treeview);
static void list_store_insert_row(GtkListStore *liststore, GValue values[]);

static void quit_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GtkWidget *window = user_data;
  aria2_shutdown();
  gtk_window_destroy(GTK_WINDOW(window));
}

static void about_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  gtk_window_present(GTK_WINDOW(aboutdialog1));
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

static void help_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  status_message(GTK_STATUSBAR(statusbar1), "Help not available");
}

static void new_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  gtk_widget_show(newdownloadialog1);
}

static void copy_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
}

static void not_implemented(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  char *text;
  text = g_strdup_printf("Action “%s” not implemented", g_action_get_name(G_ACTION(action)));
  status_message(GTK_STATUSBAR(statusbar1), text);
  g_free(text);
}

void preferencedialog_response_cb(GtkDialog *dialog, int response_id, gpointer user_data)
{
  GtkTextIter start_iter;
  GtkTextIter end_iter;
  gchar *contents;

  gtk_widget_hide(GTK_WIDGET(dialog));

  GtkTextView *tv = GTK_TEXT_VIEW(url_textview);
  GtkTextBuffer *tb = gtk_text_view_get_buffer(tv);
  gtk_text_buffer_get_bounds(tb, &start_iter, &end_iter);
  contents = gtk_text_buffer_get_text(tb, &start_iter, &end_iter, FALSE);
  if (response_id == GTK_RESPONSE_OK)
  {
    char **uris = (char **)malloc(sizeof(char **));
    *uris = strdup(contents);
    int uri_len = 1;
    key_value_pair opts[1] = {{.key = "dir", .value = cJSON_CreateString("C:\\Users\\Henry\\Desktop")}};
    int opts_len = 1;
    int pos = 0;
    jsonrpc_response_object *response = aria2_add_uri(uris, uri_len, opts, opts_len, pos);
    if (aria2_is_paused() == TRUE)
    {
      aria2_set_paused(FALSE);
      start_list_store_fresh_thread();
    }
    free(response);
    free(*uris);
    free(uris);
  }
}

void set_font_for_display_with_pango_font_desc(GtkWindow *win)
{
  PangoFontDescription *pango_font_desc = pango_font_description_from_string("Monospace");
  const char *family = pango_font_description_get_family(pango_font_desc);
  char *css = g_strdup_printf("window,label{font-family: \"%s\";font-style: %s;font-weight:%s;font-size:%dpt;color:black;}",
                              family, "normal", "900", 15);
  GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(win));
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(provider, css, -1);
  gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
  // gtk_style_context_add_provider_for_display(gdk_display_get_default(),GTK_STYLE_PROVIDER(provider),GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

const GActionEntry win_entries[] = {
    {"new", new_activate, NULL, NULL, NULL},
    {"open", not_implemented, NULL, NULL, NULL},
    {"save", not_implemented, NULL, NULL, NULL},
    {"save-as", not_implemented, NULL, NULL, NULL},
    {"copy", copy_activate, NULL, NULL, NULL},
    {"cut", not_implemented, NULL, NULL, NULL},
    {"paste", not_implemented, NULL, NULL, NULL},
    {"quit", quit_activate, NULL, NULL, NULL},
    {"about", about_activate, NULL, NULL, NULL},
    {"help", help_activate, NULL, NULL, NULL}};

const tree_view_column_object columns[5] = {
    {.column_type = Text, .min_width = 200, .resizable = TRUE, .title = "名称", .visible = TRUE},
    {.column_type = Integer, .min_width = 100, .resizable = TRUE, .title = "完成大小", .visible = TRUE},
    {.column_type = Integer, .min_width = 100, .resizable = TRUE, .title = "总计大小", .visible = TRUE},
    {.column_type = Progress, .min_width = 200, .resizable = TRUE, .title = "完成进度", .visible = TRUE},
    {.column_type = Text, .min_width = 100, .resizable = TRUE, .title = "下载速度", .visible = TRUE}};

static void list_store_insert_row(GtkListStore *store, GValue values[])
{
  GtkTreeIter tree_iter;
  int column_count = G_N_ELEMENTS(columns);
  gtk_list_store_append(store, &tree_iter);
  for (int i = 0; i < column_count; i++)
  {
    tree_view_column_object col = columns[i];

    switch (col.column_type)
    {
    case Progress:
      gint valp = g_value_get_int(&values[i]);
      fprintf(stderr, "column:%d,value:%d\n", i, valp);
      break;
    case Integer:
      gint64 vali = g_value_get_int64(&values[i]);
      fprintf(stderr, "column:%d,value:%lld\n", i, vali);
      break;
    case Text:
      const char *valt = g_value_get_string(&values[i]);
      fprintf(stderr, "column:%d,value:%s\n", i, valt);
      break;
    }
    gtk_list_store_set_value(store, &tree_iter, i, &values[i]);
  }
}

// void traverse_list_store(GtkListStore *liststore)
// {
//   GtkTreeIter iter;
//   gboolean valid;

//   /* Get first row in list store */
//   valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(liststore), &iter);

//   while (valid)
//   {
//     gtk_list_store_remove(liststore, &iter);
//     /* Make iter point to the next row in the list store */
//     valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(liststore), &iter);
//   }
// }

static int get_gvalues_format(GValue values[], ...)
{
  va_list arg_list;
  va_start(arg_list, values);

  int column_count = G_N_ELEMENTS(columns);
  for (int i = 0; i < column_count; i++)
  {
    tree_view_column_object col = columns[i];
    switch (col.column_type)
    {
    case Integer:
      GValue gvi = G_VALUE_INIT;
      g_value_init(&gvi, G_TYPE_INT64);
      g_value_set_int64(&gvi, va_arg(arg_list, gint64));
      values[i] = gvi;
      break;
    case Progress:
      GValue gvp = G_VALUE_INIT;
      g_value_init(&gvp, G_TYPE_INT);
      g_value_set_int(&gvp, va_arg(arg_list, gint));
      values[i] = gvp;
      break;
    case Text:
      // char t[20];
      // get_current_time_string(t);
      GValue gvt = G_VALUE_INIT;
      g_value_init(&gvt, G_TYPE_STRING);
      g_value_set_string(&gvt, va_arg(arg_list, gchar *));
      values[i] = gvt;
      break;
    }
  }
  va_end(arg_list);
  return column_count;
}

static void start_list_store_fresh_thread()
{
  pthread_t pthread;
  pthread_create(&pthread, NULL, list_store_fresh_thread, NULL);
}

static void *list_store_fresh_thread(void *ptr)
{
  int column_count = G_N_ELEMENTS(columns);
  GArray *type_array = g_array_new(TRUE, FALSE, sizeof(GType));
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
    g_array_append_val(type_array, cur_type);
  }

  aria2_set_paused(FALSE);
  while (aria2_is_paused() == FALSE)
  {
    GtkListStore *liststore = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview1)));
    if (liststore)
    {
      g_object_unref(liststore);
      liststore = NULL;
    }
    liststore = gtk_list_store_newv(column_count, (GType *)(type_array->data));
    jsonrpc_response_object *repsonse = aria2_tell_active();
    cJSON *resultArray = repsonse->result;
    int result_array_size = cJSON_GetArraySize(resultArray);
    if (result_array_size == 0)
    {
      aria2_set_paused(TRUE);
      break;
    }

    for (int i = 0; i < result_array_size; i++)
    {
      cJSON *result = cJSON_GetArrayItem(resultArray, i);
      gchar *name = cJSON_GetStringValue(cJSON_GetObjectItem(result, "gid"));
      gint64 completedLength = atoll(cJSON_GetStringValue(cJSON_GetObjectItem(result, "completedLength")));
      gint64 totalLength = atoll(cJSON_GetStringValue(cJSON_GetObjectItem(result, "totalLength")));
      gint progress = 0;
      if (totalLength > 0)
      {
        progress = (double)completedLength / totalLength * 100;
      }
      char *downloadSpeed = string_format("%db/s", atoi(cJSON_GetStringValue(cJSON_GetObjectItem(result, "downloadSpeed"))));
      GValue values[column_count];
      get_gvalues_format(values, name, completedLength, totalLength, progress, downloadSpeed);
      list_store_insert_row(liststore, values);
      free(downloadSpeed);
    }
    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview1), GTK_TREE_MODEL(liststore));
    cJSON_Delete(resultArray);
    free(repsonse);
    Sleep(1000);
  }
  g_array_free(type_array, TRUE);
  return NULL;
}

static void list_store_init(GtkTreeView *treeview)
{
  GtkCellRenderer *renderer;
  // GtkListStore *liststore;
  int column_count = G_N_ELEMENTS(columns);
  GArray *type_array = g_array_new(TRUE, FALSE, sizeof(GType));
  for (int i = 0; i < column_count; i++)
  {
    GType cur_type;
    tree_view_column_object col = columns[i];
    GtkTreeViewColumn *column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, col.title);
    gtk_tree_view_column_set_min_width(column, col.min_width);
    gtk_tree_view_column_set_visible(column, col.visible);
    gtk_tree_view_column_set_resizable(column, col.resizable);

    switch (col.column_type)
    {
    case Progress:
      cur_type = G_TYPE_INT;
      renderer = gtk_cell_renderer_progress_new();
      gtk_tree_view_column_pack_start(column, renderer, TRUE);
      gtk_tree_view_column_set_attributes(column, renderer, "value", i, NULL);
      break;
    case Text:
      cur_type = G_TYPE_STRING;
      renderer = gtk_cell_renderer_text_new();
      gtk_tree_view_column_pack_start(column, renderer, TRUE);
      gtk_tree_view_column_set_attributes(column, renderer, "text", i, NULL);
      break;
    case Integer:
      cur_type = G_TYPE_INT64;
      renderer = gtk_cell_renderer_text_new();
      gtk_tree_view_column_pack_start(column, renderer, TRUE);
      gtk_tree_view_column_set_attributes(column, renderer, "text", i, NULL);
      break;
    }
    gtk_tree_view_append_column(treeview, column);
    g_array_append_val(type_array, cur_type);
  }
  // liststore = gtk_list_store_newv(column_count, (GType *)(type_array->data));
  // gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(liststore));
  g_array_free(type_array, TRUE);
}

GtkWidget *do_builder(GtkApplication *app)
{
  static GtkWidget *window = NULL;
  GActionGroup *actions;

  if (!window)
  {
    GtkBuilder *builder;

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
        {"win.new", {"<Control>n", NULL}},
        {"win.open", {"<Control>o", NULL}},
        {"win.save", {"<Control>s", NULL}},
        {"win.save-as", {"<Shift><Control>s", NULL}},
        {"win.quit", {"<Control>q", NULL}}};
    for (int i = 0; i < G_N_ELEMENTS(action_accels); i++)
    {
      gtk_application_set_accels_for_action(GTK_APPLICATION(app), action_accels[i].action, action_accels[i].accels);
    }

    aboutdialog1 = GTK_WIDGET(gtk_builder_get_object(builder, "aboutdialog1"));
    statusbar1 = GTK_WIDGET(gtk_builder_get_object(builder, "statusbar1"));
    newdownloadialog1 = GTK_WIDGET(gtk_builder_get_object(builder, "newdownloadialog1"));
    url_textview = GTK_WIDGET(gtk_builder_get_object(builder, "url_textview"));
    treeview1 = GTK_WIDGET(gtk_builder_get_object(builder, "treeview1"));
    list_store_init(GTK_TREE_VIEW(treeview1));
    g_object_unref(builder);
  }

  aria2_new("ws://127.0.0.1:6800/jsonrpc", NULL, NULL, NULL);
  aria2_launch();

  while (aria2_init_completed() == FALSE)
  {
    Sleep(500);
  }

  start_list_store_fresh_thread();

  if (!gtk_widget_get_visible(window))
    gtk_widget_show(window);
  else
    gtk_window_destroy(GTK_WINDOW(window));

  return window;
}
