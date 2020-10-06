#include <glib.h>
#include <gtk/gtk.h>

#include "paxui.h"
#include "paxui-gui.h"
#include "paxui-pulse.h"
#include "paxui-actions.h"
#include "paxui-icons.h"


#define CR_RBGA(c) (double) (c).r/255, (double) (c).g/255, (double) (c).b/255, (double) (c).a/255

#define PAXUI_LAYOUT_PADDING (16)
#define PAXUI_LINE_WIDTH (10)

#define PAXUI_SC_COLOUR "#ffd0d0"
#define PAXUI_SO_COLOUR "#ffffd0"
#define PAXUI_CL_COLOUR "#d0ffd0"
#define PAXUI_MD_COLOUR "#ffd0ff"
#define PAXUI_SI_COLOUR "#d0ffff"
#define PAXUI_SK_COLOUR "#d0d0ff"


enum
{
    PAXUI_COL_SOURCES = 0,
    PAXUI_COL_SRC_OPS,
    PAXUI_COL_BLOCKS,
    PAXUI_COL_SNK_IPS,
    PAXUI_COL_SINKS
};


const GtkTargetEntry sink_target[]   = {{"STRING", GTK_TARGET_SAME_APP|GTK_TARGET_OTHER_WIDGET, 0}};
const GtkTargetEntry source_target[] = {{"STRING", GTK_TARGET_SAME_APP|GTK_TARGET_OTHER_WIDGET, 1}};
const GtkTargetEntry block_target[] = {{"STRING", GTK_TARGET_SAME_APP|GTK_TARGET_OTHER_WIDGET, 2}};


void
paxui_gui_get_default_colours (Paxui *paxui)
{
    /* default line colours */
    PaxuiColour colours[] = {{0xff808080}, {0xffff8080}, {0xff80ff80}, {0xffffff80},
                             {0xff8080ff}, {0xffff80ff}, {0xff80ffff}, {0xffffffff}};

    TRACE("use default colours");

    paxui->num_colours = 8;
    paxui->colours = g_new (PaxuiColour, 8);
    memcpy (paxui->colours, colours, 8 * sizeof (PaxuiColour));
}


static guint
paxui_str_to_index (const gchar *str)
{
    long long int id;
    gchar *p;

    id = strtoll (str, &p, 10);

    return (guint)(id >= 0 && id <= G_MAXINT32 && *p == '\0' ? id : G_MAXINT32);
}

static gboolean
paxui_str_to_block_index (const gchar *str, guint *leaf_type, guint *index)
{
    long long int b, i;
    gchar *p;

    b = strtoll (str, &p, 10);
    if (b < 0 || b >= PAXUI_LEAF_NUM_TYPES || *p != ',') return FALSE;

    p++;
    i = strtoll (p, &p, 10);
    if (b < 0 || b >= G_MAXUINT32 || *p != '\0') return FALSE;

    if (leaf_type) *leaf_type = b;
    if (index) *index = i;

    return TRUE;
}


void
paxui_gui_colour_free (Paxui *paxui, gint index)
{
    if (index < 0 || index >= paxui->num_colours || paxui->col_num[index] == 0) return;

    paxui->col_num[index]--;
}


static void
set_row_params (PaxuiLeaf *leaf, gint *y, gint *dy)
{
    switch (leaf->leaf_type)
    {
        case PAXUI_LEAF_TYPE_SOURCE:
        case PAXUI_LEAF_TYPE_MODULE:
        case PAXUI_LEAF_TYPE_CLIENT:
        case PAXUI_LEAF_TYPE_SINK:
            if (y) *y = leaf->paxui->blk_row1;
            if (dy) *dy = leaf->paxui->blk_step;
            break;
        case PAXUI_LEAF_TYPE_SOURCE_OUTPUT:
        case PAXUI_LEAF_TYPE_SINK_INPUT:
            if (y) *y = leaf->paxui->sosi_row1;
            if (dy) *dy = leaf->paxui->sosi_step;
            break;
    }
}


static gint
get_new_colour (Paxui *paxui)
{
    gint i, id, in;

    TRACE("get new colour");

    id = -1;
    for (i = 0; i < paxui->num_colours; i++)
    {
        if (paxui->col_num[i] == 0)
        {
            paxui->col_num[i] = 1;
            return i;
        }
        if (id == -1 || paxui->col_num[i] < in)
        {
            id = i;
            in = paxui->col_num[i];
        }
    }

    if (id < 0) id = 0;

    paxui->col_num[id]++;
    return id;
}

static void
leaf_grid_attach (PaxuiLeaf *leaf)
{
    GtkWidget *parent;

    if (leaf == NULL || leaf->outer == NULL) return;

    g_object_ref (leaf->outer);
    if ((parent = gtk_widget_get_parent (leaf->outer)))
        gtk_container_remove (GTK_CONTAINER (parent), leaf->outer);

    gtk_grid_attach (GTK_GRID (leaf->paxui->grid), leaf->outer, leaf->x, leaf->y, 1, 1);
    g_object_unref (leaf->outer);
}


static gint
cmp_blocks_y (gconstpointer a, gconstpointer b)
{
    const PaxuiLeaf *bk_a = a, *bk_b = b;

    return (bk_a->y - bk_b->y);
}


void
paxui_gui_remove_from_column (GList *column, PaxuiLeaf *leaf)
{
    gint y, dy = 1;
    GList *l;

    if (leaf == NULL || leaf->outer == NULL) return;

    DBG("remove from column i:%u", leaf->index);

    set_row_params (leaf, NULL, &dy);

    y = leaf->y;
    for (l = column; l; l = l->next)
    {
        PaxuiLeaf *lf = l->data;

        if (lf->outer && lf->y > y) lf->y -= dy;
    }

    g_list_foreach (column, (GFunc) leaf_grid_attach, NULL);
}

