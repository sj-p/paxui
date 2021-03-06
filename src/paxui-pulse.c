#include <glib.h>
#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>

#include "paxui.h"
#include "paxui-pulse.h"
#include "paxui-gui.h"


#define PAXUI_CLIENT_NAME "Paxui"


/* First strips whitespace from the ends of long_name.
 * Then, if long_name contains more than n words, return n words
 * with the addition of ellipsis character in utf-8;
 * otherwise returns a copy of stripped long_name */
static gchar *
make_short_name (gchar *long_name, gint n)
{
    gchar *short_name, *p, *s;
    gboolean in_word;

    if (long_name == NULL || n < 1) return NULL;

    g_strstrip (long_name);

    for (p = long_name, s = p, in_word = TRUE; *p; p++)
    {
        if (g_ascii_isspace (*p))
        {
            if (in_word)
            {
                s = p;
                n--;
                if (n == 0) break;
                in_word = FALSE;
            }
        }
        else
        {
            in_word = TRUE;
        }
    }
    if (*p == '\0')
    {
        short_name = g_malloc (p - long_name + 1);
        strcpy (short_name, long_name);
    }
    else
    {
        short_name = g_malloc (s - long_name + 4);
        strncpy (short_name, long_name, s - long_name);
        strcpy (short_name + (s - long_name), PAXUI_UTF8_ELLIPSIS);
    }

    return short_name;
}

static gchar *
make_owner_module_name (const gchar *name, guint index)
{
    return g_strdup_printf ("%s:%u",
                (g_str_has_prefix (name, "module-") ? name + 7 : name),
                index);
}

static gchar *
make_utf8_name (const gchar *name)
{
    static const gchar *charset = NULL;
    static gboolean is_utf8;
    gchar *utf8;

    if (charset == NULL)
    {
        is_utf8 = g_get_charset (&charset);

        DBG("encoding is '%s'", charset);
    }

    if (is_utf8)
    {

        return g_utf8_make_valid (name, -1);
    }

    utf8 = g_locale_to_utf8 (name, -1, NULL, NULL, NULL);

    return (utf8 ? utf8 : g_utf8_make_valid (name, -1));
}

static void
leaf_get_strings (PaxuiLeaf *leaf, const gchar *name)
{
    g_free (leaf->name);
    g_free (leaf->short_name);
    g_free (leaf->utf8_name);

    leaf->name = g_strdup (name);

    switch (leaf->leaf_type)
    {
        PaxuiLeaf *md;

        case PAXUI_LEAF_TYPE_CLIENT:
            leaf->utf8_name = make_utf8_name (name);
            leaf->short_name = make_short_name (leaf->utf8_name, 3);
            break;

        case PAXUI_LEAF_TYPE_SOURCE_OUTPUT:
        case PAXUI_LEAF_TYPE_SINK_INPUT:
            leaf->utf8_name = make_utf8_name (name);
            if (leaf->client == G_MAXUINT32 &&
                (md = paxui_find_module_for_index (leaf->paxui, leaf->module)))
            {
                leaf->short_name = make_owner_module_name (md->utf8_name, md->index);
            }
            else
            {
                leaf->short_name = make_short_name (leaf->utf8_name, 3);
            }
            break;

        default:
            leaf->utf8_name = g_utf8_make_valid (name, -1);
            leaf->short_name = NULL;
            break;
    }
}


