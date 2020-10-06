#include <glib.h>
#include <glib/gstdio.h>
#include <glib-unix.h>
#include <gtk/gtk.h>

#include "paxui.h"
#include "paxui-pulse.h"
#include "paxui-gui.h"


gint debug = 0;


static guint32
text_to_argb (const gchar *text)
{
    guint64 value;
    gchar *end_ptr;

    if (text == NULL) return 0;

    if (text[0] == '#')
    {
        /* html/css-style #rrggbb or #rrggbbaa */
        switch (strlen (text))
        {
            case 7:
                value = g_ascii_strtoull (text + 1, &end_ptr, 16);
                return (guint32) (*end_ptr == '\0' ? 0xff000000 | value : 0);
            case 9:
                value = g_ascii_strtoull (text + 1, &end_ptr, 16);
                return (guint32) (*end_ptr == '\0' ? (value >> 8) | ((value & 0xff) << 24) : 0);
            default:
                return 0;
        }
    }
    else
    {
        value = g_ascii_strtoull (text, &end_ptr, 0);
    }

    if (*end_ptr == '\0' && value <= G_MAXUINT32)
        return (guint32) value;
    else
        return 0;
}


void
paxui_log_append (int log_level, const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    int size;
    char *str = NULL;

    if (debug < log_level) return;

    va_start (ap, fmt);
    size = g_vasprintf (&str, fmt, ap);
    va_end (ap);

    if (size > 0)
        fprintf (stderr, "paxui %s[%s:%d] %s\n",
                    (log_level == 0 ? "***ERROR*** " : ""),
                    file, line, str);

    g_free (str);
}


PaxuiLeaf *
paxui_find_module_for_index (const Paxui *paxui, guint32 index)
{
    GList *l;
    PaxuiLeaf *md;

    for (l = paxui->modules; l; l = l->next)
    {
        md = l->data;

        if (md->index == index) return md;
    }

    return NULL;
}

PaxuiLeaf *
paxui_find_client_for_index (const Paxui *paxui, guint32 index)
{
    GList *l;
    PaxuiLeaf *cl;

    for (l = paxui->clients; l; l = l->next)
    {
        cl = l->data;

        if (cl->index == index) return cl;
    }

    return NULL;
}

PaxuiLeaf *
paxui_find_source_output_for_index (const Paxui *paxui, guint32 index)
{
    GList *l;
    PaxuiLeaf *so;

    for (l = paxui->source_outputs; l; l = l->next)
    {
        so = l->data;

        if (so->index == index) return so;
    }

    return NULL;
}

PaxuiLeaf *
paxui_find_sink_input_for_index (const Paxui *paxui, guint32 index)
{
    GList *l;
    PaxuiLeaf *si;

    for (l = paxui->sink_inputs; l; l = l->next)
    {
        si = l->data;

        if (si->index == index) return si;
    }

    return NULL;
}

PaxuiLeaf *
paxui_find_source_for_index (const Paxui *paxui, guint32 index)
{
    GList *l;
    PaxuiLeaf *sc;

    for (l = paxui->sources; l; l = l->next)
    {
        sc = l->data;

        if (sc->index == index) return sc;
    }

    return NULL;
}

PaxuiLeaf *
paxui_find_sink_for_index (const Paxui *paxui, guint32 index)
{
    GList *l;
    PaxuiLeaf *sk;

    for (l = paxui->sinks; l; l = l->next)
    {
        sk = l->data;

        if (sk->index == index) return sk;
    }

    return NULL;
}


void
paxui_block_update_active (PaxuiLeaf *block, Paxui *paxui)
{
    GList *l;

    switch (block->leaf_type)
    {
        case PAXUI_LEAF_TYPE_CLIENT:
            for (l = paxui->sink_inputs; l; l = l->next)
            {
                PaxuiLeaf *si = l->data;

                if (si->client == block->index)
                {
                    block->active = TRUE;
                    return;
                }
            }
            for (l = paxui->source_outputs; l; l = l->next)
            {
                PaxuiLeaf *so = l->data;

                if (so->client == block->index)
                {
                    block->active = TRUE;
                    return;
                }
            }
            break;
        case PAXUI_LEAF_TYPE_MODULE:
            for (l = paxui->sink_inputs; l; l = l->next)
            {
                PaxuiLeaf *si = l->data;

                if (si->module == block->index)
                {
                    block->active = TRUE;
                    return;
                }
            }
            for (l = paxui->source_outputs; l; l = l->next)
            {
                PaxuiLeaf *so = l->data;

                if (so->module == block->index)
                {
                    block->active = TRUE;
                    return;
                }
            }
            break;
    }
    block->active = FALSE;
}


