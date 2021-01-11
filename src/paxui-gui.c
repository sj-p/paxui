#include <glib.h>
#include <gtk/gtk.h>

#include "paxui.h"
#include "paxui-gui.h"
#include "paxui-pulse.h"
#include "paxui-actions.h"
#include "paxui-data.h"


#define CR_RBGA(c) (double) (c).r/255, (double) (c).g/255, (double) (c).b/255, (double) (c).a/255

#define PAXUI_SPACE (24)
#define PAXUI_LINE_WIDTH (10)


enum
{
    PAXUI_COL_SOURCES = 0,
    PAXUI_COL_SRC_OPS,
    PAXUI_COL_BLOCKS,
    PAXUI_COL_SNK_IPS,
    PAXUI_COL_SINKS,
    PAXUI_COL_DUMMY
};


const GtkTargetEntry drop_target[] = {{"STRING", GTK_TARGET_SAME_APP|GTK_TARGET_OTHER_WIDGET, 2}};

const gchar *icon_paths[] = {
    "/org/paxui/source.png",
    "/org/paxui/sink.png",
    "/org/paxui/gear.png",
    "/org/paxui/muted.png",
    "/org/paxui/unmuted.png",
    "/org/paxui/locked.png",
    "/org/paxui/unlocked.png",
    "/org/paxui/switch.png"};

const gchar *dark_icon_paths[] = {
    "/org/paxui/source-dark.png",
    "/org/paxui/sink-dark.png",
    "/org/paxui/gear-dark.png",
    "/org/paxui/muted-dark.png",
    "/org/paxui/unmuted-dark.png",
    "/org/paxui/locked-dark.png",
    "/org/paxui/unlocked-dark.png",
    "/org/paxui/switch-dark.png"};


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


static void
add_new_row (Paxui *paxui)
{
    GtkWidget *label;

    DBG("adding dummy @ row:%d", paxui->end_row);

    label = gtk_label_new ("|");
    gtk_widget_set_vexpand (label, FALSE);
    gtk_widget_set_valign (label, GTK_ALIGN_START);
    gtk_style_context_add_class (gtk_widget_get_style_context (label), "dummy");
    gtk_size_group_add_widget (GTK_SIZE_GROUP (paxui->size_group), label);
    gtk_grid_attach (GTK_GRID (paxui->grid), label, PAXUI_COL_DUMMY, paxui->end_row++, 1, 1);
    gtk_widget_show (label);
}


void
paxui_gui_colour_free (Paxui *paxui, gint index)
{
    if (index < 0 || index >= paxui->num_colours || paxui->col_num[index] == 0) return;

    paxui->col_num[index]--;
}


static gboolean
window_focusin_cb (GtkWidget *window, GdkEvent *event, gpointer udata)
{
    gtk_style_context_add_class (gtk_widget_get_style_context (window), "active");

    return FALSE;
}

static gboolean
window_focusout_cb (GtkWidget *window, GdkEvent *event, gpointer udata)
{
    gtk_style_context_remove_class (gtk_widget_get_style_context (window), "active");

    return FALSE;
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
leaf_grid_reattach (PaxuiLeaf *leaf)
{
    if (leaf == NULL || leaf->outer == NULL) return;

    gtk_grid_attach (GTK_GRID (leaf->paxui->grid), leaf->outer, leaf->x, leaf->y, 1, 1);
    g_object_unref (leaf->outer);

    while (leaf->paxui->end_row - leaf->y <= 4) add_new_row (leaf->paxui);
}

static void
leaf_grid_detach (PaxuiLeaf *leaf)
{
    GtkWidget *parent;

    if (leaf == NULL || leaf->outer == NULL) return;

    g_object_ref (leaf->outer);
    if ((parent = gtk_widget_get_parent (leaf->outer)))
        gtk_container_remove (GTK_CONTAINER (parent), leaf->outer);
}

static void
leaf_grid_attach (PaxuiLeaf *leaf)
{
    leaf_grid_detach (leaf);
    leaf_grid_reattach (leaf);
}


gint
paxui_cmp_blocks_y (gconstpointer a, gconstpointer b)
{
    const PaxuiLeaf *bk_a = a, *bk_b = b;

    return (bk_a->y - bk_b->y);
}


static gboolean
scroll_cb (gpointer udata)
{
    GtkAdjustment *adj;
    gdouble value, upper, page_size;
    Paxui *paxui = udata;

    DBG("timed scroll %d", paxui->scroll_dir);

    adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (paxui->scr_win));
    value = gtk_adjustment_get_value (adj);
    upper = gtk_adjustment_get_upper (adj);
    page_size = gtk_adjustment_get_page_size (adj);

    if (value + page_size >= upper) add_new_row (paxui);

    gtk_adjustment_set_value (adj, value + paxui->scroll_dir * PAXUI_SPACE);

    return G_SOURCE_CONTINUE;
}