const gchar *
paxui_pulse_channel_str (guint32 pos)
{
    switch (pos)
    {
        case PA_CHANNEL_POSITION_MONO:
            return "M";

        case PA_CHANNEL_POSITION_LEFT:
            return "L";
        case PA_CHANNEL_POSITION_RIGHT:
            return "R";
        case PA_CHANNEL_POSITION_CENTER:
            return "C";

        case PA_CHANNEL_POSITION_REAR_CENTER:
            return "RC";
        case PA_CHANNEL_POSITION_REAR_LEFT:
            return "RL";
        case PA_CHANNEL_POSITION_REAR_RIGHT:
            return "RR";

        case PA_CHANNEL_POSITION_SUBWOOFER:
            return "SW";

        case PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER:
            return "FLC";
        case PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER:
            return "FRC";

        case PA_CHANNEL_POSITION_SIDE_LEFT:
            return "SL";
        case PA_CHANNEL_POSITION_SIDE_RIGHT:
            return "SR";

        case PA_CHANNEL_POSITION_AUX0:
            return "A0";
        case PA_CHANNEL_POSITION_AUX1:
            return "A1";
        case PA_CHANNEL_POSITION_AUX2:
            return "A2";
        case PA_CHANNEL_POSITION_AUX3:
            return "A3";
        case PA_CHANNEL_POSITION_AUX4:
            return "A4";
        case PA_CHANNEL_POSITION_AUX5:
            return "A5";
        case PA_CHANNEL_POSITION_AUX6:
            return "A6";
        case PA_CHANNEL_POSITION_AUX7:
            return "A7";
        case PA_CHANNEL_POSITION_AUX8:
            return "A8";
        case PA_CHANNEL_POSITION_AUX9:
            return "A9";
        case PA_CHANNEL_POSITION_AUX10:
            return "A10";
        case PA_CHANNEL_POSITION_AUX11:
            return "A11";
        case PA_CHANNEL_POSITION_AUX12:
            return "A12";
        case PA_CHANNEL_POSITION_AUX13:
            return "A13";
        case PA_CHANNEL_POSITION_AUX14:
            return "A14";
        case PA_CHANNEL_POSITION_AUX15:
            return "A15";
        case PA_CHANNEL_POSITION_AUX16:
            return "A16";
        case PA_CHANNEL_POSITION_AUX17:
            return "A17";
        case PA_CHANNEL_POSITION_AUX18:
            return "A18";
        case PA_CHANNEL_POSITION_AUX19:
            return "A19";
        case PA_CHANNEL_POSITION_AUX20:
            return "A20";
        case PA_CHANNEL_POSITION_AUX21:
            return "A21";
        case PA_CHANNEL_POSITION_AUX22:
            return "A22";
        case PA_CHANNEL_POSITION_AUX23:
            return "A23";
        case PA_CHANNEL_POSITION_AUX24:
            return "A24";
        case PA_CHANNEL_POSITION_AUX25:
            return "A25";
        case PA_CHANNEL_POSITION_AUX26:
            return "A26";
        case PA_CHANNEL_POSITION_AUX27:
            return "A27";
        case PA_CHANNEL_POSITION_AUX28:
            return "A28";
        case PA_CHANNEL_POSITION_AUX29:
            return "A29";
        case PA_CHANNEL_POSITION_AUX30:
            return "A30";
        case PA_CHANNEL_POSITION_AUX31:
            return "A31";

        case PA_CHANNEL_POSITION_TOP_CENTER:
            return "TC";

        case PA_CHANNEL_POSITION_TOP_FRONT_LEFT:
            return "TFL";
        case PA_CHANNEL_POSITION_TOP_FRONT_RIGHT:
            return "TFR";
        case PA_CHANNEL_POSITION_TOP_FRONT_CENTER:
            return "TFC";

        case PA_CHANNEL_POSITION_TOP_REAR_LEFT:
            return "TRL";
        case PA_CHANNEL_POSITION_TOP_REAR_RIGHT:
            return "TRR";
        case PA_CHANNEL_POSITION_TOP_REAR_CENTER:
            return "TRC";

        default:
            return "?";
    }
}

static const gchar *
event_fac_str (pa_subscription_event_type_t facility)
{
    switch (facility)
    {
        case PA_SUBSCRIPTION_EVENT_SERVER:
            return "server";
        case PA_SUBSCRIPTION_EVENT_SINK:
            return "sink";
        case PA_SUBSCRIPTION_EVENT_SOURCE:
            return "source";
        case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
            return "sink i/p";
        case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:
            return "source o/p";
        case PA_SUBSCRIPTION_EVENT_MODULE:
            return "module";
        case PA_SUBSCRIPTION_EVENT_CLIENT:
            return "client";
        default:
            return "unknown";
    }
}


static PaxuiLeaf *
paxui_find_source_output_for_module (const Paxui *paxui, guint32 index)
{
    GList *l;
    PaxuiLeaf *so;

    for (l = paxui->source_outputs; l; l = l->next)
    {
        so = l->data;

        if (so->module == index) return so;
    }

    return NULL;
}

static PaxuiLeaf *
paxui_find_sink_input_for_module (const Paxui *paxui, guint32 index)
{
    GList *l;
    PaxuiLeaf *si;

    for (l = paxui->sink_inputs; l; l = l->next)
    {
        si = l->data;

        if (si->module == index) return si;
    }

    return NULL;
}