static void
cycle_column (GList *column, PaxuiLeaf *dest, PaxuiLeaf *from)
{
    gint y, dy = 1;
    GList *l;

    DBG("cycle column: i:%u to pos of i:%u", from->index, dest->index);

    set_row_params (dest, NULL, &dy);

    if (from->y > dest->y)
    {
        DBG("higher");
        y = dest->y;
        for (l = column; l; l = l->next)
        {
            PaxuiLeaf *lf = l->data;

            if (lf->outer && lf->y >= y && lf->y < from->y) lf->y += dy;
        }
        from->y = y;

        g_list_foreach (column, (GFunc) leaf_grid_attach, NULL);
    }
    else
    {
        DBG("lower");
        y = dest->y;
        for (l = column; l; l = l->next)
        {
            PaxuiLeaf *lf = l->data;

            if (lf->outer && lf->y > from->y && lf->y <= y) lf->y -= dy;
        }
        from->y = y;

        g_list_foreach (column, (GFunc) leaf_grid_attach, NULL);
    }
}

static void
any_drag_end (GtkWidget *widget, GdkDragContext *context, gpointer udata)
{
    PaxuiLeaf *leaf = udata;

    DBG("drag end");

    paxui_gui_trigger_update (leaf->paxui);
}


static void
any_drag_data_get (GtkWidget *widget, GdkDragContext *context, GtkSelectionData *data,
                   guint info, guint time, gpointer udata)
{
    gchar *str;
    PaxuiLeaf *lf = udata;

    DBG("drag data get");

    switch (lf->leaf_type)
    {
        case PAXUI_LEAF_TYPE_MODULE:
        case PAXUI_LEAF_TYPE_CLIENT:
            str = g_strdup_printf ("block:%u,%u", lf->leaf_type, lf->index);
            break;
        case PAXUI_LEAF_TYPE_SOURCE:
            str = g_strdup_printf ("source:%u", lf->index);
            break;
        case PAXUI_LEAF_TYPE_SINK:
            str = g_strdup_printf ("sink:%u", lf->index);
            break;
        case PAXUI_LEAF_TYPE_SOURCE_OUTPUT:
            str = g_strdup_printf ("so:%u", lf->index);
            break;
        case PAXUI_LEAF_TYPE_SINK_INPUT:
            str = g_strdup_printf ("si:%u", lf->index);
            break;
        default:
            return;
    }
    gtk_selection_data_set_text (data, str, -1);
    g_free (str);
}


static void
source_drag_data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
                           GtkSelectionData *data, guint info, guint time, gpointer udata)
{
    gchar *str;
    guint source_index;
    PaxuiLeaf *sc = udata;

    DBG("source drag received");

    str = (gchar *) gtk_selection_data_get_text (data);
    if (str == NULL) return;

    DBG("  received '%s'", str);
    if (g_str_has_prefix (str, "so:"))
    {
        source_index = paxui_str_to_index (str + 3);

        if (source_index < G_MAXUINT32)
        {
            paxui_pulse_move_source_output (sc->paxui, source_index, sc->index);
        }
        else
        {
            DBG("    drag-drop received bad source_output:%u", source_index);
        }
    }
    else if (g_str_has_prefix (str, "source:"))
    {
        PaxuiLeaf *mv;

        source_index = paxui_str_to_index (str + 7);

        if (source_index < G_MAXUINT32 && (mv = paxui_find_source_for_index (sc->paxui, source_index)))
        {
            DBG("    move source:%u to pos of source:%u", source_index, sc->index);

            sc->paxui->sources = g_list_sort (sc->paxui->sources, cmp_blocks_y);
            cycle_column (sc->paxui->sources, sc, mv);
        }
        else
        {
            DBG("    drag-drop received bad source:%u", source_index);
        }
    }
    else
    {
        DBG("    bad drop, ignoring");
    }

    g_free (str);
}

static void
sink_drag_data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
                         GtkSelectionData *data, guint info, guint time, gpointer udata)
{
    gchar *str;
    guint sink_index;
    PaxuiLeaf *sk = udata;

    DBG("sink drag received");

    str = (gchar *) gtk_selection_data_get_text (data);
    if (str == NULL) return;

    if (g_str_has_prefix (str, "si:"))
    {
        sink_index = paxui_str_to_index (str + 3);

        if (sink_index < G_MAXUINT32)
        {
            paxui_pulse_move_sink_input (sk->paxui, sink_index, sk->index);
        }
        else
        {
            DBG("    drag-drop received bad sink_input:%u", sink_index);
        }
    }
    else if (g_str_has_prefix (str, "sink:"))
    {
        PaxuiLeaf *mv;

        sink_index = paxui_str_to_index (str + 5);

        if (sink_index < G_MAXUINT32 && (mv = paxui_find_sink_for_index (sk->paxui, sink_index)))
        {
            DBG("    move sink:%u to pos of sink:%u", sink_index, sk->index);

            sk->paxui->sinks = g_list_sort (sk->paxui->sinks, cmp_blocks_y);
            cycle_column (sk->paxui->sinks, sk, mv);
        }
        else
        {
            DBG("    drag-drop received bad sink:%u", sink_index);
        }
    }
    else
    {
        DBG("    bad drag, ignoring");
    }

    g_free (str);
}


static void
block_drag_data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
                          GtkSelectionData *data, guint info, guint time, gpointer udata)
{
    gchar *str;
    guint leaf_type, block_index;
    PaxuiLeaf *lf = udata;

    DBG("block drag received");

    str = (gchar *) gtk_selection_data_get_text (data);
    if (str == NULL) return;

    DBG("    string:'%s'", str);
    if (g_str_has_prefix (str, "block:") &&
        paxui_str_to_block_index (str + 6, &leaf_type, &block_index))
    {
        PaxuiLeaf *bk = NULL;

        DBG("    %u:%u", leaf_type, block_index);
        switch (leaf_type)
        {
            case PAXUI_LEAF_TYPE_MODULE:
                bk = paxui_find_module_for_index (lf->paxui, block_index);
                break;
            case PAXUI_LEAF_TYPE_CLIENT:
                bk = paxui_find_client_for_index (lf->paxui, block_index);
                break;
        }
        if (bk)
        {
            lf->paxui->acams = g_list_sort (lf->paxui->acams, cmp_blocks_y);

            cycle_column (lf->paxui->acams, lf, bk);
        }
    }
    else
    {
        DBG("    bad drag, ignoring");
    }

    g_free (str);
}