static gboolean
position_is_valid (PaxuiLeaf *leaf, gint x, gint y, gint *row, GdkRectangle *trg_pos, PaxuiLeaf **target)
{
    GtkWidget *w_ref;
    PaxuiLeaf *dest = NULL;
    GdkRectangle alloc;
    gint sx, sy;
    gint target_col, target_row, target_type, col_x, col_w, row_h;

    gtk_widget_translate_coordinates (leaf->paxui->layout, leaf->paxui->grid, x, y, &sx, &sy);
    x = sx + PAXUI_SPACE;
    y = sy + PAXUI_SPACE;

    switch (leaf->leaf_type)
    {
        case PAXUI_LEAF_TYPE_SOURCE_OUTPUT:
            target_type = PAXUI_LEAF_TYPE_SOURCE;
            break;
        case PAXUI_LEAF_TYPE_SINK_INPUT:
            target_type = PAXUI_LEAF_TYPE_SINK;
            break;
        default:
            target_type = leaf->leaf_type;
    }

    switch (target_type)
    {
        case PAXUI_LEAF_TYPE_SOURCE:
            target_col = PAXUI_COL_SOURCES;
            break;
        case PAXUI_LEAF_TYPE_CLIENT:
        case PAXUI_LEAF_TYPE_MODULE:
            target_col = PAXUI_COL_BLOCKS;
            break;
        case PAXUI_LEAF_TYPE_SINK:
            target_col = PAXUI_COL_SINKS;
            break;
        default:
            return FALSE;
    }

    w_ref = gtk_grid_get_child_at (GTK_GRID (leaf->paxui->grid), target_col, 0);
    gtk_widget_get_allocation (w_ref, &alloc);

    if (x < alloc.x || x >= alloc.x + alloc.width)
    {
        return FALSE;
    }
    col_x = alloc.x;
    col_w = alloc.width;

    w_ref = gtk_grid_get_child_at (GTK_GRID (leaf->paxui->grid), PAXUI_COL_DUMMY, 1);
    gtk_widget_get_allocation (w_ref, &alloc);

    row_h = alloc.y - PAXUI_SPACE;
    target_row = (y - PAXUI_SPACE) / row_h;
    if (target_row < 1) return FALSE;
    if ((y - target_row * row_h) >= row_h) return FALSE;

    w_ref = gtk_grid_get_child_at (GTK_GRID (leaf->paxui->grid), target_col, target_row);
    switch (target_type)
    {
        case PAXUI_LEAF_TYPE_SOURCE:
        case PAXUI_LEAF_TYPE_SINK:
            if (leaf->leaf_type == PAXUI_LEAF_TYPE_SOURCE_OUTPUT ||
                leaf->leaf_type == PAXUI_LEAF_TYPE_SINK_INPUT)
            {
                if (w_ref == NULL) return FALSE;

                dest = g_object_get_data (G_OBJECT (w_ref), "leaf");
                if (dest == NULL) return FALSE;

                if (leaf->leaf_type == PAXUI_LEAF_TYPE_SOURCE_OUTPUT &&
                    leaf->source == dest->index) return FALSE;
                if (leaf->leaf_type == PAXUI_LEAF_TYPE_SINK_INPUT &&
                    leaf->sink == dest->index) return FALSE;

                break;
            }
            /* fall through */
        case PAXUI_LEAF_TYPE_CLIENT:
        case PAXUI_LEAF_TYPE_MODULE:
            if (w_ref) return FALSE;
            break;
    }

    if (row) *row = target_row;
    if (trg_pos)
    {
        trg_pos->x = col_x;
        trg_pos->y = row_h * target_row + PAXUI_SPACE;
        trg_pos->width = col_w;
        trg_pos->height = alloc.height;
    }
    if (target) *target = dest;

    return TRUE;
}

static void
grid_drag_leave (GtkWidget *layout, GdkDragContext *context, guint time, gpointer udata)
{
    PaxuiLeaf *leaf;

    DBG("grid drag leave");

    leaf = g_object_get_data (G_OBJECT (gtk_drag_get_source_widget (context)), "leaf");
    if (leaf == NULL) return;

    if (leaf->paxui->scroll_src)
    {
        g_source_remove (leaf->paxui->scroll_src);
        leaf->paxui->scroll_src = 0;
    }

    if (leaf->paxui->drop_y < 1) return;

    leaf->paxui->drop_y = -1;
    gtk_widget_queue_draw (leaf->paxui->layout);
}

static gboolean
grid_drag_drop (GtkWidget *layout, GdkDragContext *context, gint x, gint y, guint time, gpointer udata)
{
    GtkWidget *w_src;
    gint target_row;
    PaxuiLeaf *leaf, *dest;

    DBG("grid drop %d,%d", x, y);

    w_src = gtk_drag_get_source_widget (context);
    leaf = g_object_get_data (G_OBJECT (w_src), "leaf");
    if (leaf == NULL) goto drop_exit;

    if (!position_is_valid (leaf, x, y, &target_row, NULL, &dest))
        goto drop_exit;

    switch (leaf->leaf_type)
    {
        case PAXUI_LEAF_TYPE_SOURCE_OUTPUT:
            DBG("  connecting so...");
            paxui_pulse_move_source_output (leaf->paxui, leaf->index, dest->index);
            break;
        case PAXUI_LEAF_TYPE_SINK_INPUT:
            DBG("  connecting si...");
            paxui_pulse_move_sink_input (leaf->paxui, leaf->index, dest->index);
            break;
        default:
            leaf->y = target_row;
            leaf_grid_attach (leaf);
    }

    paxui_gui_trigger_update (leaf->paxui);

drop_exit:
    gtk_drag_finish (context, TRUE, TRUE, time);

    return TRUE;
}

