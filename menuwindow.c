#include "menuwindow.h"
#include "aria2.h"

#define SHOW_WINDOW 12
#define APP_QUIT 13

typedef struct
{
  gint index;
  gchar *gid;
  gchar *name;
  gint64 completed_length;
  gint64 total_length;
  gint progress;
  gchar *download_speed_text;
  gint64 version;
} Item;

enum
{
  COLUMN_GID,
  COLUMN_NAME,
  COLUMN_COMPLETEED_LENGTH,
  COLUMN_TOTAL_LENGTH,
  COLUMN_COMPLETED_PROGRESS,
  COLUMN_DOWNLOAD_SPEED,
  NUM_COLUMNS
};
static GArray *data_array = NULL;
static gint64 data_version = 0;
static GArray *column_type_array = NULL;

static gint cmp_item_gid(gconstpointer a, gconstpointer b)
{
  const Item *_a = a;
  const Item *_b = b;
  return strcmp(_a->gid, _b->gid);
}

static gint cmp_item_index(gconstpointer a, gconstpointer b)
{
  const Item *_a = a;
  const Item *_b = b;
  return _a->index - _b->index;
}

LPCTSTR szAppClassName = L"aria2-gtk-ui";
LPCTSTR szAppWindowName = L"aria2-gtk-ui";
HMENU hmenu; //菜单句柄

static GtkWidget *window;
static GtkWidget *aboutdialog1;
static GtkWidget *statusbar1;
static GtkWidget *newdownloadialog1;
static GtkWidget *url_textview;
static GtkWidget *treeview1;
static GtkWidget *scrolledwindow1;
static aria2_object *aria2;

static void start_list_store_fresh_thread();
static void *list_store_fresh_thread(void *ptr);
static int add_or_update_item(Item foo);
static void remove_item(GtkWidget *widget, gpointer data);
static void remove_all_item();
static void remove_unused_item();

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  NOTIFYICONDATAW nid;
  UINT WM_TASKBARCREATED;
  POINT pt; //用于接收鼠标坐标
  int xx;   //用于接收菜单选项返回值

  // 不要修改TaskbarCreated，这是系统任务栏自定义的消息
  WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");
  switch (message)
  {
  case WM_CREATE: //窗口创建时候的消息.
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = 0;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER;
    nid.hIcon = (HICON)LoadImageW(NULL, L"app.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    lstrcpyW(nid.szTip, szAppClassName);
    Shell_NotifyIconW(NIM_ADD, &nid);
    hmenu = CreatePopupMenu();                                  //生成菜单
    AppendMenuW(hmenu, MF_STRING, SHOW_WINDOW, L"show window"); //为菜单添加两个选项
    AppendMenuW(hmenu, MF_STRING, APP_QUIT, L"quit");
    break;
  case WM_USER: //连续使用该程序时候的消息.
    if (lParam == WM_RBUTTONDOWN)
    {
      GetCursorPos(&pt);         //取鼠标坐标
      SetForegroundWindow(hwnd); //解决在菜单外单击左键菜单不消失的问题
      // EnableMenuItem(hmenu, SHOW_WINDOW, MF_GRAYED);                           //让菜单中的某一项变灰
      xx = TrackPopupMenu(hmenu, TPM_RETURNCMD, pt.x, pt.y, NULL, hwnd, NULL); //显示菜单并获取选项ID
      if (xx == SHOW_WINDOW)
      {
        gtk_widget_show(window);
      }
      if (xx == APP_QUIT)
      {
        aria2_shutdown(aria2);
        gtk_window_destroy(GTK_WINDOW(window));
        SendMessageW(hwnd, WM_CLOSE, wParam, lParam);
      }
      if (xx == 0)
      {
        PostMessageW(hwnd, WM_LBUTTONDOWN, NULL, NULL);
      }
    }
    break;
  case WM_DESTROY: //窗口销毁时候的消息.
    Shell_NotifyIconW(NIM_DELETE, &nid);
    PostQuitMessage(0);
    break;
  default:
    /*
     * 防止当Explorer.exe 崩溃以后，程序在系统系统托盘中的图标就消失
     *
     * 原理：Explorer.exe 重新载入后会重建系统任务栏。当系统任务栏建立的时候会向系统内所有
     * 注册接收TaskbarCreated 消息的顶级窗口发送一条消息，我们只需要捕捉这个消息，并重建系
     * 统托盘的图标即可。
     */
    if (message == WM_TASKBARCREATED)
      SendMessageW(hwnd, WM_CREATE, wParam, lParam);
    break;
  }
  return DefWindowProcW(hwnd, message, wParam, lParam);
}

