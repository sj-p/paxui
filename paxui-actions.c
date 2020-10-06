#include <glib.h>
#include <gtk/gtk.h>

#include "paxui.h"
#include "paxui-pulse.h"


static PaxuiLeaf *
find_sink_for_name (const Paxui *paxui, const gchar *name)
{
    GList *l;
    PaxuiLeaf *sk;

    for (l = paxui->sinks; l; l = l->next)
    {
        sk = l->data;

        if (g_strcmp0 (sk->name, name) == 0) return sk;
    }

    return NULL;
}


static void
nullsink_add_from_entry (GtkWidget *entry, gpointer udata)
{
    Paxui *paxui = udata;
    const gchar *name;

    DBG("add nullsink ?");

    name = gtk_entry_get_text (GTK_ENTRY (entry));

    if (name && *name && find_sink_for_name (paxui, name) == NULL)
    {
        gchar *mod_arg;

        mod_arg = g_strdup_printf ("sink_name=%s", name);

        DBG("    named '%s'", mod_arg);

        paxui_pulse_load_module (paxui, "module-null-sink", mod_arg);

        g_free (mod_arg);
    }
}

static void
nullsink_entry_inserted (GtkEditable *editable, const gchar *text, gint n_chars, gint *pos, gpointer udata)
{
    Paxui *paxui = udata;

    DBG("inserted len:%d pos:%d", n_chars, *pos);

    if (!g_regex_match_full (paxui->name_regex, text, n_chars, 0, 0, NULL, NULL))
    {
        DBG("    rejected");

        g_signal_stop_emission_by_name (editable, "insert-text");
    }
}

static void
nullsink_entry_activated (GtkWidget *entry, gpointer udata)
{
    GtkWidget *dialog = udata;

    gtk_dialog_response (GTK_DIALOG (dialog), 1);
}

static void
window_nullsink_dialog (GtkWidget *menu_item, gpointer udata)
{
    GtkWidget *dialog, *vbox, *label, *entry;
    gint resp;
    Paxui *paxui = udata;

    DBG("null-sink dialog");

    gtk_widget_destroy (gtk_widget_get_parent (menu_item));

    dialog = gtk_dialog_new ();
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 8);

    vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    label = gtk_label_new ("Enter name for new null-sink");
    gtk_container_add (GTK_CONTAINER (vbox), label);

    entry = gtk_entry_new ();
    gtk_container_add (GTK_CONTAINER (vbox), entry);
    g_signal_connect (entry, "activate",    G_CALLBACK (nullsink_entry_activated), dialog);
    g_signal_connect (entry, "insert-text", G_CALLBACK (nullsink_entry_inserted),  paxui);

    gtk_widget_show_all (vbox);

    resp = gtk_dialog_run (GTK_DIALOG (dialog));

    DBG("    resp: %d", resp);

    if (resp == 1)
        nullsink_add_from_entry (entry, paxui);

    gtk_widget_destroy (dialog);
}

static void
window_add_loopback (GtkWidget *menu_item, gpointer udata)
{
    Paxui *paxui = udata;

    DBG("add loopback");

    paxui_pulse_load_module (paxui, "module-loopback", "");

    gtk_widget_destroy (gtk_widget_get_parent (menu_item));
}

static void
window_popup_menu (Paxui *paxui, GdkEventButton *event)
{
    GtkWidget *menu, *item;

    DBG("window context menu");

    menu = gtk_menu_new ();
    g_signal_connect (menu, "selection-done", G_CALLBACK (gtk_widget_destroy), NULL);

    item = gtk_menu_item_new_with_label ("Add loopback module");
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    g_signal_connect (item, "activate", G_CALLBACK (window_add_loopback), paxui);

    item = gtk_menu_item_new_with_label ("Add null-sink" PAXUI_UTF8_ELLIPSIS);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    g_signal_connect (item, "activate", G_CALLBACK (window_nullsink_dialog), paxui);

    gtk_widget_show_all (menu);

    gtk_menu_popup_at_pointer (GTK_MENU (menu), (GdkEvent *) event);
}


static void
sink_remove (GtkWidget *menu_item, gpointer udata)
{
    PaxuiLeaf *sink = udata;

    DBG("remove sink %u: module:%u", sink->index, sink->module);

    gtk_widget_destroy (gtk_widget_get_parent (menu_item));

    paxui_pulse_unload_module (sink->paxui, sink->module);
}

static void
sink_popup_menu (PaxuiLeaf *sink, GdkEventButton *event)
{
    GtkWidget *menu, *item;
    PaxuiLeaf *module;

    module = paxui_find_module_for_index (sink->paxui, sink->module);
    if (module == NULL || g_strcmp0 (module->name, "module-null-sink")) return;

    menu = gtk_menu_new ();
    g_signal_connect (menu, "selection-done", G_CALLBACK (gtk_widget_destroy), NULL);

    item = gtk_menu_item_new_with_label ("Remove sink");
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    g_signal_connect (item, "activate", G_CALLBACK (sink_remove), sink);

    gtk_widget_show_all (menu);

    gtk_menu_popup_at_pointer (GTK_MENU (menu), (GdkEvent *) event);
}


static void
block_module_unload (GtkWidget *menu_item, gpointer udata)
{
    PaxuiLeaf *block = udata;

    DBG("unload module %u", block->index);

    gtk_widget_destroy (gtk_widget_get_parent (menu_item));

    paxui_pulse_unload_module (block->paxui, block->index);
}

static void
block_popup_menu (PaxuiLeaf *block, GdkEventButton *event)
{
    GtkWidget *menu, *item;

    if (block->leaf_type != PAXUI_LEAF_TYPE_MODULE) return;

    menu = gtk_menu_new ();
    g_signal_connect (menu, "selection-done", G_CALLBACK (gtk_widget_destroy), NULL);

    item = gtk_menu_item_new_with_label ("Unload module");
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    g_signal_connect (item, "activate", G_CALLBACK (block_module_unload), block);

    gtk_widget_show_all (menu);

    gtk_menu_popup_at_pointer (GTK_MENU (menu), (GdkEvent *) event);
}


gboolean
paxui_actions_sink_button_event (GtkWidget *widget, GdkEventButton *event, gpointer udata)
{
    PaxuiLeaf *sink = udata;

    DBG("sink button event");

    /* Ignore double-clicks and triple-clicks */
    if (gdk_event_triggers_context_menu ((GdkEvent *) event) &&
        event->type == GDK_BUTTON_PRESS)
    {
        DBG("    context menu up");
        sink_popup_menu (sink, event);

        return TRUE;
    }

    return FALSE;
}

gboolean
paxui_actions_block_button_event (GtkWidget *widget, GdkEventButton *event, gpointer udata)
{
    PaxuiLeaf *block = udata;

    DBG("block button event");

    /* Ignore double-clicks and triple-clicks */
    if (gdk_event_triggers_context_menu ((GdkEvent *) event) &&
        event->type == GDK_BUTTON_PRESS)
    {
        DBG("    context menu up");
        block_popup_menu (block, event);

        return TRUE;
    }

    return FALSE;
}

gboolean
paxui_actions_window_button_event (GtkWidget *widget, GdkEventButton *event, gpointer udata)
{
    Paxui *paxui = udata;

    DBG("window button event");

    /* Ignore double-clicks and triple-clicks */
    if (gdk_event_triggers_context_menu ((GdkEvent *) event) &&
        event->type == GDK_BUTTON_PRESS)
    {
        window_popup_menu (paxui, event);

        return TRUE;
    }

    return FALSE;
}