static gboolean
grid_drag_motion (GtkWidget *layout, GdkDragContext *context, gint x, gint y, guint time, gpointer udata)
{
    GtkWidget *w_src;
    GdkRectangle alloc;
    gint sx, sy, old_y;
    gboolean valid;
    PaxuiLeaf *leaf;

    DBG("grid motion %d,%d", x, y);

    w_src = gtk_drag_get_source_widget (context);
    leaf = g_object_get_data (G_OBJECT (w_src), "leaf");
    if (leaf == NULL) return FALSE;

    /* check vertical pos relative window, scroll if at edge */
    gtk_widget_translate_coordinates (layout, leaf->paxui->scr_win, x, y, &sx, &sy);
    gtk_widget_get_allocation (leaf->paxui->scr_win, &alloc);
    if (alloc.height - sy < PAXUI_SPACE*2)
    {
        DBG("    scroll down");
        leaf->paxui->scroll_dir = +1;
    }
    else if (sy < PAXUI_SPACE*2)
    {
        DBG("    scroll up");
        leaf->paxui->scroll_dir = -1;
    }
    else
    {
        leaf->paxui->scroll_dir = 0;
    }

    if (leaf->paxui->scroll_dir && !leaf->paxui->scroll_src)
    {
        leaf->paxui->scroll_src = g_timeout_add (100, scroll_cb, leaf->paxui);
    }
    else if (!leaf->paxui->scroll_dir && leaf->paxui->scroll_src)
    {
        g_source_remove (leaf->paxui->scroll_src);
        leaf->paxui->scroll_src = 0;
    }

    valid = (leaf->paxui->scroll_dir ?
                FALSE :
                position_is_valid (leaf, x, y, NULL, &alloc, NULL));

    DBG("  returning valid:%d", valid);

    old_y = leaf->paxui->drop_y;
    if (valid)
    {
        leaf->paxui->drop_x = alloc.x;
        leaf->paxui->drop_y = alloc.y;
        leaf->paxui->drop_w = alloc.width;
        leaf->paxui->drop_h = alloc.height;
    }
    else
    {
        leaf->paxui->drop_y = -1;
    }
    if (leaf->paxui->drop_y != old_y) gtk_widget_queue_draw (leaf->paxui->layout);

    return valid;
}


static gdouble
volume_uint_to_double (guint32 volume)
{
    return MIN ((gdouble)(guint)(((gdouble) volume / 655.35) + .2), 100.);
}

static guint32
volume_double_to_uint (gdouble volume)
{
    return MIN (655.35 * (guint)(volume + .2), 65535);
}

static void
slider_value_cb (GtkRange *range, gpointer udata)
{
    PaxuiLeaf *leaf = udata;
    gdouble value;
    guint32 level;
    guint i;

    value = gtk_range_get_value (GTK_RANGE (range));
    level = volume_double_to_uint (value);

    DBG("slider changed v:%f l:%u", value, level);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (leaf->lock_button)))
    {
        for (i = 0; i < leaf->n_chan; i++)
            leaf->levels[i] = level;
    }
    else
    {
        for (i = 0; i < leaf->n_chan; i++)
            if (leaf->sliders[i] == GTK_WIDGET (range))
            {
                DBG("    chan:%u changed", i);
                leaf->levels[i] = level;
                break;
            }
    }

    paxui_pulse_volume_set (leaf);
}

static void
lock_button_set (PaxuiLeaf *leaf)
{
    GtkWidget *image;

    image = gtk_button_get_image (GTK_BUTTON (leaf->lock_button));

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (leaf->lock_button)))
    {
        gtk_image_set_from_pixbuf (GTK_IMAGE (image), leaf->paxui->icons[PX_ICON_LOCKED]);
        gtk_widget_set_tooltip_text (leaf->lock_button, "Channels locked to same level");
    }
    else
    {
        gtk_image_set_from_pixbuf (GTK_IMAGE (image), leaf->paxui->icons[PX_ICON_UNLOCKED]);
        gtk_widget_set_tooltip_text (leaf->lock_button, "Channels independent");
    }
}

static void
mute_button_set (PaxuiLeaf *leaf)
{
    guint i;
    GtkWidget *image;

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (leaf->mute_button), leaf->muted);

    image = gtk_button_get_image (GTK_BUTTON (leaf->mute_button));
    if (leaf->muted)
    {
        gtk_image_set_from_pixbuf (GTK_IMAGE (image), leaf->paxui->icons[PX_ICON_MUTED]);
        gtk_widget_set_tooltip_text (leaf->mute_button, "All channels muted");
    }
    else
    {
        gtk_image_set_from_pixbuf (GTK_IMAGE (image), leaf->paxui->icons[PX_ICON_UNMUTED]);
        gtk_widget_set_tooltip_text (leaf->mute_button, "All channels unmuted");
    }

    for (i = 0; i < leaf->n_chan; i++)
        gtk_widget_set_sensitive (leaf->sliders[i], !leaf->muted);
}

static void
lock_button_cb (GtkToggleButton *button, gpointer udata)
{
    PaxuiLeaf *leaf = udata;

    DBG("lock button clicked");

    lock_button_set (leaf);
}