static gdouble
volume_uint_to_double (guint32 volume)
{
    return MIN ((gdouble) volume / 65535, 1.);
}

static guint32
volume_double_to_uint (gdouble volume)
{
    return MIN (65535 * volume, 65535);
}

static void
leaf_vol_button_cb (GtkScaleButton *button, gdouble value, gpointer udata)
{
    PaxuiLeaf *leaf = udata;
    guint32 volume;

    DBG("leaf vol setting %f", value);
    volume = volume_double_to_uint (value);
    if (volume != leaf->level)
    {
        DBG("  ->%u", volume);
        leaf->level = volume;
        paxui_pulse_volume_set (leaf);
    }
}

static void
leaf_mute_button_cb (GtkCheckButton *check, gpointer udata)
{
    PaxuiLeaf *leaf = udata;
    gboolean muted;

    muted = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check));
    DBG("leaf muted:%d", leaf->muted);

    if (leaf->muted != muted)
    {
        leaf->muted = muted;
        paxui_pulse_mute_set (leaf);
    }
}


static void
leaf_set_volume (PaxuiLeaf *leaf)
{
    gboolean muted;
    guint32 level;

    if (leaf->vol_button == NULL) return;

    /* currently displayed values */
    muted = !gtk_widget_get_sensitive (leaf->vol_button);
    level = volume_double_to_uint (gtk_scale_button_get_value (GTK_SCALE_BUTTON (leaf->vol_button)));
    DBG("vol update l:%u->%u m:%d->%d", level, leaf->level, muted, leaf->muted);

    if (leaf->muted != muted)
    {
        gtk_widget_set_sensitive (leaf->vol_button, !leaf->muted);
    }
    if (leaf->level != level)
    {
        gtk_scale_button_set_value (GTK_SCALE_BUTTON (leaf->vol_button),
                                    volume_uint_to_double (leaf->level));
    }
}

static void
leaf_set_label (PaxuiLeaf *leaf)
{
    if (leaf->label == NULL) return;

    switch (leaf->leaf_type)
    {
        gchar *txt;
        GString *str;
        gchar *p;

        case PAXUI_LEAF_TYPE_MODULE:
            p = (leaf->name && g_str_has_prefix (leaf->name, "module-")
                    ? leaf->name + 7 : leaf->name);
            txt = g_markup_printf_escaped (
                            "%s\n<small>module #%u</small>",
                            (p ? p : ""),
                            leaf->index);
            gtk_label_set_markup (GTK_LABEL (leaf->label), txt);
            g_free (txt);
            break;
        case PAXUI_LEAF_TYPE_CLIENT:
            gtk_widget_set_tooltip_text (leaf->outer, leaf->name);
            txt = g_markup_printf_escaped (
                            "%s\n<small>client #%u</small>",
                            leaf->short_name,
                            leaf->index);
            gtk_label_set_markup (GTK_LABEL (leaf->label), txt);
            g_free (txt);
            break;
        case PAXUI_LEAF_TYPE_SOURCE:
            if (leaf->name && (p = strchr (leaf->name, '.')))
                str = g_string_new_len (leaf->name, p - leaf->name);
            else
                str = g_string_new (leaf->name);

            txt = g_markup_printf_escaped (
                            "%s\n<small>%s#%u</small>",
                            str->str,
                            (leaf->monitor != G_MAXUINT ? "monitor " : ""),
                            leaf->index);
            gtk_label_set_markup (GTK_LABEL (leaf->label), txt);

            g_string_free (str, TRUE);
            g_free (txt);
            break;
        case PAXUI_LEAF_TYPE_SINK:
            if (leaf->name && (p = strchr (leaf->name, '.')))
                str = g_string_new_len (leaf->name, p - leaf->name);
            else
                str = g_string_new (leaf->name);

            txt = g_markup_printf_escaped (
                            "%s\n<small>#%u</small>",
                            str->str,
                            leaf->index);
            gtk_label_set_markup (GTK_LABEL (leaf->label), txt);

            g_string_free (str, TRUE);
            g_free (txt);
            break;
        case PAXUI_LEAF_TYPE_SOURCE_OUTPUT:
        case PAXUI_LEAF_TYPE_SINK_INPUT:
            txt = g_strdup_printf ("%s\n#%u", leaf->short_name, leaf->index);

            gtk_label_set_text (GTK_LABEL (leaf->label), txt);
            g_free (txt);
            break;
    }
}


static void
label_style_updated (GtkWidget *label, gpointer udata)
{
    GtkStyleContext *st;
    GValue value = G_VALUE_INIT;
    GtkSettings *settings;
    gint w, dpi = 96;

    st = gtk_widget_get_style_context (label);
    gtk_style_context_get_property (st, "font", GTK_STATE_FLAG_NORMAL, &value);

    if ((settings = gtk_settings_get_default ()))
    {
        g_object_get (settings, "gtk-xft-dpi", &dpi, NULL);
        dpi = (dpi > 0 ? dpi / 1024 : 96);
    }

    w = pango_font_description_get_size (g_value_get_boxed (&value)) * dpi / 12288;
    DBG("label width updated to w:%d", w);
    gtk_widget_set_size_request (label, w, -1);

    g_value_unset (&value);
}


static void
leaf_set_initial_pos (PaxuiLeaf *leaf)
{
    gint y, dy;

    switch (leaf->leaf_type)
    {
        case PAXUI_LEAF_TYPE_SOURCE:
            leaf->x = PAXUI_COL_SOURCES;
            break;
        case PAXUI_LEAF_TYPE_SOURCE_OUTPUT:
            leaf->x = PAXUI_COL_SRC_OPS;
            break;
        case PAXUI_LEAF_TYPE_MODULE:
        case PAXUI_LEAF_TYPE_CLIENT:
            leaf->x = PAXUI_COL_BLOCKS;
            break;
        case PAXUI_LEAF_TYPE_SINK_INPUT:
            leaf->x = PAXUI_COL_SNK_IPS;
            break;
        case PAXUI_LEAF_TYPE_SINK:
            leaf->x = PAXUI_COL_SINKS;
            break;
        default:
            return;
    }

    set_row_params (leaf, &y, &dy);

    for (; ; y += dy)
    {
        GtkWidget *w;

        w = gtk_grid_get_child_at (GTK_GRID (leaf->paxui->grid), leaf->x, y);
        if (w == NULL)
        {
            leaf->y = y;
            leaf_grid_attach (leaf);
            return;
        }
    }
}