void
paxui_pulse_mute_set (PaxuiLeaf *leaf)
{
    DBG("set mute i:%u m:%d", leaf->index, leaf->muted);

    switch (leaf->leaf_type)
    {
        case PAXUI_LEAF_TYPE_SOURCE:
            pa_context_set_source_mute_by_index (
                    leaf->paxui->pa_ctx, leaf->index, leaf->muted, NULL, NULL);
            break;
        case PAXUI_LEAF_TYPE_SINK:
            pa_context_set_sink_mute_by_index (
                    leaf->paxui->pa_ctx, leaf->index, leaf->muted, NULL, NULL);
            break;
        case PAXUI_LEAF_TYPE_SOURCE_OUTPUT:
            pa_context_set_source_output_mute (
                    leaf->paxui->pa_ctx, leaf->index, leaf->muted, NULL, NULL);
            break;
        case PAXUI_LEAF_TYPE_SINK_INPUT:
            pa_context_set_sink_input_mute (
                    leaf->paxui->pa_ctx, leaf->index, leaf->muted, NULL, NULL);
            break;
    }
}

void
paxui_pulse_volume_set (PaxuiLeaf *leaf)
{
    pa_cvolume volume;
    guint i;

    DBG("set volume i:%u", leaf->index);

    pa_cvolume_init (&volume);
    volume.channels = leaf->n_chan;
    for (i = 0; i < leaf->n_chan; i++)
        volume.values[i] = leaf->levels[i];

    switch (leaf->leaf_type)
    {
        case PAXUI_LEAF_TYPE_SOURCE:
            pa_context_set_source_volume_by_index (
                    leaf->paxui->pa_ctx, leaf->index, &volume, NULL, NULL);
            break;
        case PAXUI_LEAF_TYPE_SINK:
            pa_context_set_sink_volume_by_index (
                    leaf->paxui->pa_ctx, leaf->index, &volume, NULL, NULL);
            break;
        case PAXUI_LEAF_TYPE_SOURCE_OUTPUT:
            pa_context_set_source_output_volume (
                    leaf->paxui->pa_ctx, leaf->index, &volume, NULL, NULL);
            break;
        case PAXUI_LEAF_TYPE_SINK_INPUT:
            pa_context_set_sink_input_volume (
                    leaf->paxui->pa_ctx, leaf->index, &volume, NULL, NULL);
            break;
    }
}


void
paxui_pulse_load_module (Paxui *paxui, const gchar *mod_name, const gchar *mod_arg)
{
    pa_operation_unref (
        pa_context_load_module (paxui->pa_ctx, mod_name, mod_arg, NULL, NULL));
}

void
paxui_pulse_unload_module (Paxui *paxui, guint32 index)
{
    pa_operation_unref (
        pa_context_unload_module (paxui->pa_ctx, index, NULL, NULL));
}


void
paxui_pulse_move_source_output (Paxui *paxui, guint32 so_index, guint32 sc_index)
{
    GList *l;

    DBG("move source_output:%u to source:%u", so_index, sc_index);

    for (l = paxui->source_outputs; l; l = l->next)
    {
        PaxuiLeaf *so = l->data;

        if (so->index == so_index)
        {
            if (so->source == sc_index)
            {
                DBG("    already connected");
            }
            else
            {
                DBG("    moving...");

                pa_context_move_source_output_by_index (
                            paxui->pa_ctx, so_index, sc_index, NULL, NULL);
            }

            return;
        }
    }
    DBG("    unknown data");
}


void
paxui_pulse_move_sink_input (Paxui *paxui, guint32 si_index, guint32 sk_index)
{
    GList *l;

    DBG("move sink_input:%u to sink:%u", si_index, sk_index);

    for (l = paxui->sink_inputs; l; l = l->next)
    {
        PaxuiLeaf *si = l->data;

        if (si->index == si_index)
        {
            if (si->sink == sk_index)
            {
                DBG("    already connected");
            }
            else
            {
                DBG("    moving...");

                pa_context_move_sink_input_by_index (
                            paxui->pa_ctx, si_index, sk_index, NULL, NULL);
            }

            return;
        }
    }
    DBG("    unknown data");
}


static void
update_volume (PaxuiLeaf *leaf, const pa_cvolume *volume)
{
    guint i;

    for (i = 0; i < leaf->n_chan; i++)
        leaf->levels[i] = volume->values[i];
}