void
paxui_leaf_destroy (PaxuiLeaf *leaf)
{
    TRACE("leaf destroy");

    if (GTK_IS_WIDGET (leaf->outer)) gtk_widget_destroy (leaf->outer);
    g_free (leaf->name);
    g_free (leaf->short_name);
    paxui_gui_colour_free (leaf->paxui, leaf->colour);

    g_free (leaf);
}


static void
paxui_destroy (Paxui *paxui)
{
    TRACE("paxui destroy");

    if (paxui->source_icon) g_object_unref (paxui->source_icon);
    if (paxui->sink_icon)   g_object_unref (paxui->sink_icon);
    if (paxui->name_regex)  g_regex_unref  (paxui->name_regex);

    g_free (paxui->colours);
    g_free (paxui->col_num);
    g_free (paxui->conf_dir);
    g_free (paxui->data_dir);
    g_free (paxui);
}

void
paxui_unload_data (Paxui *paxui)
{
    g_list_free (paxui->acams);
    paxui->acams = NULL;
    g_list_free_full (paxui->modules, (GDestroyNotify) paxui_leaf_destroy);
    paxui->modules = NULL;
    g_list_free_full (paxui->clients, (GDestroyNotify) paxui_leaf_destroy);
    paxui->clients = NULL;
    g_list_free_full (paxui->source_outputs, (GDestroyNotify) paxui_leaf_destroy);
    paxui->source_outputs = NULL;
    g_list_free_full (paxui->sink_inputs, (GDestroyNotify) paxui_leaf_destroy);
    paxui->sink_inputs = NULL;
    g_list_free_full (paxui->sources, (GDestroyNotify) paxui_leaf_destroy);
    paxui->sources = NULL;
    g_list_free_full (paxui->sinks, (GDestroyNotify) paxui_leaf_destroy);
    paxui->sinks = NULL;
}


PaxuiLeaf *
paxui_leaf_new (guint leaf_type)
{
    PaxuiLeaf *leaf;

    leaf = g_new0 (PaxuiLeaf, 1);
    leaf->leaf_type = leaf_type;

    leaf->module  = G_MAXUINT32;
    leaf->client  = G_MAXUINT32;
    leaf->source  = G_MAXUINT32;
    leaf->sink    = G_MAXUINT32;
    leaf->monitor = G_MAXUINT32;

    leaf->colour  = -1;
    leaf->x       = -1;
    leaf->y       = -1;

    return leaf;
}


void
paxui_settings_load_state (Paxui *paxui, PaxuiState *state)
{
    gchar *text, *filename, **sline, **slines;

    filename = g_build_filename (paxui->data_dir, "paxui.state", NULL);

    DBG("load from state_file: '%s'", filename);

    if (!g_file_get_contents (filename, &text, NULL, NULL))
    {
        DBG("failed to load state");
        g_free (filename);
        return;
    }
    g_free (filename);

    slines = g_strsplit (text, "\n", -1);
    g_free (text);

    if (slines == NULL) return;

    for (sline = slines; *sline; sline++)
    {
        gchar *p;

        if (**sline == '\0' || **sline == '#') continue;

        TRACE("state line: '%s'", *sline);

        if ((p = strchr (*sline, '=')))
        {
            *p++ = '\0';
            if (*p == '\0') continue;

            if (g_strcmp0 (*sline, "WindowWidth") == 0)
                state->width = MAX (200, atoi (p));
            else if (g_strcmp0 (*sline, "WindowHeight") == 0)
                state->height = MAX (200, atoi (p));
            else if (g_strcmp0 (*sline, "ViewMode") == 0)
                paxui->view_mode = CLAMP (atoi (p), 0, 2);
        }
    }

    g_strfreev (slines);
}