void
leaf_gui_new (PaxuiLeaf *leaf)
{
    GtkWidget *hbox, *vbox = NULL;

    if (leaf == NULL || leaf->outer) return;

    leaf->outer = gtk_event_box_new ();
    gtk_widget_set_tooltip_text (leaf->outer, leaf->name);
    gtk_style_context_add_class (gtk_widget_get_style_context (leaf->outer),
                                 "outer");

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_container_add (GTK_CONTAINER (leaf->outer), hbox);

    leaf->label = gtk_label_new (NULL);
    gtk_label_set_xalign (GTK_LABEL (leaf->label), 0.);
    gtk_label_set_line_wrap (GTK_LABEL (leaf->label), TRUE);
    gtk_label_set_line_wrap_mode (GTK_LABEL (leaf->label), PANGO_WRAP_WORD_CHAR);
    g_signal_connect (leaf->label, "style-updated", G_CALLBACK (label_style_updated), leaf);

    leaf_set_label (leaf);

    if (leaf->vol_enabled)
    {
        GtkWidget *check;
        GtkAdjustment *adj;

        vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_valign (vbox, GTK_ALIGN_CENTER);

        leaf->vol_button = gtk_volume_button_new ();
        adj = gtk_scale_button_get_adjustment (GTK_SCALE_BUTTON (leaf->vol_button));
        gtk_adjustment_set_page_increment (adj, .05);
        gtk_box_pack_start (GTK_BOX (vbox), leaf->vol_button, FALSE, FALSE, 0);
        g_signal_connect (leaf->vol_button, "value-changed", G_CALLBACK (leaf_vol_button_cb), leaf);

        check = gtk_check_button_new ();
        gtk_widget_set_halign (check, GTK_ALIGN_CENTER);
        gtk_box_pack_start (GTK_BOX (vbox), check, FALSE, FALSE, 0);

        g_object_bind_property (check, "active", leaf->vol_button, "sensitive",
                                G_BINDING_BIDIRECTIONAL);
        g_signal_connect (check, "toggled", G_CALLBACK (leaf_mute_button_cb), leaf);

        /* set controls to initial values */
        gtk_scale_button_set_value (GTK_SCALE_BUTTON (leaf->vol_button),
                                    volume_uint_to_double (leaf->level));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), !leaf->muted);
        gtk_widget_set_sensitive (leaf->vol_button, !leaf->muted);
    }

    switch (leaf->leaf_type)
    {
        case PAXUI_LEAF_TYPE_MODULE:
            leaf->paxui->acams = g_list_prepend (leaf->paxui->acams, leaf);

            gtk_style_context_add_class (
                    gtk_widget_get_style_context (leaf->outer), "module");

            gtk_box_pack_start (GTK_BOX (hbox), leaf->label, TRUE, TRUE, 0);

            gtk_drag_source_set (leaf->outer, GDK_BUTTON1_MASK, block_target, 1, GDK_ACTION_MOVE);
            gtk_drag_dest_set (leaf->outer, GTK_DEST_DEFAULT_ALL, block_target, 1, GDK_ACTION_MOVE);
            g_signal_connect (leaf->outer, "button-press-event", G_CALLBACK (paxui_actions_block_button_event), leaf);
            g_signal_connect (leaf->outer, "drag-data-received", G_CALLBACK (block_drag_data_received), leaf);
            g_signal_connect (leaf->outer, "drag-data-get", G_CALLBACK (any_drag_data_get), leaf);
            g_signal_connect (leaf->outer, "drag-end", G_CALLBACK (any_drag_end), leaf);
            break;
        case PAXUI_LEAF_TYPE_CLIENT:
            leaf->paxui->acams = g_list_prepend (leaf->paxui->acams, leaf);

            gtk_style_context_add_class (
                    gtk_widget_get_style_context (leaf->outer), "client");

            gtk_box_pack_start (GTK_BOX (hbox), leaf->label, TRUE, TRUE, 0);

            gtk_drag_source_set (leaf->outer, GDK_BUTTON1_MASK, block_target, 1, GDK_ACTION_MOVE);
            gtk_drag_dest_set (leaf->outer, GTK_DEST_DEFAULT_ALL, block_target, 1, GDK_ACTION_MOVE);
            g_signal_connect (leaf->outer, "drag-data-get", G_CALLBACK (any_drag_data_get), leaf);
            g_signal_connect (leaf->outer, "drag-end", G_CALLBACK (any_drag_end), leaf);
            g_signal_connect (leaf->outer, "drag-data-received", G_CALLBACK (block_drag_data_received), leaf);
            break;
        case PAXUI_LEAF_TYPE_SOURCE:
            gtk_style_context_add_class (
                    gtk_widget_get_style_context (leaf->outer), "source");

            gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
            gtk_box_pack_start (GTK_BOX (hbox), leaf->label, TRUE, TRUE, 0);

            gtk_drag_source_set (leaf->outer, GDK_BUTTON1_MASK, source_target, 1, GDK_ACTION_MOVE);
            gtk_drag_source_set_icon_pixbuf (leaf->outer, leaf->paxui->source_icon);
            gtk_drag_dest_set (leaf->outer, GTK_DEST_DEFAULT_ALL, source_target, 1, GDK_ACTION_MOVE);
            g_signal_connect (leaf->outer, "drag-data-received", G_CALLBACK (source_drag_data_received), leaf);
            g_signal_connect (leaf->outer, "drag-data-get", G_CALLBACK (any_drag_data_get), leaf);
            g_signal_connect (leaf->outer, "drag-end", G_CALLBACK (any_drag_end), leaf);
            break;
        case PAXUI_LEAF_TYPE_SINK:
            gtk_style_context_add_class (
                    gtk_widget_get_style_context (leaf->outer), "sink");

            gtk_box_pack_start (GTK_BOX (hbox), leaf->label, TRUE, TRUE, 0);
            gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

            gtk_drag_source_set (leaf->outer, GDK_BUTTON1_MASK, sink_target, 1, GDK_ACTION_MOVE);
            gtk_drag_source_set_icon_pixbuf (leaf->outer, leaf->paxui->sink_icon);
            gtk_drag_dest_set (leaf->outer, GTK_DEST_DEFAULT_ALL, sink_target, 1, GDK_ACTION_MOVE);
            g_signal_connect (leaf->outer, "button-press-event", G_CALLBACK (paxui_actions_sink_button_event), leaf);
            g_signal_connect (leaf->outer, "drag-data-received", G_CALLBACK (sink_drag_data_received), leaf);
            g_signal_connect (leaf->outer, "drag-data-get", G_CALLBACK (any_drag_data_get), leaf);
            g_signal_connect (leaf->outer, "drag-end", G_CALLBACK (any_drag_end), leaf);
            break;
        case PAXUI_LEAF_TYPE_SOURCE_OUTPUT:
            gtk_style_context_add_class (
                    gtk_widget_get_style_context (leaf->outer), "source-output");

            gtk_box_pack_start (GTK_BOX (hbox), leaf->label, TRUE, TRUE, 0);
            if (vbox) gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

            gtk_drag_source_set (leaf->outer, GDK_BUTTON1_MASK, source_target, 1, GDK_ACTION_MOVE);
            gtk_drag_source_set_icon_pixbuf (leaf->outer, leaf->paxui->source_icon);
            g_signal_connect (leaf->outer, "drag-data-get", G_CALLBACK (any_drag_data_get), leaf);
            g_signal_connect (leaf->outer, "drag-end", G_CALLBACK (any_drag_end), leaf);
            break;
        case PAXUI_LEAF_TYPE_SINK_INPUT:
            gtk_style_context_add_class (
                    gtk_widget_get_style_context (leaf->outer), "sink-input");

            if (vbox) gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
            gtk_box_pack_start (GTK_BOX (hbox), leaf->label, TRUE, TRUE, 0);

            gtk_drag_source_set (leaf->outer, GDK_BUTTON1_MASK, sink_target, 1, GDK_ACTION_MOVE);
            gtk_drag_source_set_icon_pixbuf (leaf->outer, leaf->paxui->sink_icon);
            g_signal_connect (leaf->outer, "drag-data-get", G_CALLBACK (any_drag_data_get), leaf);
            g_signal_connect (leaf->outer, "drag-end", G_CALLBACK (any_drag_end), leaf);
            break;
    }

    leaf_set_initial_pos (leaf);

    gtk_widget_show_all (leaf->outer);
}


