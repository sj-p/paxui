#ifndef _PAXUI_H_
#define _PAXUI_H_


#define ERR(...)    paxui_log_append(0,__FILE__,__LINE__,__VA_ARGS__);
#define DBG(...)    paxui_log_append(1,__FILE__,__LINE__,__VA_ARGS__);
#define TRACE(...)  paxui_log_append(2,__FILE__,__LINE__,__VA_ARGS__);


#include <glib.h>
#include <gtk/gtk.h>
#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>


#define PAXUI_UTF8_ELLIPSIS "\342\200\246"


typedef union _PaxuiColour PaxuiColour;


enum
{
    PAXUI_LEAF_TYPE_MODULE = 0,
    PAXUI_LEAF_TYPE_CLIENT,
    PAXUI_LEAF_TYPE_SOURCE,
    PAXUI_LEAF_TYPE_SINK,
    PAXUI_LEAF_TYPE_SOURCE_OUTPUT,
    PAXUI_LEAF_TYPE_SINK_INPUT,
    PAXUI_LEAF_NUM_TYPES
};


typedef struct _Paxui
{
    pa_glib_mainloop   *pa_ml;
    pa_mainloop_api    *pa_mlapi;
    pa_context         *pa_ctx;

    gboolean            terminated;
    gboolean            updating;

    GtkApplication     *app;
    GtkWidget          *window;
    GtkWidget          *layout;
    GtkWidget          *grid;
    GtkWidget          *spinner;
    GdkPixbuf          *source_icon;
    GdkPixbuf          *sink_icon;
    GdkPixbuf          *view_icon;

    GList              *sources;
    GList              *source_outputs;
    GList              *sinks;
    GList              *sink_inputs;
    GList              *modules;
    GList              *clients;
    GList              *acams;      /* active clients and modules */

    PaxuiColour        *colours;
    guint              *col_num;
    guint               num_colours;
    guint               view_mode;
    gboolean            dark_theme;
    gboolean            volume_disabled;

    guint               blk_row1;
    guint               blk_step;
    guint               sosi_row1;
    guint               sosi_step;

    GRegex             *name_regex;

    gchar              *conf_dir;
    gchar              *data_dir;
} Paxui;

union _PaxuiColour
{
    guint32 argb;
    struct
    {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
        guchar b;
        guchar g;
        guchar r;
        guchar a;
#else
        guchar a;
        guchar r;
        guchar g;
        guchar b;
#endif
    };
};


typedef struct _PaxuiLeaf
{
    guint       leaf_type;

    /* basic data */
    guint32     index;
    gchar      *name;
    gchar      *short_name;

    /* relative items by pa index */
    guint32     module;
    guint32     client;
    guint32     source;
    guint32     sink;
    guint32     monitor;

    /* whether client/module owns so/si */
    gboolean    active;

    /* back reference to main struct */
    Paxui      *paxui;

    /* gui data */
    GtkWidget  *outer, *label;
    gint        x, y;       /* grid coords */
    gint        tx, ty;     /* calculated coords */
    gint        colour;

    /* volume control */
    gboolean    vol_enabled;
    gboolean    muted;
    guint32     level;
    guint32     n_chan;
    GtkWidget  *vol_button;
} PaxuiLeaf;


typedef struct _PaxuiState
{
    gint        width;
    gint        height;
} PaxuiState;


extern gint debug;


PaxuiLeaf          *paxui_find_module_for_index         (const Paxui *paxui, guint32 index);
PaxuiLeaf          *paxui_find_client_for_index         (const Paxui *paxui, guint32 index);
PaxuiLeaf          *paxui_find_source_for_index         (const Paxui *paxui, guint32 index);
PaxuiLeaf          *paxui_find_sink_for_index           (const Paxui *paxui, guint32 index);
PaxuiLeaf          *paxui_find_source_output_for_index  (const Paxui *paxui, guint32 index);
PaxuiLeaf          *paxui_find_sink_input_for_index     (const Paxui *paxui, guint32 index);
void                paxui_block_update_active           (PaxuiLeaf *block, Paxui *paxui);

PaxuiLeaf          *paxui_leaf_new                      (guint leaf_type);
void                paxui_unload_data                   (Paxui *paxui);
void                paxui_leaf_destroy                  (PaxuiLeaf *leaf);

void                paxui_load_conf                     (Paxui *paxui);
void                paxui_log_append                    (int log_level, const char *file, int line, const char *fmt, ...);
void                paxui_settings_save_state           (Paxui *paxui, PaxuiState *state);
void                paxui_settings_load_state           (Paxui *paxui, PaxuiState *state);


#endif