void
paxui_settings_save_state (Paxui *paxui, PaxuiState *state)
{
    gchar *text, *filename;

    filename = g_build_filename (paxui->data_dir, "paxui.state", NULL);

    DBG("save to state_file: '%s'", filename);

    text = g_strdup_printf ("WindowWidth=%d\nWindowHeight=%d\nViewMode=%u\n",
                state->width, state->height, paxui->view_mode);

    if (!g_file_set_contents (filename, text, -1, NULL))
    {
        DBG("failed to save state");
    }

    g_free (text);
    g_free (filename);
}


static void
settings_setup_dirs (Paxui *paxui)
{
    const gchar *prog_name = "paxui";

    paxui->data_dir = g_build_filename (g_get_user_data_dir (), prog_name, NULL);
    if (g_mkdir_with_parents (paxui->data_dir, 0700))
    {
        DBG("failed to make data_dir");

        g_free (paxui->data_dir);

        paxui->data_dir = g_strdup (g_get_current_dir ());
    }

    paxui->conf_dir = g_build_filename (g_get_user_config_dir (), prog_name, NULL);
    if (g_mkdir_with_parents (paxui->conf_dir, 0700))
    {
        DBG("failed to make conf_dir");

        g_free (paxui->conf_dir);

        paxui->conf_dir = g_strdup (g_get_current_dir ());
    }
}

static gboolean
theme_is_dark ()
{
    const gchar *env_var;
    gchar *theme_name = NULL;
    gboolean dark_theme = FALSE;

    /* guess from theme name whether it's a dark theme, and default
     * 'dark_theme' setting accordingly */
    env_var = g_getenv ("GTK_THEME");
    if (env_var != NULL)
    {
        theme_name = g_ascii_strdown (env_var, -1);
    }
    else
    {
        GtkSettings *settings;

        if ((settings = gtk_settings_get_default ()))
        {
            gchar *temp_str;

            g_object_get (settings, "gtk-theme-name", &temp_str, NULL);
            theme_name = g_ascii_strdown (temp_str, -1);

            g_free (temp_str);
        }
    }

    if (theme_name)
    {
        if (strstr (theme_name, "dark") != NULL) dark_theme = TRUE;

        g_free (theme_name);
    }

    return dark_theme;
}