static void
copy_volume_chmap (PaxuiLeaf *leaf, const pa_cvolume *volume, const pa_channel_map *chmap)
{
    guint i;

    leaf->n_chan = volume->channels;
    leaf->n_pos = chmap->channels;

    leaf->levels = g_new (guint32, MAX (leaf->n_chan, leaf->n_pos));
    leaf->positions = g_new (guint32, MAX (leaf->n_chan, leaf->n_pos));

    for (i = 0; i < leaf->n_pos; i++)
        leaf->positions[i] = chmap->map[i];
}

static void
client_info_cb (pa_context *c, const pa_client_info *info, int eol, void *udata)
{
    Paxui *paxui = udata;
    PaxuiLeaf *client;
    gboolean is_new = FALSE;

    if (eol > 0 || info == NULL) return;

    DBG("client info index:%u '%s' mod:%u", info->index, info->name, info->owner_module);

    if ((client = paxui_find_client_for_index (paxui, info->index)))
    {
        TRACE("    have this");
    }
    else
    {
        is_new = TRUE;
        client = paxui_leaf_new (PAXUI_LEAF_TYPE_CLIENT);
        client->index = info->index;
        client->paxui = paxui;

        client->module = info->owner_module;

        paxui->clients = g_list_append (paxui->clients, client);
    }

    leaf_get_strings (client, info->name);

    if (client->outer == NULL)
    {
        paxui_block_update_active (client, paxui);
        if (client->active)
        {
            DBG("  adding gui");
            leaf_gui_new (client);
            paxui_gui_trigger_update (paxui);
        }
    }

    if (!is_new)
        leaf_gui_update (client);
}


static void
module_info_cb (pa_context *c, const pa_module_info *info, int eol, void *udata)
{
    PaxuiLeaf *module;
    Paxui *paxui = udata;
    gboolean is_new = FALSE;

    if (eol > 0 || info == NULL) return;

    DBG("module info index:%u '%s' '%s'", info->index, info->name, info->argument);

    if ((module = paxui_find_module_for_index (paxui, info->index)))
    {
        TRACE("    have this");
    }
    else
    {
        PaxuiLeaf *lf;

        is_new = TRUE;
        module = paxui_leaf_new (PAXUI_LEAF_TYPE_MODULE);
        module->index = info->index;
        module->paxui = paxui;

        paxui->modules = g_list_append (paxui->modules, module);

        leaf_get_strings (module, info->name);

        if ((lf = paxui_find_source_output_for_module (paxui, module->index))
            && lf->client == G_MAXUINT32)
        {
            DBG("    so for this module already exists");

            g_free (lf->short_name);
            lf->short_name = make_owner_module_name (module->utf8_name, info->index);
            leaf_gui_update (lf);
        }
        if ((lf = paxui_find_sink_input_for_module (paxui, module->index))
            && lf->client == G_MAXUINT32)
        {
            DBG("    si for this module already exists");

            g_free (lf->short_name);
            lf->short_name = make_owner_module_name (module->utf8_name, info->index);
            leaf_gui_update (lf);
        }
    }

    if (module->outer == NULL)
    {
        paxui_block_update_active (module, paxui);
        if (module->active)
        {
            leaf_gui_new (module);
            paxui_gui_trigger_update (paxui);
        }
    }

    if (!is_new)
        leaf_gui_update (module);
}


static void
source_output_info_cb (pa_context *c, const pa_source_output_info *info, int eol, void *udata)
{
    Paxui *paxui = udata;
    PaxuiLeaf *source_output;
    gboolean is_new = FALSE;

    if (eol > 0 || info == NULL) return;

    DBG("source_output info index:%u '%s' client:%u src:%u", info->index, info->name, info->client, info->source);

    if ((source_output = paxui_find_source_output_for_index (paxui, info->index)))
    {
        TRACE("    have this");
    }
    else
    {
        is_new = TRUE;
        source_output = paxui_leaf_new (PAXUI_LEAF_TYPE_SOURCE_OUTPUT);
        source_output->index = info->index;
        source_output->paxui = paxui;

        source_output->module = info->owner_module;
        source_output->client = info->client;

        source_output->vol_enabled = !paxui->volume_disabled && info->has_volume && info->volume_writable;
        if (source_output->vol_enabled)
            copy_volume_chmap (source_output, &info->volume, &info->channel_map);

        paxui->source_outputs = g_list_append (paxui->source_outputs, source_output);

    }

    source_output->source = info->source;

    leaf_get_strings (source_output, info->name);

    if (source_output->vol_enabled)
    {
        update_volume (source_output, &info->volume);
        source_output->muted = !!info->mute;
    }

    if (source_output->client != G_MAXUINT32)
    {
        PaxuiLeaf *cl;

        cl = paxui_find_client_for_index (paxui, source_output->client);
        if (cl && cl->outer == NULL)
        {
            DBG("    adding gui for client:%u", source_output->client);

            leaf_gui_new (cl);
            paxui_gui_trigger_update (paxui);
        }
    }
    else if (source_output->module != G_MAXUINT32)
    {
        PaxuiLeaf *md;

        md = paxui_find_module_for_index (paxui, source_output->module);
        if (md && md->outer == NULL)
        {
            DBG("    adding gui for module:%u", source_output->module);

            leaf_gui_new (md);
            paxui_gui_trigger_update (paxui);
        }
    }

    if (is_new)
    {
        leaf_gui_new (source_output);
        paxui_gui_trigger_update (paxui);
    }
    else
    {
        leaf_gui_update (source_output);
    }
}