static void
mute_button_cb (GtkToggleButton *button, gpointer udata)
{
    gboolean muted;
    PaxuiLeaf *leaf = udata;

    muted = gtk_toggle_button_get_active (button);
    DBG("mute button clicked, :%d -> %d", leaf->muted, muted);

    if (leaf->muted != muted)
    {
        leaf->muted = muted;
        mute_button_set (leaf);

        paxui_pulse_mute_set (leaf);
    }
}

static void
leaf_set_volume (PaxuiLeaf *leaf)
{
    guint i;

    if (leaf->sliders == NULL) return;

    DBG("set volume, m:%d", leaf->muted);

    g_signal_handlers_block_by_func (leaf->mute_button, mute_button_cb, leaf);
    mute_button_set (leaf);
    g_signal_handlers_unblock_by_func (leaf->mute_button, mute_button_cb, leaf);

    for (i = 0; i < leaf->n_chan; i++)
    {
        gdouble level;

        level = volume_uint_to_double (leaf->levels[i]);

        DBG("    l:%d v:%f", leaf->levels[i], level);

        g_signal_handlers_block_by_func (leaf->sliders[i], slider_value_cb, leaf);
        gtk_range_set_value (GTK_RANGE (leaf->sliders[i]), level);
        g_signal_handlers_unblock_by_func (leaf->sliders[i], slider_value_cb, leaf);

        gtk_widget_set_sensitive (leaf->sliders[i], !leaf->muted);
    }
}


