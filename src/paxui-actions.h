#ifndef _PAXUI_ACTIONS_H_
#define _PAXUI_ACTIONS_H_


#include <glib.h>
#include <gtk/gtk.h>


gboolean    paxui_actions_window_button_event      (GtkWidget *widget, GdkEventButton *event, gpointer udata);
gboolean    paxui_actions_sink_button_event        (GtkWidget *widget, GdkEventButton *event, gpointer udata);
gboolean    paxui_actions_block_button_event       (GtkWidget *widget, GdkEventButton *event, gpointer udata);

#endif