static void
source_info_cb (pa_context *c, const pa_source_info *info, int eol, void *udata)
{
    Paxui *paxui = udata;
    PaxuiLeaf *source;
    gboolean is_new = FALSE;

    if (eol > 0 || info == NULL) return;

    DBG("source info index:%u '%s' '%s'", info->index, info->name, info->description);

    if ((source = paxui_find_source_for_index (paxui, info->index)))
    {
        TRACE("    have this");
    }
    else
    {
        is_new = TRUE;
        source = paxui_leaf_new (PAXUI_LEAF_TYPE_SOURCE);
        source->index = info->index;
        source->paxui = paxui;

        source->module = info->owner_module;
        source->monitor = info->monitor_of_sink;

        source->vol_enabled = !paxui->volume_disabled;
        if (source->vol_enabled)
            copy_volume_chmap (source, &info->volume, &info->channel_map);

        paxui->sources = g_list_append (paxui->sources, source);

        leaf_get_strings (source, info->name);
    }

    if (source->vol_enabled)
    {
        update_volume (source, &info->volume);
        source->muted = !!info->mute;
    }

    if (is_new)
        leaf_gui_new (source);
    else
        leaf_gui_update (source);

    paxui_gui_trigger_update (paxui);
}


static void
sink_input_info_cb (pa_context *c, const pa_sink_input_info *info, int eol, void *udata)
{
    Paxui *paxui = udata;
    PaxuiLeaf *sink_input;
    gboolean is_new = FALSE;

    if (eol > 0 || info == NULL) return;

    DBG("sink_input info index:%u '%s' client:%u sink:%u", info->index, info->name, info->client, info->sink);

    /* check non-client sink inputs */
    if (info->client == G_MAXUINT32)
    {
        PaxuiLeaf *module;

        module = paxui_find_module_for_index  (paxui, info->owner_module);

        if (module && g_strcmp0 (module->name, "module-loopback")) return;
    }

    if ((sink_input = paxui_find_sink_input_for_index (paxui, info->index)))
    {
        TRACE("    have this");
    }
    else
    {
        is_new = TRUE;
        sink_input = paxui_leaf_new (PAXUI_LEAF_TYPE_SINK_INPUT);
        sink_input->index = info->index;
        sink_input->paxui = paxui;

        sink_input->module = info->owner_module;
        sink_input->client = info->client;

        sink_input->vol_enabled = !paxui->volume_disabled && info->has_volume && info->volume_writable;
        if (sink_input->vol_enabled)
            copy_volume_chmap (sink_input, &info->volume, &info->channel_map);

        paxui->sink_inputs = g_list_append (paxui->sink_inputs, sink_input);
    }

    sink_input->sink = info->sink;

    leaf_get_strings (sink_input, info->name);

    if (sink_input->vol_enabled)
    {
        update_volume (sink_input, &info->volume);
        sink_input->muted = !!info->mute;
    }

    if (sink_input->client != G_MAXUINT32)
    {
        PaxuiLeaf *cl;

        cl = paxui_find_client_for_index (paxui, sink_input->client);
        if (cl && cl->outer == NULL)
        {
            DBG("    adding gui for client:%u", sink_input->client);

            leaf_gui_new (cl);
            paxui_gui_trigger_update (paxui);
        }
    }
    else if (sink_input->module != G_MAXUINT32)
    {
        PaxuiLeaf *md;

        md = paxui_find_module_for_index (paxui, sink_input->module);
        if (md && md->outer == NULL)
        {
            DBG("    adding gui for module:%u", sink_input->module);

            leaf_gui_new (md);
            paxui_gui_trigger_update (paxui);
        }
    }

    if (is_new)
    {
        leaf_gui_new (sink_input);
        paxui_gui_trigger_update (paxui);
    }
    else
    {
        leaf_gui_update (sink_input);
    }
}


