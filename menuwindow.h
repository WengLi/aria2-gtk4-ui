#include <gtk/gtk.h>
#include "utilities.h"
#include "aria2.h"

typedef struct
{
  int (*cb)(GtkWidget *, GFile *, gpointer);
  GtkWidget *widget;
  gpointer data;
} location_file_args;

typedef enum
{
  uri,
  torrent,
  metalink
} download_type;

typedef enum _tree_view_column_type
{
  Text,
  Integer,
  Progress
} tree_view_column_type;

typedef enum
{
  COLUMN_GID,
  COLUMN_NAME,
  COLUMN_COMPLETEED_LENGTH,
  COLUMN_TOTAL_LENGTH,
  COLUMN_COMPLETED_PROGRESS,
  COLUMN_DOWNLOAD_SPEED,
  COLUMN_STATUS,
  COLUMN_ERRORMSG,
  NUM_COLUMNS
} tree_view_column_bound_type;

typedef struct _tree_view_column_object
{
  tree_view_column_type column_type;
  gboolean visible;
  char *title;
  gboolean resizable;
  int min_width;
  tree_view_column_bound_type type;
} tree_view_column_object;

static const tree_view_column_object columns[] = {
    {.column_type = Text, .type = COLUMN_GID, .min_width = 200, .resizable = TRUE, .title = "GID", .visible = TRUE},
    {.column_type = Text, .type = COLUMN_NAME, .min_width = 200, .resizable = TRUE, .title = "名称", .visible = TRUE},
    {.column_type = Integer, .type = COLUMN_COMPLETEED_LENGTH, .min_width = 100, .resizable = TRUE, .title = "完成大小", .visible = TRUE},
    {.column_type = Integer, .type = COLUMN_TOTAL_LENGTH, .min_width = 100, .resizable = TRUE, .title = "总计大小", .visible = TRUE},
    {.column_type = Progress, .type = COLUMN_COMPLETED_PROGRESS, .min_width = 200, .resizable = TRUE, .title = "完成进度", .visible = TRUE},
    {.column_type = Text, .type = COLUMN_DOWNLOAD_SPEED, .min_width = 100, .resizable = TRUE, .title = "下载速度", .visible = TRUE},
    {.column_type = Text, .type = COLUMN_STATUS, .min_width = 100, .resizable = TRUE, .title = "状态", .visible = TRUE},
    {.column_type = Text, .type = COLUMN_ERRORMSG, .min_width = 100, .resizable = TRUE, .title = "错误信息", .visible = TRUE}};

static int get_tree_view_column_index_by_type(tree_view_column_bound_type type)
{
  int column_count = G_N_ELEMENTS(columns);
  for (int i = 0; i < column_count; i++)
  {
    tree_view_column_object col = columns[i];
    if (col.type == type)
    {
      return i;
    }
  }
  return -1;
}

GtkWidget *do_builder(GtkApplication *app);
void app_trayicon();