void *gtk_set_tray_ico(void *ptr)
{
  HWND hwnd;
  MSG msg;
  WNDCLASS wndclass;
  HINSTANCE hInstance = GetModuleHandle(NULL);

  HWND handle = FindWindowW(NULL, szAppWindowName);
  if (handle != NULL)
  {
    MessageBoxW(NULL, L"Application is already running", szAppClassName, MB_ICONERROR);
    return 0;
  }

  wndclass.style = CS_HREDRAW | CS_VREDRAW;
  wndclass.lpfnWndProc = WndProc;
  wndclass.cbClsExtra = 0;
  wndclass.cbWndExtra = 0;
  wndclass.hInstance = hInstance;
  wndclass.hIcon = LoadIconW(NULL, IDI_APPLICATION);
  wndclass.hCursor = LoadCursorW(NULL, IDC_ARROW);
  wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
  wndclass.lpszMenuName = NULL;
  wndclass.lpszClassName = szAppClassName;

  if (!RegisterClass(&wndclass))
  {
    MessageBoxW(NULL, L"This program requires Windows NT!", szAppClassName, MB_ICONERROR);
    return 0;
  }

  // 此处使用WS_EX_TOOLWINDOW 属性来隐藏显示在任务栏上的窗口程序按钮
  hwnd = CreateWindowEx(WS_EX_TOOLWINDOW,
                        szAppClassName, szAppWindowName,
                        WS_POPUP,
                        CW_USEDEFAULT,
                        CW_USEDEFAULT,
                        CW_USEDEFAULT,
                        CW_USEDEFAULT,
                        NULL, NULL, hInstance, NULL);

  ShowWindow(hwnd, 0);
  UpdateWindow(hwnd);
  //消息循环
  while (GetMessage(&msg, NULL, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return NULL;
}

static void quit_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GtkWidget *window = user_data;
  aria2_shutdown(aria2);
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
  remove_item(NULL, GTK_TREE_VIEW(treeview1));
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
    jsonrpc_response_object *response = aria2_add_uri(aria2, uris, uri_len, opts, opts_len, pos);
    start_list_store_fresh_thread();
    jsonrpc_response_object_free(response);
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
    jsonrpc_response_object *repsonse = aria2_tell_active(aria2);
    if (!repsonse)
    {
      continue;
    }
    cJSON *resultArray = repsonse->result;
    int result_array_size = cJSON_GetArraySize(resultArray);
    if (result_array_size == 0)
    {
      aria2->paused = TRUE;
    }
    for (int i = 0; i < result_array_size; i++)
    {
      cJSON *result = cJSON_GetArrayItem(resultArray, i);
      gchar *gid = g_strdup(cJSON_GetStringValue(cJSON_GetObjectItem(result, "gid")));
      gchar *name = gid;
      gint64 completedLength = atoll(cJSON_GetStringValue(cJSON_GetObjectItem(result, "completedLength")));
      gint64 totalLength = atoll(cJSON_GetStringValue(cJSON_GetObjectItem(result, "totalLength")));
      gint progress = 0;
      if (totalLength > 0)
      {
        progress = (double)completedLength / totalLength * 100;
      }
      char *downloadSpeed = string_format("%db/s", atoi(cJSON_GetStringValue(cJSON_GetObjectItem(result, "downloadSpeed"))));
      Item foo = {.gid = gid,
                  .name = name,
                  .completed_length = completedLength,
                  .total_length = totalLength,
                  .progress = progress,
                  .download_speed_text = downloadSpeed,
                  .version = data_version};
      add_or_update_item(foo);
    }
    remove_unused_item();
    jsonrpc_response_object_free(repsonse);
    Sleep(500);
    data_version++;
  }
  return NULL;
}

const tree_view_column_object columns[6] = {
    {.column_type = Text, .min_width = 200, .resizable = TRUE, .title = "GID", .visible = TRUE},
    {.column_type = Text, .min_width = 200, .resizable = TRUE, .title = "名称", .visible = TRUE},
    {.column_type = Integer, .min_width = 100, .resizable = TRUE, .title = "完成大小", .visible = TRUE},
    {.column_type = Integer, .min_width = 100, .resizable = TRUE, .title = "总计大小", .visible = TRUE},
    {.column_type = Progress, .min_width = 200, .resizable = TRUE, .title = "完成进度", .visible = TRUE},
    {.column_type = Text, .min_width = 100, .resizable = TRUE, .title = "下载速度", .visible = TRUE}};

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

  /* create array */
  data_array = g_array_sized_new(FALSE, FALSE, sizeof(Item), 1);
}