void
leaf_gui_update (PaxuiLeaf *leaf)
{
    leaf_set_label (leaf);
    leaf_set_volume (leaf);
}


static gint
cmp_so_y (gconstpointer a, gconstpointer b)
{
    const PaxuiLeaf *so_a = a, *so_b = b;
    PaxuiLeaf *bk_a, *bk_b;
    Paxui *paxui = so_a->paxui;
    gint d;

    bk_a = (so_a->client == G_MAXUINT32 ?
                paxui_find_module_for_index (paxui, so_a->module) :
                paxui_find_client_for_index (paxui, so_a->client));
    if (bk_a == NULL) return 0;
    bk_b = (so_b->client == G_MAXUINT32 ?
                paxui_find_module_for_index (paxui, so_b->module) :
                paxui_find_client_for_index (paxui, so_b->client));
    if (bk_b == NULL) return 0;
    if ((d = bk_a->y - bk_b->y)) return d;

    bk_a = paxui_find_source_for_index (paxui, so_a->source);
    if (bk_a == NULL) return 0;
    bk_b = paxui_find_source_for_index (paxui, so_b->source);
    if (bk_b == NULL) return 0;

    return (bk_a->y - bk_b->y);
}

static gint
cmp_si_y (gconstpointer a, gconstpointer b)
{
    const PaxuiLeaf *si_a = a, *si_b = b;
    PaxuiLeaf *bk_a, *bk_b;
    Paxui *paxui = si_a->paxui;
    gint d;

    bk_a = (si_a->client == G_MAXUINT32 ?
                paxui_find_module_for_index (paxui, si_a->module) :
                paxui_find_client_for_index (paxui, si_a->client));
    if (bk_a == NULL) return 0;
    bk_b = (si_b->client == G_MAXUINT32 ?
                paxui_find_module_for_index (paxui, si_b->module) :
                paxui_find_client_for_index (paxui, si_b->client));
    if (bk_b == NULL) return 0;
    if ((d = bk_a->y - bk_b->y)) return d;

    bk_a = paxui_find_sink_for_index (paxui, si_a->sink);
    if (bk_a == NULL) return 0;
    bk_b = paxui_find_sink_for_index (paxui, si_b->sink);
    if (bk_b == NULL) return 0;

    return (bk_a->y - bk_b->y);
}

static void
paxui_gui_layout_siso (Paxui *paxui)
{
    GList *l;
    gint y;

    TRACE("layout siso");

    /* place source_outputs */
    paxui->source_outputs = g_list_sort (paxui->source_outputs, cmp_so_y);
    for (l = paxui->source_outputs, y = paxui->sosi_row1; l; l = l->next)
    {
        PaxuiLeaf *so = l->data;

        if (so->outer)
        {
            so->y = y;
            y += paxui->sosi_step;
        }
    }

    /* place sink_inputs */
    paxui->sink_inputs = g_list_sort (paxui->sink_inputs, cmp_si_y);
    for (l = paxui->sink_inputs, y = paxui->sosi_row1; l; l = l->next)
    {
        PaxuiLeaf *si = l->data;

        if (si->outer)
        {
            si->y = y;
            y += paxui->sosi_step;
        }
    }

    g_list_foreach (paxui->source_outputs, (GFunc) leaf_grid_attach, NULL);
    g_list_foreach (paxui->sink_inputs, (GFunc) leaf_grid_attach, NULL);
}

static void
layout_scms_column (GList **column, gint y1, gint dy)
{
    gint y;
    GList *l;

    *column = g_list_sort (*column, cmp_blocks_y);
    for (l = *column, y = y1; l; l = l->next)
    {
        PaxuiLeaf *lf = l->data;

        if (lf->outer)
        {
            lf->y = y;
            y += dy;
        }
    }
    g_list_foreach (*column, (GFunc) leaf_grid_attach, NULL);
}