static void
sink_info_cb (pa_context *c, const pa_sink_info *info, int eol, void *udata)
{
    Paxui *paxui = udata;
    PaxuiLeaf *sink;
    gboolean is_new = FALSE;

    if (eol > 0 || info == NULL) return;

    DBG("sink info index:%u '%s' '%s'", info->index, info->name, info->description);

    if ((sink = paxui_find_sink_for_index (paxui, info->index)))
    {
        TRACE("    have this");
    }
    else
    {
        is_new = TRUE;
        sink = paxui_leaf_new (PAXUI_LEAF_TYPE_SINK);
        sink->index = info->index;
        sink->paxui = paxui;

        sink->module = info->owner_module;
        sink->monitor = info->monitor_source;

        leaf_get_strings (sink, info->name);

        sink->vol_enabled = !paxui->volume_disabled;
        if (sink->vol_enabled)
            copy_volume_chmap (sink, &info->volume, &info->channel_map);

        paxui->sinks = g_list_append (paxui->sinks, sink);
    }

    if (sink->vol_enabled)
    {
        update_volume (sink, &info->volume);
        sink->muted = !!info->mute;
    }

    if (is_new)
        leaf_gui_new (sink);
    else
        leaf_gui_update (sink);

    paxui_gui_trigger_update (paxui);
}


static void
get_clients (Paxui *paxui)
{
    pa_operation_unref (
        pa_context_get_client_info_list (paxui->pa_ctx,
                                         (pa_client_info_cb_t) client_info_cb,
                                         paxui));
}

static void
get_modules (Paxui *paxui)
{
    pa_operation_unref (
        pa_context_get_module_info_list (paxui->pa_ctx,
                                         (pa_module_info_cb_t) module_info_cb,
                                         paxui));
}

static void
get_source_outputs (Paxui *paxui)
{
    pa_operation_unref (
        pa_context_get_source_output_info_list (paxui->pa_ctx,
                                       (pa_source_output_info_cb_t) source_output_info_cb,
                                       paxui));
}

static void
get_sources (Paxui *paxui)
{
    pa_operation_unref (
        pa_context_get_source_info_list (paxui->pa_ctx,
                                         (pa_source_info_cb_t) source_info_cb,
                                         paxui));
}

static void
get_sink_inputs (Paxui *paxui)
{
    pa_operation_unref (
        pa_context_get_sink_input_info_list (paxui->pa_ctx,
                                       (pa_sink_input_info_cb_t) sink_input_info_cb,
                                       paxui));
}

static void
get_sinks (Paxui *paxui)
{
    pa_operation_unref (
        pa_context_get_sink_info_list (paxui->pa_ctx,
                                       (pa_sink_info_cb_t) sink_info_cb,
                                       paxui));
}


static void
event_change (pa_context *c, pa_subscription_event_type_t facility, uint32_t idx, Paxui *paxui)
{
    DBG("event 'change'  fac:%s  id:%u", event_fac_str (facility), idx);

    switch (facility)
    {
        case PA_SUBSCRIPTION_EVENT_SOURCE:
            pa_operation_unref (
                pa_context_get_source_info_by_index (c, idx, source_info_cb, paxui));
            break;
        case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:
            pa_operation_unref (
                pa_context_get_source_output_info (c, idx, source_output_info_cb, paxui));
            break;
        case PA_SUBSCRIPTION_EVENT_SINK:
            pa_operation_unref (
                pa_context_get_sink_info_by_index (c, idx, sink_info_cb, paxui));
        case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
            pa_operation_unref (
                pa_context_get_sink_input_info (c, idx, sink_input_info_cb, paxui));
            break;
        case PA_SUBSCRIPTION_EVENT_CLIENT:
            pa_operation_unref (
                pa_context_get_client_info (c, idx, client_info_cb, paxui));
            break;
        case PA_SUBSCRIPTION_EVENT_MODULE:
            pa_operation_unref (
                pa_context_get_module_info (c, idx, module_info_cb, paxui));
            break;
        default:
            break;
    }
}