static GtkTreeModel *create_items_model()
{
  int i = 0;
  GtkListStore *model;
  GtkTreeIter iter;

  /* create list store */
  model = gtk_list_store_newv(NUM_COLUMNS, (GType *)(column_type_array->data));

  /* add items */
  for (i = 0; i < data_array->len; i++)
  {
    gtk_list_store_append(model, &iter);

    gtk_list_store_set(model, &iter,
                       COLUMN_GID,
                       g_array_index(data_array, Item, i).gid,
                       COLUMN_NAME,
                       g_array_index(data_array, Item, i).name,
                       COLUMN_COMPLETEED_LENGTH,
                       g_array_index(data_array, Item, i).completed_length,
                       COLUMN_TOTAL_LENGTH,
                       g_array_index(data_array, Item, i).total_length,
                       COLUMN_COMPLETED_PROGRESS,
                       g_array_index(data_array, Item, i).progress,
                       COLUMN_DOWNLOAD_SPEED,
                       g_array_index(data_array, Item, i).download_speed_text,
                       -1);
  }

  return GTK_TREE_MODEL(model);
}

int add_or_update_item(Item foo)
{
  GtkTreeIter iter;
  GtkTreeView *treeview = GTK_TREE_VIEW(treeview1);
  GtkTreeModel *model = gtk_tree_view_get_model(treeview);

  guint matched_index;
  gboolean result = g_array_binary_search(data_array, &foo, cmp_item_gid, &matched_index);
  if (result)
  {
    foo.index = g_array_index(data_array, Item, matched_index).index;
    g_array_index(data_array, Item, matched_index) = foo;
    GtkTreePath *path = gtk_tree_path_new_from_indices(matched_index, -1);
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_path_free(path);
  }
  else
  {
    gtk_list_store_insert(GTK_LIST_STORE(model), &iter, -1);
    GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
    foo.index = gtk_tree_path_get_indices(path)[0];
    g_array_append_vals(data_array, &foo, 1);
    g_array_sort(data_array, cmp_item_gid);
  }

  /* Set the data for the new row */
  gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                     COLUMN_GID,
                     foo.gid,
                     COLUMN_NAME,
                     foo.name,
                     COLUMN_COMPLETEED_LENGTH,
                     foo.completed_length,
                     COLUMN_TOTAL_LENGTH,
                     foo.total_length,
                     COLUMN_COMPLETED_PROGRESS,
                     foo.progress,
                     COLUMN_DOWNLOAD_SPEED,
                     foo.download_speed_text,
                     -1);
  return TRUE;
}

static void remove_item(GtkWidget *widget, gpointer data)
{
  GtkTreeIter iter;
  GtkTreeView *treeview = (GtkTreeView *)data;
  GtkTreeModel *model = gtk_tree_view_get_model(treeview);
  GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);

  if (gtk_tree_selection_get_selected(selection, NULL, &iter))
  {
    int i;
    GtkTreePath *path;

    path = gtk_tree_model_get_path(model, &iter);
    i = gtk_tree_path_get_indices(path)[0];
    gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

    g_array_remove_index(data_array, i);

    gtk_tree_path_free(path);
  }
}

static void remove_unused_item()
{
  GtkTreeIter iter;
  GtkTreeView *treeview = GTK_TREE_VIEW(treeview1);
  GtkTreeModel *model = gtk_tree_view_get_model(treeview);

  gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
  while (valid)
  {
    gchar *gid;
    guint matched_index;
    gtk_tree_model_get(model, &iter, COLUMN_GID, &gid, -1);
    gboolean result = g_array_binary_search(data_array, &(Item){.gid = gid}, cmp_item_gid, &matched_index);
    if (result)
    {
      if (g_array_index(data_array, Item, matched_index).version != data_version)
      {
        gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
        g_array_remove_index(data_array, matched_index);
      }
    }
    valid = gtk_tree_model_iter_next(model, &iter);
  }
}

void remove_all_item(GtkTreeView *treeview)
{

  GtkListStore *liststore = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview)));
  if (liststore)
  {
    g_object_unref(liststore);
    liststore = NULL;
  }
  gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), NULL);
  g_free(liststore);
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
    scrolledwindow1 = GTK_WIDGET(gtk_builder_get_object(builder, "scrolledwindow1"));
    gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW(scrolledwindow1), TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow1),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    /* create array */
    create_column_array();
    /* create models */
    items_model = create_items_model();
    /* create tree view */
    treeview1 = gtk_tree_view_new_with_model(items_model);
    gtk_widget_set_vexpand(treeview1, TRUE);
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview1)), GTK_SELECTION_SINGLE);
    add_columns(GTK_TREE_VIEW(treeview1));
    g_object_unref(items_model);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolledwindow1), treeview1);

    g_object_unref(builder);
  }

  aria2 = aria2_new("ws://127.0.0.1:6800/jsonrpc", NULL);
  aria2_launch(aria2);

  while (aria2->init_completed == FALSE)
  {
    Sleep(500);
  }

  start_list_store_fresh_thread();

  pthread_t pthread_ico;
  pthread_create(&pthread_ico, NULL, gtk_set_tray_ico, NULL);
  gtk_widget_show(window);
  return window;
}