static void
paxui_gui_layout_scms (Paxui *paxui)
{
    gint y1, dy;

    y1 = paxui->blk_row1;
    dy = paxui->blk_step;

    layout_scms_column (&paxui->sources, y1, dy);

    layout_scms_column (&paxui->acams, y1, dy);

    layout_scms_column (&paxui->sinks, y1, dy);
}

static void
set_view_state (Paxui *paxui)
{
    /* set layout constants for state of compact */
    switch (paxui->view_mode)
    {
        case 1:
            paxui->blk_row1 = 1;
            paxui->sosi_row1 = 2;
            paxui->blk_step = 2;
            paxui->sosi_step = 2;

            DBG("row layout: blocks first");
            break;
        case 2:
            paxui->blk_row1 = 2;
            paxui->sosi_row1 = 1;
            paxui->blk_step = 2;
            paxui->sosi_step = 2;

            DBG("row layout: siso first");
            break;
        default:
            paxui->blk_row1 = 1;
            paxui->sosi_row1 = 1;
            paxui->blk_step = 1;
            paxui->sosi_step = 1;

            DBG("row layout: compact");
            break;
    }
}

static gboolean
paxui_gui_layout_update (void *udata)
{
    Paxui *paxui = udata;

    TRACE("update layout");

    set_view_state (paxui);

    paxui_gui_layout_siso (paxui);

    paxui->updating = FALSE;

    gtk_widget_queue_draw (paxui->layout);

    return G_SOURCE_REMOVE;
}


void
paxui_gui_trigger_update (Paxui *paxui)
{
    if (paxui->updating) return;
    paxui->updating = TRUE;

    g_timeout_add (100, paxui_gui_layout_update, paxui);
}


static void
layout_draw_sink_inputs (cairo_t *cr, Paxui *paxui)
{
    GList *l;

    TRACE("draw si");

    for (l = paxui->sink_inputs; l; l = l->next)
    {
        PaxuiLeaf *si = l->data;
        PaxuiLeaf *sk = NULL;
        PaxuiLeaf *bk = NULL;
        PaxuiColour colour;
        gint si_x, si_y;
        GdkRectangle si_alloc;

        if (si->y < 1 || si->outer == NULL) continue;

        if (si->client == G_MAXUINT32)
        {
            PaxuiLeaf *md;

            md = paxui_find_module_for_index (paxui, si->module);
            if (md) bk = md;
        }
        else
        {
            PaxuiLeaf *cl;

            cl = paxui_find_client_for_index (paxui, si->client);
            if (cl) bk = cl;
        }
        if (bk == NULL) continue;

        if (si->colour < 0)
            si->colour = get_new_colour (paxui);

        colour = paxui->colours[si->colour];

        cairo_set_source_rgba (cr, CR_RBGA (colour));

        if (bk->outer && bk->y > 0)
        {
            gint bk_x, bk_y;
            GdkRectangle bk_alloc;

            gtk_widget_get_allocation (bk->outer, &bk_alloc);
            bk_x = bk_alloc.x + bk_alloc.width - PAXUI_LINE_WIDTH;
            bk_y = bk_alloc.y + bk_alloc.height / 2;

            gtk_widget_get_allocation (si->outer, &si_alloc);
            si_x = si_alloc.x + PAXUI_LINE_WIDTH;
            si_y = si_alloc.y + si_alloc.height / 2;

            cairo_move_to (cr, bk_x, bk_y);
            cairo_line_to (cr, si_x, si_y);
            cairo_stroke (cr);
        }

        sk = paxui_find_sink_for_index (paxui, si->sink);
        if (sk && sk->y && sk->outer)
        {
            gint sk_x, sk_y;
            GdkRectangle sk_alloc;

            gtk_widget_get_allocation (si->outer, &si_alloc);
            si_x = si_alloc.x + si_alloc.width - PAXUI_LINE_WIDTH;
            si_y = si_alloc.y + si_alloc.height / 2;

            gtk_widget_get_allocation (sk->outer, &sk_alloc);
            sk_x = sk_alloc.x + PAXUI_LINE_WIDTH;
            sk_y = sk_alloc.y + sk_alloc.height / 2;

            cairo_move_to (cr, si_x, si_y);
            cairo_line_to (cr, sk_x, sk_y);
            cairo_stroke (cr);
        }
    }
}

static void
layout_draw_source_outputs (cairo_t *cr, Paxui *paxui)
{
    GList *l;

    TRACE ("draw so");

    for (l = paxui->source_outputs; l; l = l->next)
    {
        PaxuiLeaf *so = l->data;
        PaxuiLeaf *sc = NULL;
        PaxuiLeaf *bk = NULL;
        PaxuiColour colour;
        gint so_x, so_y;
        GdkRectangle so_alloc;

        if (so->y < 1 || so->outer == NULL) continue;

        if (so->client == G_MAXUINT32)
        {
            PaxuiLeaf *md;

            md = paxui_find_module_for_index (paxui, so->module);
            if (md) bk = md;
        }
        else
        {
            PaxuiLeaf *cl;

            cl = paxui_find_client_for_index (paxui, so->client);
            if (cl) bk = cl;
        }
        if (bk == NULL) continue;

        if (so->colour < 0)
            so->colour = get_new_colour (paxui);

        colour = paxui->colours[so->colour];

        cairo_set_source_rgba (cr, CR_RBGA (colour));

        if (bk->outer && bk->y > 0)
        {
            gint bk_x, bk_y;
            GdkRectangle bk_alloc;

            gtk_widget_get_allocation (so->outer, &so_alloc);
            so_x = so_alloc.x + so_alloc.width - PAXUI_LINE_WIDTH;
            so_y = so_alloc.y + so_alloc.height / 2;

            gtk_widget_get_allocation (bk->outer, &bk_alloc);
            bk_x = bk_alloc.x + PAXUI_LINE_WIDTH;
            bk_y = bk_alloc.y + bk_alloc.height / 2;

            cairo_move_to (cr, so_x, so_y);
            cairo_line_to (cr, bk_x, bk_y);
            cairo_stroke (cr);
        }

        sc = paxui_find_source_for_index (paxui, so->source);
        if (sc && sc->y && sc->outer)
        {
            gint sc_x, sc_y;
            GdkRectangle sc_alloc;

            gtk_widget_get_allocation (sc->outer, &sc_alloc);
            sc_x = sc_alloc.x + sc_alloc.width - PAXUI_LINE_WIDTH;
            sc_y = sc_alloc.y + sc_alloc.height / 2;

            gtk_widget_get_allocation (so->outer, &so_alloc);
            so_x = so_alloc.x + + PAXUI_LINE_WIDTH;
            so_y = so_alloc.y + so_alloc.height / 2;

            cairo_move_to (cr, sc_x, sc_y);
            cairo_line_to (cr, so_x, so_y);
            cairo_stroke (cr);
        }
    }
}

