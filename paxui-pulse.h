#ifndef _PAXUI_PULSE_H_
#define _PAXUI_PULSE_H_


void        paxui_pulse_start_client        (Paxui *paxui);
void        paxui_pulse_stop_client         (Paxui *paxui);
gboolean    paxui_pulse_connect_cb          (Paxui *paxui);
void        paxui_pulse_move_source_output  (Paxui *paxui, guint32 so_index, guint32 sc_index);
void        paxui_pulse_move_sink_input     (Paxui *paxui, guint32 si_index, guint32 sk_index);
void        paxui_pulse_load_module         (Paxui *paxui, const gchar *mod_name, const gchar *mod_arg);
void        paxui_pulse_unload_module       (Paxui *paxui, guint32 index);

void        paxui_pulse_volume_set          (PaxuiLeaf *leaf);
void        paxui_pulse_mute_set            (PaxuiLeaf *leaf);


#endif
