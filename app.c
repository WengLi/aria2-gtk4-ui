#include <pthread.h>
#include "menuwindow.h"

#define APPLICATION_ID "com.henry.app.downloadmgr"

static void app_activate(GApplication *application)
{
    g_print("app_activate ok\n");
}

// static void app_open(GApplication *application, GFile **files, gint n_files, const gchar *hint)
// {
//     GtkApplication *app = GTK_APPLICATION(application);
//     GtkWidget *win = GTK_WIDGET(gtk_application_get_active_window(app));
//     g_print("app_open ok\n");
//     gtk_widget_show(win);
// }

static void app_startup(GApplication *application)
{
    GtkApplication *app = GTK_APPLICATION(application);
    do_builder(app);
    g_print("app_startup ok\n");
}

int main(int argc, char **argv)
{
    GtkApplication *app;
    int stat;

    app = gtk_application_new(APPLICATION_ID, G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "startup", G_CALLBACK(app_startup), NULL);
    g_signal_connect(app, "activate", G_CALLBACK(app_activate), NULL);
    // g_signal_connect(app, "open", G_CALLBACK(app_open), NULL);

    stat = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return stat;
}