static gboolean
layout_draw (GtkWidget *layout, cairo_t *cr, Paxui *paxui)
{
    GtkAdjustment *h, *v;

    TRACE("draw cb");

    h = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (paxui->layout));
    v = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (paxui->layout));
    cairo_translate (cr, -gtk_adjustment_get_value (h), -gtk_adjustment_get_value (v));

    cairo_set_line_width (cr, PAXUI_LINE_WIDTH);

    layout_draw_sink_inputs (cr, paxui);
    layout_draw_source_outputs (cr, paxui);

    return FALSE;
}


static void
grid_alloc (GtkWidget *grid, GdkRectangle *alloc, gpointer udata)
{
    Paxui *paxui = udata;

    DBG("grid alloc %dx%d", alloc->width, alloc->height);

    gtk_layout_set_size (GTK_LAYOUT (paxui->layout),
                         alloc->width  + 2*PAXUI_LAYOUT_PADDING,
                         alloc->height + 2*PAXUI_LAYOUT_PADDING);
}


static void
view_button_cb (GtkButton *button, gpointer udata)
{
    Paxui *paxui = udata;

    paxui->view_mode = (paxui->view_mode < 2 ? paxui->view_mode + 1 : 0);
    set_view_state (paxui);
    paxui_gui_layout_scms (paxui);
    paxui_gui_trigger_update (paxui);
}

void
paxui_gui_add_spinner (Paxui *paxui)
{
    paxui->spinner = gtk_spinner_new ();
    gtk_grid_attach (GTK_GRID (paxui->grid), paxui->spinner, PAXUI_COL_BLOCKS, 1, 1, 1);

    gtk_widget_show (paxui->spinner);
    gtk_spinner_start (GTK_SPINNER (paxui->spinner));
}

void
paxui_gui_rm_spinner (Paxui *paxui)
{
    if (paxui->spinner == NULL) return;

    gtk_widget_destroy (paxui->spinner);
    paxui->spinner = NULL;
}


static gboolean
window_delete_event (GtkWidget *window, GdkEvent *event, gpointer udata)
{
    Paxui *paxui = udata;
    PaxuiState state;

    DBG("window delete");

    paxui_pulse_stop_client (paxui);

    gtk_window_get_size (GTK_WINDOW (window), &state.width, &state.height);

    paxui_settings_save_state (paxui, &state);

    return FALSE;
}