static void
tool_slider_new (PaxuiLeaf *leaf, GtkBox *sbox, guint index)
{
    GtkWidget *slider, *vbox, *label;
    GtkAdjustment *adj;

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start (sbox, vbox, FALSE, FALSE, 0);

    adj = gtk_adjustment_new (
                volume_uint_to_double (leaf->levels[index]), 0., 100., 1., 1., 0.);
    slider = gtk_scale_new (GTK_ORIENTATION_VERTICAL, adj);
    gtk_range_set_inverted (GTK_RANGE (slider), TRUE);
    gtk_range_set_round_digits (GTK_RANGE (slider), 0);
    gtk_scale_set_draw_value (GTK_SCALE (slider), FALSE);
    gtk_widget_set_halign (slider, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request (slider, -1, 120);
    gtk_widget_set_sensitive (slider, !leaf->muted);
    if (index == 0)
    {
        gtk_scale_add_mark (GTK_SCALE (slider), 0., GTK_POS_LEFT, "0");
        gtk_scale_add_mark (GTK_SCALE (slider),
                (leaf->paxui->tist_enabled ? 1000./11. : 100.),
                GTK_POS_LEFT,
                (leaf->paxui->tist_enabled ? "10" : "100"));
    }
    else if (index == leaf->n_chan - 1)
    {
        gtk_scale_add_mark (GTK_SCALE (slider), 0., GTK_POS_RIGHT, "0");
        gtk_scale_add_mark (GTK_SCALE (slider),
                (leaf->paxui->tist_enabled ? 1000./11. : 100.),
                GTK_POS_RIGHT,
                (leaf->paxui->tist_enabled ? "10" : "100"));
    }
    gtk_box_pack_start (GTK_BOX (vbox), slider, FALSE, FALSE, 0);

    label = gtk_label_new (paxui_pulse_channel_str (leaf->positions[index]));
    gtk_label_set_angle (GTK_LABEL (label), 90);
    gtk_style_context_add_class (gtk_widget_get_style_context (label), "channel");
    if (index == 0)
        gtk_widget_set_halign (label, GTK_ALIGN_END);
    else if (index == leaf->n_chan - 1)
        gtk_widget_set_halign (label, GTK_ALIGN_START);

    gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

    g_signal_connect (slider, "value-changed", G_CALLBACK (slider_value_cb), leaf);

    leaf->sliders[index] = slider;
}

static void
tool_clicked_cb (GtkWidget *button, PaxuiLeaf *leaf)
{
    DBG("tool clicked");

    if (leaf->popover) gtk_popover_popup (GTK_POPOVER (leaf->popover));
}

static void
build_tool_popover (PaxuiLeaf *leaf)
{
    GtkWidget *obox, *hbox, *sbox, *vbox;
    gboolean tools_added = FALSE;
    guint i;

    DBG("build tool");

    leaf->popover = gtk_popover_new (leaf->tool_button);
    gtk_container_set_border_width (GTK_CONTAINER (leaf->popover), 4);

    obox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_add (GTK_CONTAINER (leaf->popover), obox);

    if (!leaf->vol_enabled) goto skip_volume;

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);
    gtk_box_pack_start (GTK_BOX (obox), hbox, FALSE, FALSE, 0);

    sbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start (GTK_BOX (hbox), sbox, FALSE, FALSE, 0);

    leaf->sliders = g_new (GtkWidget *, leaf->n_chan);
    for (i = 0; i < leaf->n_chan; i++)
    {
        tool_slider_new (leaf, GTK_BOX (sbox), i);
    }

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_valign (vbox, GTK_ALIGN_CENTER);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

    leaf->mute_button = gtk_toggle_button_new ();
    gtk_button_set_image (GTK_BUTTON (leaf->mute_button), gtk_image_new ());
    mute_button_set (leaf);
    gtk_box_pack_start (GTK_BOX (vbox), leaf->mute_button, FALSE, FALSE, 0);
    g_signal_connect (leaf->mute_button, "toggled", G_CALLBACK (mute_button_cb), leaf);

    leaf->lock_button = gtk_toggle_button_new ();
    gtk_button_set_image (GTK_BUTTON (leaf->lock_button), gtk_image_new ());
    lock_button_set (leaf);
    gtk_box_pack_start (GTK_BOX (vbox), leaf->lock_button, FALSE, FALSE, 0);
    g_signal_connect (leaf->lock_button, "toggled", G_CALLBACK (lock_button_cb), leaf);

    tools_added = TRUE;

skip_volume:
    /* add any other tool widgets here */

    /* after adding any tool widgets */
    if (tools_added)
    {
        gtk_widget_show_all (obox);
    }
    else
    {
        gtk_widget_destroy (leaf->popover);
        leaf->popover = NULL;
        gtk_widget_set_sensitive (leaf->tool_button, FALSE);
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
            p = (leaf->utf8_name && g_str_has_prefix (leaf->utf8_name, "module-")
                    ? leaf->utf8_name + 7 : leaf->utf8_name);
            txt = g_markup_printf_escaped (
                            "%s\n<small>module #%u</small>",
                            (p ? p : ""),
                            leaf->index);
            gtk_label_set_markup (GTK_LABEL (leaf->label), txt);
            g_free (txt);
            break;
        case PAXUI_LEAF_TYPE_CLIENT:
            gtk_widget_set_tooltip_text (leaf->outer, leaf->utf8_name);
            txt = g_markup_printf_escaped (
                            "%s\n<small>client #%u</small>",
                            leaf->short_name,
                            leaf->index);
            gtk_label_set_markup (GTK_LABEL (leaf->label), txt);
            g_free (txt);
            break;
        case PAXUI_LEAF_TYPE_SOURCE:
            if (leaf->utf8_name && (p = strchr (leaf->utf8_name, '.')))
                str = g_string_new_len (leaf->utf8_name, p - leaf->utf8_name);
            else
                str = g_string_new (leaf->utf8_name);

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
            if (leaf->utf8_name && (p = strchr (leaf->utf8_name, '.')))
                str = g_string_new_len (leaf->utf8_name, p - leaf->utf8_name);
            else
                str = g_string_new (leaf->utf8_name);

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

    w = pango_font_description_get_size (g_value_get_boxed (&value)) * dpi / 10240;
    DBG("label width updated to w:%d", w);
    gtk_widget_set_size_request (label, w, -1);

    g_value_unset (&value);
}


static void
position_to_init_column (Paxui *paxui, GList *column, GList *init_list)
{
    gint y;
    GList *lc, *li;
    PaxuiLeaf *leaf;

    if (init_list == NULL) return;

    /* detach all in column & clear y coords */
    for (lc = column; lc; lc = lc->next)
    {
        leaf = lc->data;
        leaf_grid_detach (leaf);
        leaf->y = 0;
    }

    /* set y coord for items matched to init_list */
    for (li = init_list; li; li = li->next)
    {
        PaxuiInitItem *irec = li->data;

        for (lc = column; lc; lc = lc->next)
        {
            leaf = lc->data;

            if (leaf->y) continue;

            if (irec->str2)
            {
                PaxuiLeaf *module;

                module = paxui_find_module_for_index (paxui, leaf->module);
                if (module == NULL || irec->str1 == NULL ||
                    strcmp (irec->str1, module->name) || strcmp (irec->str2, leaf->name))
                {
                    continue;
                }
            }
            else if (irec->str1 == NULL || strcmp (irec->str1, leaf->name))
            {
                continue;
            }

            if (irec->y > 0) leaf->y = irec->y;

            break;
        }
    }

    /* attach any leaf with specified position that's free */
    for (lc = column, y = 1; lc; lc = lc->next)
    {
        leaf = lc->data;

        if (leaf->y > 0 &&
            gtk_grid_get_child_at (GTK_GRID (paxui->grid), leaf->x, leaf->y) == NULL)
        {
            y = leaf->y + 1;
            leaf_grid_reattach (leaf);
            gtk_widget_queue_draw (leaf->outer);
        }
        else
        {
            leaf->y = 0;
        }
    }
    /* attach remaining items below */
    for (lc = column; lc; lc = lc->next)
    {
        leaf = lc->data;

        if (leaf->y > 0) continue;

        while (gtk_grid_get_child_at (GTK_GRID (paxui->grid), leaf->x, y)) y++;

        leaf->y = y++;
        leaf_grid_reattach (leaf);
        gtk_widget_queue_draw (leaf->outer);
    }
}

static gboolean
position_to_init_full (gpointer udata)
{
    Paxui *paxui = udata;

    DBG("full repos to init states");

    position_to_init_column (paxui, paxui->sources, paxui->src_inits);
    position_to_init_column (paxui, paxui->acams, paxui->cam_inits);
    position_to_init_column (paxui, paxui->sinks, paxui->snk_inits);

    paxui_unload_init_strings (paxui);

    return G_SOURCE_REMOVE;
}


static void
leaf_set_initial_pos (PaxuiLeaf *leaf)
{
    gint y;

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

    for (y = 1; ; y++)
    {
        if (gtk_grid_get_child_at (GTK_GRID (leaf->paxui->grid), leaf->x, y) == NULL)
        {
            leaf->y = y;
            leaf_grid_attach (leaf);
            return;
        }
    }
}

static void
tool_button_add (PaxuiLeaf *leaf, GtkWidget *hbox)
{
    GtkWidget *vbox, *image;

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_valign (vbox, GTK_ALIGN_CENTER);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

    leaf->tool_button = gtk_button_new ();
    image = gtk_image_new_from_pixbuf (leaf->paxui->icons[PX_ICON_GEAR]);
    gtk_button_set_image (GTK_BUTTON (leaf->tool_button), image);
    gtk_container_add (GTK_CONTAINER (vbox), leaf->tool_button);

    g_signal_connect (leaf->tool_button, "clicked", G_CALLBACK (tool_clicked_cb), leaf);
}

void
leaf_gui_new (PaxuiLeaf *leaf)
{
    GtkWidget *hbox;

    if (leaf == NULL || leaf->outer) return;

    leaf->outer = gtk_event_box_new ();
    g_object_set_data (G_OBJECT (leaf->outer), "leaf", leaf);
    gtk_widget_set_tooltip_text (leaf->outer, leaf->utf8_name);
    gtk_widget_set_vexpand (leaf->outer, FALSE);
    gtk_size_group_add_widget (GTK_SIZE_GROUP (leaf->paxui->size_group), leaf->outer);
    gtk_style_context_add_class (gtk_widget_get_style_context (leaf->outer), "outer");
    if (leaf->paxui->dark_theme)
        gtk_style_context_add_class (gtk_widget_get_style_context (leaf->outer), "dark");

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_container_add (GTK_CONTAINER (leaf->outer), hbox);

    leaf->label = gtk_label_new (NULL);
    gtk_label_set_xalign (GTK_LABEL (leaf->label), 0.);
    gtk_label_set_line_wrap (GTK_LABEL (leaf->label), TRUE);
    gtk_label_set_line_wrap_mode (GTK_LABEL (leaf->label), PANGO_WRAP_WORD_CHAR);
    gtk_style_context_add_class (gtk_widget_get_style_context (leaf->label), "title");
    g_signal_connect (leaf->label, "style-updated", G_CALLBACK (label_style_updated), leaf);

    leaf_set_label (leaf);

    switch (leaf->leaf_type)
    {
        case PAXUI_LEAF_TYPE_MODULE:
            leaf->paxui->acams = g_list_prepend (leaf->paxui->acams, leaf);

            gtk_style_context_add_class (
                    gtk_widget_get_style_context (leaf->outer), "module");

            gtk_box_pack_start (GTK_BOX (hbox), leaf->label, TRUE, TRUE, 0);

            gtk_drag_source_set (leaf->outer, GDK_BUTTON1_MASK, drop_target, 1, GDK_ACTION_MOVE);
            gtk_drag_source_set_icon_pixbuf (leaf->outer, leaf->paxui->icons[PX_ICON_SWITCH]);
            g_signal_connect (leaf->outer, "button-press-event", G_CALLBACK (paxui_actions_block_button_event), leaf);
            break;
        case PAXUI_LEAF_TYPE_CLIENT:
            leaf->paxui->acams = g_list_prepend (leaf->paxui->acams, leaf);

            gtk_style_context_add_class (
                    gtk_widget_get_style_context (leaf->outer), "client");

            gtk_box_pack_start (GTK_BOX (hbox), leaf->label, TRUE, TRUE, 0);

            gtk_drag_source_set (leaf->outer, GDK_BUTTON1_MASK, drop_target, 1, GDK_ACTION_MOVE);
            gtk_drag_source_set_icon_pixbuf (leaf->outer, leaf->paxui->icons[PX_ICON_SWITCH]);
            break;
        case PAXUI_LEAF_TYPE_SOURCE:
            gtk_style_context_add_class (
                    gtk_widget_get_style_context (leaf->outer), "source");

            tool_button_add (leaf, hbox);
            build_tool_popover (leaf);

            gtk_box_pack_start (GTK_BOX (hbox), leaf->label, TRUE, TRUE, 0);

            gtk_drag_source_set (leaf->outer, GDK_BUTTON1_MASK, drop_target, 1, GDK_ACTION_MOVE);
            gtk_drag_source_set_icon_pixbuf (leaf->outer, leaf->paxui->icons[PX_ICON_SOURCE]);
            break;
        case PAXUI_LEAF_TYPE_SINK:
            gtk_style_context_add_class (
                    gtk_widget_get_style_context (leaf->outer), "sink");

            gtk_box_pack_start (GTK_BOX (hbox), leaf->label, TRUE, TRUE, 0);

            tool_button_add (leaf, hbox);
            build_tool_popover (leaf);

            gtk_drag_source_set (leaf->outer, GDK_BUTTON1_MASK, drop_target, 1, GDK_ACTION_MOVE);
            gtk_drag_source_set_icon_pixbuf (leaf->outer, leaf->paxui->icons[PX_ICON_SINK]);
            g_signal_connect (leaf->outer, "button-press-event", G_CALLBACK (paxui_actions_sink_button_event), leaf);
            break;
        case PAXUI_LEAF_TYPE_SOURCE_OUTPUT:
            gtk_style_context_add_class (
                    gtk_widget_get_style_context (leaf->outer), "source-output");

            gtk_box_pack_start (GTK_BOX (hbox), leaf->label, TRUE, TRUE, 0);

            tool_button_add (leaf, hbox);
            build_tool_popover (leaf);

            gtk_drag_source_set (leaf->outer, GDK_BUTTON1_MASK, drop_target, 1, GDK_ACTION_MOVE);
            gtk_drag_source_set_icon_pixbuf (leaf->outer, leaf->paxui->icons[PX_ICON_SOURCE]);
            break;
        case PAXUI_LEAF_TYPE_SINK_INPUT:
            gtk_style_context_add_class (
                    gtk_widget_get_style_context (leaf->outer), "sink-input");

            tool_button_add (leaf, hbox);
            build_tool_popover (leaf);

            gtk_box_pack_start (GTK_BOX (hbox), leaf->label, TRUE, TRUE, 0);

            gtk_drag_source_set (leaf->outer, GDK_BUTTON1_MASK, drop_target, 1, GDK_ACTION_MOVE);
            gtk_drag_source_set_icon_pixbuf (leaf->outer, leaf->paxui->icons[PX_ICON_SINK]);
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

    paxui->acams = g_list_sort (paxui->acams, paxui_cmp_blocks_y);

    /* place source_outputs */
    g_list_foreach (paxui->source_outputs, (GFunc) leaf_grid_detach, NULL);
    for (l = paxui->acams; l; l = l->next)
    {
        GList *c, *t;
        gint n;
        PaxuiLeaf *bl = l->data;

        t = NULL;
        n = 0;
        for (c = paxui->source_outputs; c; c = c->next)
        {
            PaxuiLeaf *so = c->data;

            if ((bl->leaf_type == PAXUI_LEAF_TYPE_CLIENT && so->client == bl->index) ||
                (bl->leaf_type == PAXUI_LEAF_TYPE_MODULE && so->module == bl->index))
            {
                t = g_list_append (t, so);
                n++;
            }
        }
        if (n == 0) continue;

        t = g_list_sort (t, cmp_so_y);
        y = MAX (1, bl->y - (n - 1) / 2);
        for (c = t; c; c = c->next)
        {
            PaxuiLeaf *so = c->data;

            while (gtk_grid_get_child_at (GTK_GRID (paxui->grid), PAXUI_COL_SRC_OPS, y)) y++;

            so->y = y;
            leaf_grid_reattach (so);
        }

        g_list_free (t);
    }

    /* place sink_inputs */
    g_list_foreach (paxui->sink_inputs, (GFunc) leaf_grid_detach, NULL);
    for (l = paxui->acams; l; l = l->next)
    {
        GList *c, *t;
        gint n;
        PaxuiLeaf *bl = l->data;

        t = NULL;
        n = 0;
        for (c = paxui->sink_inputs; c; c = c->next)
        {
            PaxuiLeaf *si = c->data;

            if ((bl->leaf_type == PAXUI_LEAF_TYPE_CLIENT && si->client == bl->index) ||
                (bl->leaf_type == PAXUI_LEAF_TYPE_MODULE && si->module == bl->index))
            {
                t = g_list_append (t, si);
                n++;
            }
        }
        if (n == 0) continue;

        t = g_list_sort (t, cmp_si_y);
        y = MAX (1, bl->y - (n - 1) / 2);
        for (c = t; c; c = c->next)
        {
            PaxuiLeaf *si = c->data;

            while (gtk_grid_get_child_at (GTK_GRID (paxui->grid), PAXUI_COL_SNK_IPS, y)) y++;

            si->y = y;
            leaf_grid_reattach (si);
        }

        g_list_free (t);
    }
}

static gboolean
paxui_gui_layout_update (void *udata)
{
    Paxui *paxui = udata;

    TRACE("update layout");

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
layout_draw_drop_zone (cairo_t *cr, Paxui *paxui)
{
    if (paxui->drop_y < 1) return;

    TRACE("draw drop zone");

    cairo_rectangle (cr, paxui->drop_x, paxui->drop_y, paxui->drop_w, paxui->drop_h);
    if (paxui->have_drop_colour)
    {
        cairo_set_source_rgba (cr,
                               paxui->drop_colour.red,
                               paxui->drop_colour.green,
                               paxui->drop_colour.blue,
                               paxui->drop_colour.alpha);
        cairo_stroke (cr);
    }
    else
    {
        cairo_set_source_rgb (cr, 1., 1., 1.);
        cairo_set_operator (cr, CAIRO_OPERATOR_EXCLUSION);
        cairo_stroke (cr);
        cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
    }
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

    layout_draw_drop_zone (cr, paxui);

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
                         alloc->width  + 2*PAXUI_SPACE,
                         alloc->height + 2*PAXUI_SPACE);

    if (paxui->pa_init_done)
    {
        paxui->pa_init_done = FALSE;

        g_idle_add (position_to_init_full, paxui);
    }
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
    GError *err = NULL;

    TRACE("load css");

    /* load internal css */
    provider = gtk_css_provider_new();
    gtk_css_provider_load_from_resource (provider, "/org/paxui/internal.css");
    gtk_style_context_add_provider_for_screen (
                    gdk_screen_get_default(),
                    GTK_STYLE_PROVIDER (provider),
                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
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
add_grid_head_item (Paxui *paxui, const gchar *title, GdkPixbuf *pb, gint column)
{
    GtkWidget *label, *vbox, *image;

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_valign (vbox, GTK_ALIGN_END);
    gtk_grid_attach (GTK_GRID (paxui->grid), vbox, column, 0, 1, 1);
    gtk_size_group_add_widget (GTK_SIZE_GROUP (paxui->size_group), vbox);

    if (pb)
    {
        image = gtk_image_new_from_pixbuf (pb);
        gtk_container_add (GTK_CONTAINER (vbox), image);
    }

    label = gtk_label_new (title);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
    gtk_widget_set_vexpand (label, TRUE);
    gtk_widget_set_valign (label, GTK_ALIGN_END);
    gtk_container_add (GTK_CONTAINER (vbox), label);
}

static void
paxui_gui_build_gui (Paxui *paxui)
{
    GtkWidget *head;
    gint i;

    TRACE("build gui");

    g_signal_connect (paxui->window, "focus-in-event",  G_CALLBACK (window_focusin_cb),  paxui);
    g_signal_connect (paxui->window, "focus-out-event", G_CALLBACK (window_focusout_cb), paxui);

    head = gtk_header_bar_new ();
    gtk_window_set_titlebar (GTK_WINDOW (paxui->window), head);
    gtk_header_bar_set_has_subtitle (GTK_HEADER_BAR (head), FALSE);
    gtk_header_bar_set_title (GTK_HEADER_BAR (head), "Paxui");
    gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (head), TRUE);
    gtk_header_bar_set_decoration_layout (GTK_HEADER_BAR (head),
                "icon:minimize,maximize,close");

    gtk_widget_show_all (head);

    paxui->scr_win = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (paxui->scr_win),
        GTK_POLICY_ALWAYS, GTK_POLICY_ALWAYS);
    gtk_container_add (GTK_CONTAINER (paxui->window), paxui->scr_win);

    paxui->layout = gtk_layout_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (paxui->scr_win), paxui->layout);
    gtk_drag_dest_set (paxui->layout, GTK_DEST_DEFAULT_MOTION, drop_target, 1, GDK_ACTION_MOVE);
    g_signal_connect (paxui->layout, "drag-motion", G_CALLBACK (grid_drag_motion), paxui);
    g_signal_connect (paxui->layout, "drag-leave", G_CALLBACK (grid_drag_leave), paxui);
    g_signal_connect (paxui->layout, "drag-drop", G_CALLBACK (grid_drag_drop), paxui);
    g_signal_connect (paxui->layout, "draw", G_CALLBACK (layout_draw), paxui);

    paxui->have_drop_colour =
            gtk_style_context_lookup_color (
                    gtk_widget_get_style_context (paxui->layout),
                    "theme_selected_bg_color",
                    &paxui->drop_colour);

    paxui->size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

    paxui->grid = gtk_grid_new ();
    gtk_layout_put (GTK_LAYOUT (paxui->layout), paxui->grid,
                                PAXUI_SPACE, PAXUI_SPACE);
    gtk_grid_set_column_spacing (GTK_GRID (paxui->grid), PAXUI_SPACE);
    gtk_grid_set_row_spacing (GTK_GRID (paxui->grid), PAXUI_SPACE);
    g_signal_connect (paxui->grid, "size-allocate", G_CALLBACK (grid_alloc), paxui);

    add_grid_head_item (paxui, "Sources", paxui->icons[PX_ICON_SOURCE], PAXUI_COL_SOURCES);
    add_grid_head_item (paxui, "Source\nOutputs", NULL, PAXUI_COL_SRC_OPS);
    add_grid_head_item (paxui, "Modules\n & Clients", NULL, PAXUI_COL_BLOCKS);
    add_grid_head_item (paxui, "Sink\nInputs", NULL, PAXUI_COL_SNK_IPS);
    add_grid_head_item (paxui, "Sinks", paxui->icons[PX_ICON_SINK], PAXUI_COL_SINKS);

    for (i = 0; i < 3; i++) add_new_row (paxui);

    paxui_gui_add_spinner (paxui);

    gtk_widget_show_all (paxui->scr_win);

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

    paxui_pulse_start_client (paxui);
    g_idle_add ((GSourceFunc) paxui_pulse_connect_cb, paxui);

    resource = paxui_data_get_resource ();
    if (resource)
    {
        gint i;

        logo = gdk_pixbuf_new_from_resource ("/org/paxui/logo.png", NULL);

        for (i = 0; i < PX_NUM_ICONS; i++)
        {
            if (paxui->icons[i] == NULL)
            {
                paxui->icons[i] = gdk_pixbuf_new_from_resource (
                    (paxui->dark_theme ? dark_icon_paths[i] : icon_paths[i]),
                    NULL);
            }
        }

        load_css (paxui);

        g_resource_unref (resource);
    }

    paxui->window = gtk_application_window_new (paxui->app);
    if (logo)
    {
        gtk_window_set_icon (GTK_WINDOW (paxui->window), logo);
        g_object_unref (logo);
    }

    paxui_gui_build_gui (paxui);

    gtk_window_resize (GTK_WINDOW (paxui->window), state.width, state.height);
    gtk_window_present (GTK_WINDOW (paxui->window));
}