static void
event_new (pa_context *c, pa_subscription_event_type_t facility, uint32_t idx, Paxui *paxui)
{
    DBG("event 'new'     fac:%s  id:%u", event_fac_str (facility), idx);

    switch (facility)
    {
        case PA_SUBSCRIPTION_EVENT_SOURCE:
            pa_operation_unref (
                pa_context_get_source_info_by_index (c, idx, source_info_cb, paxui));
            break;
        case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:
            pa_operation_unref (
                pa_context_get_source_output_info (c, idx, source_output_info_cb, paxui));
            break;
        case PA_SUBSCRIPTION_EVENT_SINK:
            pa_operation_unref (
                pa_context_get_sink_info_by_index (c, idx, sink_info_cb, paxui));
            break;
        case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
            pa_operation_unref (
                pa_context_get_sink_input_info (c, idx, sink_input_info_cb, paxui));
            break;
        case PA_SUBSCRIPTION_EVENT_CLIENT:
            pa_operation_unref (
                pa_context_get_client_info (c, idx, client_info_cb, paxui));
            break;
        case PA_SUBSCRIPTION_EVENT_MODULE:
            pa_operation_unref (
                pa_context_get_module_info (c, idx, module_info_cb, paxui));
            break;
        default:
            break;
    }
}

static void
event_remove (pa_context *c, pa_subscription_event_type_t facility, uint32_t idx, Paxui *paxui)
{
    DBG("event 'remove'  fac:%s  id:%u", event_fac_str (facility), idx);

    switch (facility)
    {
        case PA_SUBSCRIPTION_EVENT_SOURCE:
            {
                PaxuiLeaf *sc;

                sc = paxui_find_source_for_index (paxui, idx);
                if (sc)
                {
                    paxui->sources = g_list_remove (paxui->sources, sc);

                    paxui_leaf_destroy (sc);

                    paxui_gui_trigger_update (paxui);
                }
            }
            break;
        case PA_SUBSCRIPTION_EVENT_SINK:
            {
                PaxuiLeaf *sk;

                sk = paxui_find_sink_for_index (paxui, idx);
                if (sk)
                {
                    paxui->sinks = g_list_remove (paxui->sinks, sk);
                    paxui_leaf_destroy (sk);

                    paxui_gui_trigger_update (paxui);
                }
            }
            break;
        case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:
            {
                PaxuiLeaf *so;

                so = paxui_find_source_output_for_index (paxui, idx);
                if (so)
                {
                    paxui->source_outputs = g_list_remove (paxui->source_outputs, so);
                    paxui_leaf_destroy (so);

                    paxui_gui_trigger_update (paxui);
                }
            }
            break;
        case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
            {
                PaxuiLeaf *si;

                si = paxui_find_sink_input_for_index (paxui, idx);
                if (si)
                {
                    paxui->sink_inputs = g_list_remove (paxui->sink_inputs, si);
                    paxui_leaf_destroy (si);

                    paxui_gui_trigger_update (paxui);
                }
            }
            break;
        case PA_SUBSCRIPTION_EVENT_CLIENT:
            {
                PaxuiLeaf *cl;

                cl = paxui_find_client_for_index (paxui, idx);
                if (cl)
                {
                    paxui->acams = g_list_remove (paxui->acams, cl);
                    paxui->clients = g_list_remove (paxui->clients, cl);
                    paxui_leaf_destroy (cl);

                    paxui_gui_trigger_update (paxui);
                }
            }
            break;
        case PA_SUBSCRIPTION_EVENT_MODULE:
            {
                PaxuiLeaf *md;

                md = paxui_find_module_for_index (paxui, idx);
                if (md)
                {
                    paxui->acams = g_list_remove (paxui->acams, md);
                    paxui->modules = g_list_remove (paxui->modules, md);
                    paxui_leaf_destroy (md);

                    paxui_gui_trigger_update (paxui);
                }
            }
            break;
        default:
            break;
    }
}

static void
event_cb (pa_context *c, pa_subscription_event_type_t t, uint32_t idx, Paxui *paxui)
{
    pa_subscription_event_type_t facility = t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;

    pa_subscription_event_type_t type = t & PA_SUBSCRIPTION_EVENT_TYPE_MASK;

    switch (type)
    {
        case PA_SUBSCRIPTION_EVENT_NEW:
            event_new (c, facility, idx, paxui);
            break;
        case PA_SUBSCRIPTION_EVENT_CHANGE:
            event_change (c, facility, idx, paxui);
            break;
        case PA_SUBSCRIPTION_EVENT_REMOVE:
            event_remove (c, facility, idx, paxui);
            break;
        default:
            DBG("event type:%d  fac:%s  id:%u", type, event_fac_str (facility), idx);
            break;
    }

}