static void
load_css (Paxui *paxui)
{
    GtkCssProvider *provider;
    gchar *css_file;
    const gchar *paxui_css;
    GError *err = NULL;

    TRACE("load css");

    /* load internal css */
    provider = gtk_css_provider_new();
    paxui_css = (paxui->dark_theme
        ?
            ".source        {background-color: shade("PAXUI_SC_COLOUR",.3);}"
            ".source-output {background-color: shade("PAXUI_SO_COLOUR",.3);}"
            ".client        {background-color: shade("PAXUI_CL_COLOUR",.3);}"
            ".module        {background-color: shade("PAXUI_MD_COLOUR",.3);}"
            ".sink-input    {background-color: shade("PAXUI_SI_COLOUR",.3);}"
            ".sink          {background-color: shade("PAXUI_SK_COLOUR",.3);}"
            ".outer         {border: 2px solid @theme_fg_color; border-radius: 10px;}"
            ".outer>box     {margin: 6px;}"
            ".outer label   {font-size: 112.5%;}"
            ".source-output label, .sink-input label {font-size: 87.5%;}"
            "checkbutton check    {padding-left: 0px; padding-right: 0px;}"
        :
            ".source        {background-color: "PAXUI_SC_COLOUR"}"
            ".source-output {background-color: "PAXUI_SO_COLOUR"}"
            ".client        {background-color: "PAXUI_CL_COLOUR"}"
            ".module        {background-color: "PAXUI_MD_COLOUR"}"
            ".sink-input    {background-color: "PAXUI_SI_COLOUR"}"
            ".sink          {background-color: "PAXUI_SK_COLOUR"}"
            ".outer         {border: 2px solid @theme_fg_color; border-radius: 10px;}"
            ".outer>box     {margin: 6px;}"
            ".outer label   {font-size: 112.5%;}"
            ".source-output label, .sink-input label {font-size: 87.5%;}"
            "checkbutton check    {padding-left: 0px; padding-right: 0px;}"
        );
    if (gtk_css_provider_load_from_data (provider, paxui_css, -1, &err))
    {
        gtk_style_context_add_provider_for_screen (
                        gdk_screen_get_default(),
                        GTK_STYLE_PROVIDER (provider),
                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    else
    {
        ERR("failed to load internal css");
        if (err)
        {
            DBG("    %s", err->message);
        }
    }
    if (err) g_error_free (err);
    g_object_unref (provider);

    /* attempt load of user css */
    provider = gtk_css_provider_new();
    css_file = g_build_filename (paxui->conf_dir, "paxui.css", NULL);

    if (gtk_css_provider_load_from_path (provider, css_file, &err))
    {
        gtk_style_context_add_provider_for_screen (
                        gdk_screen_get_default(),
                        GTK_STYLE_PROVIDER (provider),
                        GTK_STYLE_PROVIDER_PRIORITY_USER);
    }
    else
    {
        DBG("failed to load user css from '%s'", css_file);
        if (err)
        {
            DBG("    %s", err->message);
        }
    }
    if (err) g_error_free (err);
    g_free (css_file);
    g_object_unref (provider);
}

static void
paxui_gui_build_gui (Paxui *paxui)
{
    GtkWidget *scr, *label, *vbox, *image, *head, *view_button;

    TRACE("build gui");

    head = gtk_header_bar_new ();
    gtk_window_set_titlebar (GTK_WINDOW (paxui->window), head);
    gtk_header_bar_set_has_subtitle (GTK_HEADER_BAR (head), FALSE);
    gtk_header_bar_set_title (GTK_HEADER_BAR (head), "Paxui");
    gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (head), TRUE);
    gtk_header_bar_set_decoration_layout (GTK_HEADER_BAR (head),
                "icon:minimize,maximize,close");

    view_button = gtk_button_new ();
    gtk_widget_set_can_focus (view_button, FALSE);
    gtk_widget_set_tooltip_text (view_button, "switch layout");
    image = gtk_image_new_from_pixbuf (paxui->view_icon);
    gtk_button_set_image (GTK_BUTTON (view_button), image);
    gtk_header_bar_pack_start (GTK_HEADER_BAR (head), view_button);
    g_signal_connect (view_button, "clicked", G_CALLBACK (view_button_cb), paxui);

    gtk_widget_show_all (head);

    scr = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scr),
        GTK_POLICY_ALWAYS, GTK_POLICY_ALWAYS);
    gtk_container_add (GTK_CONTAINER (paxui->window), scr);

    paxui->layout = gtk_layout_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (scr), paxui->layout);
    g_signal_connect (paxui->layout, "draw", G_CALLBACK (layout_draw), paxui);

    paxui->grid = gtk_grid_new ();
    gtk_layout_put (GTK_LAYOUT (paxui->layout), paxui->grid,
                                PAXUI_LAYOUT_PADDING, PAXUI_LAYOUT_PADDING);
    gtk_grid_set_column_spacing (GTK_GRID (paxui->grid), 20);
    gtk_grid_set_row_spacing (GTK_GRID (paxui->grid), 20);
    g_signal_connect (paxui->grid, "size-allocate", G_CALLBACK (grid_alloc), paxui);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_grid_attach (GTK_GRID (paxui->grid), vbox, 0, 0, 1, 1);
    if (paxui->source_icon)
    {
        image = gtk_image_new_from_pixbuf (paxui->source_icon);
        gtk_container_add (GTK_CONTAINER (vbox), image);
    }
    label = gtk_label_new ("Sources");
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
    gtk_widget_set_valign (label, GTK_ALIGN_END);
    gtk_container_add (GTK_CONTAINER (vbox), label);

    label = gtk_label_new ("Source\nOutputs");
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
    gtk_widget_set_valign (label, GTK_ALIGN_END);
    gtk_grid_attach (GTK_GRID (paxui->grid), label, 1, 0, 1, 1);

    label = gtk_label_new ("Modules\n & Clients");
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
    gtk_widget_set_valign (label, GTK_ALIGN_END);
    gtk_grid_attach (GTK_GRID (paxui->grid), label, 2, 0, 1, 1);

    label = gtk_label_new ("Sink\nInputs");
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
    gtk_widget_set_valign (label, GTK_ALIGN_END);
    gtk_grid_attach (GTK_GRID (paxui->grid), label, 3, 0, 1, 1);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_grid_attach (GTK_GRID (paxui->grid), vbox, 4, 0, 1, 1);
    if (paxui->sink_icon)
    {
        image = gtk_image_new_from_pixbuf (paxui->sink_icon);
        gtk_container_add (GTK_CONTAINER (vbox), image);
    }
    label = gtk_label_new ("Sinks");
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
    gtk_widget_set_valign (label, GTK_ALIGN_END);
    gtk_container_add (GTK_CONTAINER (vbox), label);

    paxui_gui_add_spinner (paxui);

    gtk_widget_show_all (scr);

    g_signal_connect (paxui->window, "button-press-event", G_CALLBACK (paxui_actions_window_button_event), paxui);
    g_signal_connect (paxui->window, "delete-event", G_CALLBACK (window_delete_event), paxui);

    paxui_gui_layout_update (paxui);
}


void
paxui_gui_activate (GApplication *app, Paxui *paxui)
{
    GResource *resource;
    GdkPixbuf *logo = NULL;
    PaxuiState state = {640, 480};

    paxui_load_conf (paxui);
    paxui_settings_load_state (paxui, &state);

    set_view_state (paxui);

    paxui_pulse_start_client (paxui);
    g_idle_add ((GSourceFunc) paxui_pulse_connect_cb, paxui);

    resource = paxui_icons_get_resource ();
    if (resource)
    {
        logo = gdk_pixbuf_new_from_resource ("/org/paxui/logo.png", NULL);

        if (paxui->source_icon == NULL)
        {
            paxui->source_icon = gdk_pixbuf_new_from_resource (
                (paxui->dark_theme ? "/org/paxui/source-dark.png" : "/org/paxui/source.png"),
                NULL);
        }
        if (paxui->sink_icon == NULL)
        {
            paxui->sink_icon = gdk_pixbuf_new_from_resource (
                (paxui->dark_theme ? "/org/paxui/sink-dark.png" : "/org/paxui/sink.png"),
                NULL);
        }
        if (paxui->view_icon == NULL)
        {
            paxui->view_icon = gdk_pixbuf_new_from_resource (
                (paxui->dark_theme ? "/org/paxui/view-dark.png" : "/org/paxui/view.png"),
                NULL);
        }

        g_resource_unref (resource);
    }

    paxui->window = gtk_application_window_new (paxui->app);
    if (logo)
    {
        gtk_window_set_icon (GTK_WINDOW (paxui->window), logo);
        g_object_unref (logo);
    }

    load_css (paxui);
    paxui_gui_build_gui (paxui);

    if (paxui->view_icon)
    {
        g_object_unref (paxui->view_icon);
        paxui->view_icon = NULL;
    }

    gtk_window_resize (GTK_WINDOW (paxui->window), state.width, state.height);
    gtk_window_present (GTK_WINDOW (paxui->window));
}