void
paxui_load_conf (Paxui *paxui)
{
    gchar *text, *filename, **clines, **cline;
    GArray *colours;
    gboolean dark_theme, volume_disabled = FALSE;

    dark_theme = theme_is_dark ();

    colours = g_array_new (FALSE, FALSE, sizeof (PaxuiColour));

    filename = g_build_filename (paxui->conf_dir, "paxui.conf", NULL);

    DBG("conf_file: '%s'", filename);

    if (!g_file_get_contents (filename, &text, NULL, NULL))
    {
        DBG("    not read");
        g_free (filename);
        goto conf_finish;
    }
    g_free (filename);

    clines = g_strsplit (text, "\n", -1);
    g_free (text);
    if (clines == NULL) goto conf_finish;

    for (cline = clines; *cline; cline++)
    {
        gchar *p;

        if (**cline == '\0' || **cline == '#') continue;

        TRACE("conf line: '%s'", *cline);

        if ((p = strchr (*cline, '=')))
        {
            *p++ = '\0';

            if (*p)
            {
                if (strcmp (*cline, "LineColour") == 0)
                {
                    PaxuiColour colour;

                    colour.argb = text_to_argb (p);
                    if (colour.argb) g_array_append_val (colours, colour);
                }
                else if (strcmp (*cline, "ImageSource") == 0)
                {
                    GdkPixbuf *pb;
                    GError *err = NULL;

                    if ((pb = gdk_pixbuf_new_from_file (p, &err)))
                    {
                        if (paxui->source_icon) g_object_unref (paxui->source_icon);
                        paxui->source_icon = pb;
                        DBG("loaded source icon from '%s'", p);
                    }
                    else
                    {
                        ERR("failed to load source icon from '%s'", p);
                        if (err)
                        {
                            DBG("    %s", err->message);
                            g_error_free (err);
                        }
                    }
                }
                else if (strcmp (*cline, "ImageSink") == 0)
                {
                    GdkPixbuf *pb;
                    GError *err = NULL;

                    if ((pb = gdk_pixbuf_new_from_file (p, &err)))
                    {
                        if (paxui->sink_icon) g_object_unref (paxui->sink_icon);
                        paxui->sink_icon = pb;
                        DBG("loaded sink icon from '%s'", p);
                    }
                    else
                    {
                        ERR("failed to load sink icon from '%s'", p);
                        if (err)
                        {
                            DBG("    %s", err->message);
                            g_error_free (err);
                        }
                    }
                }
                else if (strcmp (*cline, "ImageView") == 0)
                {
                    GdkPixbuf *pb;
                    GError *err = NULL;

                    if ((pb = gdk_pixbuf_new_from_file (p, &err)))
                    {
                        if (paxui->view_icon) g_object_unref (paxui->view_icon);
                        paxui->view_icon = pb;
                        DBG("loaded view icon from '%s'", p);
                    }
                    else
                    {
                        ERR("failed to load view icon from '%s'", p);
                        if (err)
                        {
                            DBG("    %s", err->message);
                            g_error_free (err);
                        }
                    }
                }
                else if (strcmp (*cline, "DarkTheme") == 0)
                {
                    if (g_strcmp0 (p, "1") == 0)
                        dark_theme = TRUE;
                    else if (g_ascii_strcasecmp (p, "y") == 0)
                        dark_theme = TRUE;
                    else if (g_strcmp0 (p, "0") == 0)
                        dark_theme = FALSE;
                    else if (g_ascii_strcasecmp (p, "n") == 0)
                        dark_theme = FALSE;
                }
                else if (strcmp (*cline, "VolumeControlsDisabled") == 0)
                {
                    if (g_strcmp0 (p, "1") == 0)
                        volume_disabled = TRUE;
                    else if (g_ascii_strcasecmp (p, "y") == 0)
                        volume_disabled = TRUE;
                    else if (g_strcmp0 (p, "0") == 0)
                        volume_disabled = FALSE;
                    else if (g_ascii_strcasecmp (p, "n") == 0)
                        volume_disabled = FALSE;
                }
            }
        }
    }

    g_strfreev (clines);

conf_finish:
    if (colours->len)
    {
        paxui->num_colours = colours->len;
        paxui->colours = (PaxuiColour *) g_array_free (colours, FALSE);
    }
    else
    {
        g_array_free (colours, TRUE);
        paxui_gui_get_default_colours (paxui);
    }
    paxui->col_num = g_new0 (guint, paxui->num_colours);

    DBG("have %u colours", paxui->num_colours);

    paxui->dark_theme = dark_theme;
    paxui->volume_disabled = volume_disabled;
}


/* check what debug output is requested:
 * if DEBUG is set, set debug=1 */
static void
paxui_get_debug_level ()
{
    const gchar *env_var;

    env_var = g_getenv ("DEBUG");
    if (env_var != NULL)
    {
        debug = 1;
        if (env_var[0] != '\0')
        {   /* if DEBUG value is a number, set debug accordingly */
            gint64 debug_val;
            gchar *end_ptr;

            debug_val = g_ascii_strtoll (env_var, &end_ptr, 0);

            if (*end_ptr == '\0')
                debug = CLAMP(debug_val, G_MININT, G_MAXINT);
        }
    }
}


int
main (int argc, char **argv)
{
    Paxui *paxui;

    paxui_get_debug_level ();

    paxui = g_new0 (Paxui, 1);
    settings_setup_dirs (paxui);

    paxui->app = gtk_application_new ("org.paxui", G_APPLICATION_SEND_ENVIRONMENT);
    g_signal_connect (paxui->app, "activate", G_CALLBACK (paxui_gui_activate), paxui);

    g_unix_signal_add (SIGTERM, (GSourceFunc) g_application_quit, paxui->app);
    g_unix_signal_add (SIGHUP,  (GSourceFunc) g_application_quit, paxui->app);
    g_unix_signal_add (SIGINT,  (GSourceFunc) g_application_quit, paxui->app);

    paxui->name_regex = g_regex_new ("^[A-Za-z0-9._-]+$", 0, 0, NULL);

    g_application_run (G_APPLICATION (paxui->app), argc, argv);

    g_object_unref (paxui->app);

    /* disconnect & free pa stuff */
    paxui_pulse_stop_client (paxui);

    /* free paxui data & struct */
    paxui_unload_data (paxui);
    paxui_destroy (paxui);



    return 0;
}
