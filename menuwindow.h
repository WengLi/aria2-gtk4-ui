#include <gtk/gtk.h>
#include "utilities.h"

typedef enum _tree_view_column_type
{
  Text,
  Integer,
  Progress
} tree_view_column_type;

typedef struct _tree_view_column_object
{
  tree_view_column_type column_type;
  gboolean visible;
  char *title;
  gboolean resizable;
  int min_width;
} tree_view_column_object;

GtkWidget *do_builder(GtkApplication *app);
void app_trayicon();