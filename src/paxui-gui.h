#ifndef _PAXUI_GUI_H_
#define _PAXUI_GUI_H_


void        paxui_gui_activate              (GApplication *app, Paxui *paxui);
void        paxui_gui_trigger_update        (Paxui *paxui);

void        paxui_gui_add_spinner           (Paxui *paxui);
void        paxui_gui_rm_spinner            (Paxui *paxui);

void        leaf_gui_new                    (PaxuiLeaf *leaf);
void        leaf_gui_update                 (PaxuiLeaf *leaf);

void        paxui_gui_colour_free           (Paxui *paxui, gint index);
void        paxui_gui_get_default_colours   (Paxui *paxui);

gint        paxui_cmp_blocks_y              (gconstpointer a, gconstpointer b);

#endif
