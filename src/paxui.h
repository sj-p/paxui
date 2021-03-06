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

enum
{
    PX_ICON_SOURCE = 0,
    PX_ICON_SINK,
    PX_ICON_GEAR,
    PX_ICON_MUTED,
    PX_ICON_UNMUTED,
    PX_ICON_LOCKED,
    PX_ICON_UNLOCKED,
    PX_ICON_SWITCH,
    PX_NUM_ICONS
};


typedef struct _Paxui
{
    pa_glib_mainloop   *pa_ml;
    pa_mainloop_api    *pa_mlapi;
    pa_context         *pa_ctx;

    gboolean            terminated;
    gboolean            updating;
    gboolean            pa_init_done;

    GtkApplication     *app;
    GtkWidget          *window;
    GtkWidget          *scr_win;
    GtkWidget          *layout;
    GtkWidget          *grid;
    GtkWidget          *spinner;
    GdkPixbuf          *icons[PX_NUM_ICONS];

    GtkSizeGroup       *size_group;

    GList              *sources;
    GList              *source_outputs;
    GList              *sinks;
    GList              *sink_inputs;
    GList              *modules;
    GList              *clients;
    GList              *acams;      /* active clients and modules */
    GList              *src_inits;
    GList              *cam_inits;
    GList              *snk_inits;

    PaxuiColour        *colours;
    guint              *col_num;
    guint               num_colours;
    gint                end_row;
    gboolean            dark_theme;
    gboolean            volume_disabled;
    gboolean            tist_enabled;

    gint                drop_x, drop_y, drop_w, drop_h;
    GdkRGBA             drop_colour;
    gboolean            have_drop_colour;

    guint               scroll_src;
    gint                scroll_dir;

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
    gchar      *utf8_name;

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
    gint        colour;

    /* volume control */
    gboolean    vol_enabled;
    gboolean    muted;
    guint32    *levels;
    guint32     n_chan;

    /* channel map */
    guint32    *positions;
    guint32     n_pos;

    /* tool popover */
    GtkWidget  *tool_button;
    GtkWidget  *popover;
    GtkWidget **sliders;
    GtkWidget  *mute_button;
    GtkWidget  *lock_button;
} PaxuiLeaf;


typedef struct _PaxuiState
{
    gint        width;
    gint        height;
} PaxuiState;


typedef struct _PaxuiInitItem
{
    gchar      *str1;
    gchar      *str2;
    gint        y;
} PaxuiInitItem;


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
void                paxui_make_init_strings             (Paxui *paxui);
void                paxui_unload_init_strings           (Paxui *paxui);
void                paxui_leaf_destroy                  (PaxuiLeaf *leaf);

void                paxui_load_conf                     (Paxui *paxui);
void                paxui_log_append                    (int log_level, const char *file, int line, const char *fmt, ...);
void                paxui_settings_save_state           (Paxui *paxui, PaxuiState *state);
void                paxui_settings_load_state           (Paxui *paxui, PaxuiState *state);


#endif