static void
subscribed_cb (pa_context *c, int success, Paxui *paxui)
{
    DBG("subscribed sx %d", success);

    paxui_gui_rm_spinner (paxui);

    get_modules (paxui);
    get_sinks (paxui);
    get_sources (paxui);
    get_clients (paxui);
    get_sink_inputs (paxui);
    get_source_outputs (paxui);

    paxui->pa_init_done = TRUE;
}

static void
renew_connection (Paxui *paxui)
{
    pa_context_unref (paxui->pa_ctx);

    paxui_make_init_strings (paxui);

    paxui_unload_data (paxui);
    paxui_gui_add_spinner (paxui);

    paxui->pa_ctx = pa_context_new (paxui->pa_mlapi, PAXUI_CLIENT_NAME);
    g_idle_add ((GSourceFunc) paxui_pulse_connect_cb, paxui);
}

static void
context_state_cb (pa_context *ctx, Paxui *paxui)
{
    switch (pa_context_get_state (ctx))
    {
        case PA_CONTEXT_UNCONNECTED:
            DBG("PulseAudio context unconnected!");
            break;

        case PA_CONTEXT_READY:
            DBG("context ready");
            pa_context_set_subscribe_callback (
                    ctx, (pa_context_subscribe_cb_t) event_cb, paxui);

            pa_operation_unref (
                pa_context_subscribe (ctx,
                                      PA_SUBSCRIPTION_MASK_SINK |
                                      PA_SUBSCRIPTION_MASK_SOURCE |
                                      PA_SUBSCRIPTION_MASK_SINK_INPUT |
                                      PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT |
                                      PA_SUBSCRIPTION_MASK_MODULE |
                                      PA_SUBSCRIPTION_MASK_CLIENT |
                                      PA_SUBSCRIPTION_MASK_SERVER,
                                      (pa_context_success_cb_t) subscribed_cb,
                                      paxui)
                               );
            break;

        case PA_CONTEXT_FAILED:
            DBG("context failed!");
            renew_connection (paxui);
            break;

        case PA_CONTEXT_TERMINATED:
            DBG("context terminated!");
            if (paxui->terminated) break;

            renew_connection (paxui);
            break;

        case PA_CONTEXT_CONNECTING:
            DBG("connecting...");
            break;

        case PA_CONTEXT_AUTHORIZING:
            DBG("authorizing...");
            break;

        case PA_CONTEXT_SETTING_NAME:
            DBG("setting name...");
            break;
    }
}

static gboolean
make_connection (Paxui *paxui)
{
    int err;

    DBG("make_connection");

    paxui->pa_ctx = pa_context_new (paxui->pa_mlapi, PAXUI_CLIENT_NAME);
    pa_context_set_state_callback (
                paxui->pa_ctx, (pa_context_notify_cb_t) context_state_cb, paxui);

    if ((err = pa_context_connect (paxui->pa_ctx, NULL, PA_CONTEXT_NOFAIL, NULL)) < 0)
    {
        DBG("pa connect fail:%d", err);
        g_timeout_add (2000, (GSourceFunc) paxui_pulse_connect_cb, paxui);
        return FALSE;
    }

    return TRUE;
}

gboolean
paxui_pulse_connect_cb (Paxui *paxui)
{
    DBG("connect_cb");

    if (make_connection (paxui))
        return G_SOURCE_REMOVE;
    else
        return G_SOURCE_CONTINUE;
}



void
paxui_pulse_start_client (Paxui *paxui)
{
    /* create a mainloop & API */
    paxui->pa_ml = pa_glib_mainloop_new (NULL);
    paxui->pa_mlapi = pa_glib_mainloop_get_api (paxui->pa_ml);
}

void
paxui_pulse_stop_client (Paxui *paxui)
{
    if (paxui->terminated) return;

    paxui->terminated = TRUE;

    if (paxui->pa_ctx)
    {
        pa_context_disconnect (paxui->pa_ctx);
        pa_context_unref (paxui->pa_ctx);
        pa_glib_mainloop_free (paxui->pa_ml);
    }
}


