/*
 * Ayttm 
 *
 * Copyright (C) 2003, the Ayttm team
 * 
 * Ayttm is a derivative of Everybuddy
 * Copyright (C) 1999-2002, Torrey Searle <tsearle@uci.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "intl.h"

#include <gtk/gtk.h>
#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ui_prefs_window.h"

#include "service.h"
#include "util.h"
#include "libproxy/networking.h"
#include "sound.h"
#include "value_pair.h"
#include "gtk_globals.h"
#include "status.h"
/* #include "plugin.h" */
#include "file_select.h"
#include "smileys.h"

#include "gtkutils.h"

#ifdef HAVE_LIBASPELL
#include "spellcheck.h"
#endif

#include "prefs_window.h"
#include "plugin_api.h"
#include "plugin.h"

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 460

/* A set of preferences. Chat, Accounts, Services, etc. */
typedef struct PrefSet
{
	PreferenceSet offset;
	char *name;
	GtkWidget *panel;
	void (*init) (struct PrefSet *);
	PreferenceSet parent;
	GtkWidget *button;
	void *widgets;
	void *data;
} PrefSet;

/* Individual Preference Set widgets */
typedef struct {
	GtkWidget *tab_orientation;
	GtkWidget *tab_next_button;
	GtkWidget *tab_prev_button;
	GdkDeviceKey tab_next_key;
	GdkDeviceKey tab_prev_key;
	int accel_key_tag;
} ChatTabsWidgets;

typedef struct {
	GtkWidget *pref_button;
} AccountsWidgets;

typedef struct {
	GtkWidget *pref_button;
} ModuleWidgets;

/* columns in the left Preference set tree. Only PANEL_NAME)COL is visible */
enum {
	PANEL_NAME_COL,
	PANEL_POINTER_COL,
	COL_COUNT
};

static PrefSet prefset[PREFSET_COUNT]; 
static GtkWidget *dummy = NULL;
static GtkWidget *prefs_window = NULL;
static GtkAccelGroup *prefs_accel_group = NULL;
static int width = WINDOW_WIDTH, height = WINDOW_HEIGHT;
static gboolean inited = FALSE;
static GStringChunk *prefnames = NULL;
static AyModulePrefs *module_prefs = NULL;

static void _init_chat (PrefSet *me);
static void _init_chat_logs (PrefSet *me);
static void _init_chat_tabs (PrefSet *me);
static void _init_smileys (PrefSet *me);
static void _init_sound (PrefSet *me);
static void _init_sound_files (PrefSet *me);
static void _init_advanced (PrefSet *me);
static void _init_proxy (PrefSet *me);
static void _init_accounts (PrefSet *me);
static void _init_services (PrefSet *me);
static void _init_filters (PrefSet *me);
static void _init_utilities (PrefSet *me);
static void _init_importers (PrefSet *me);

#define INIT_PREFSET(num, n, i, p,w) {			\
		prefset[num].offset = num;		\
		prefset[num].name = n;			\
		prefset[num].init = i;			\
		prefset[num].widgets = w;		\
		prefset[num].parent = p;		\
	}

/* Initialize each of the Preference sets */
static void init_all_prefsets(void)
{
	if (inited)
		return;

	memset(prefset, 0, sizeof(PrefSet)*PREFSET_COUNT);

	INIT_PREFSET(PREFSET_CHAT, _("Chat"), _init_chat, PREFSET_NONE, NULL);
	INIT_PREFSET(PREFSET_CHAT_LOGS, _("Chat Logs"), _init_chat_logs, PREFSET_CHAT, NULL);
	INIT_PREFSET(PREFSET_CHAT_TABS, _("Chat Tabs"), _init_chat_tabs, PREFSET_CHAT, g_new0(ChatTabsWidgets, 1));
	INIT_PREFSET(PREFSET_SOUND, _("Sound"), _init_sound, PREFSET_NONE, NULL);
	INIT_PREFSET(PREFSET_SOUND_FILES, _("Sound Files"), _init_sound_files, PREFSET_SOUND, NULL);
	INIT_PREFSET(PREFSET_ACCOUNTS, _("Accounts"), _init_accounts, PREFSET_NONE, g_new0(AccountsWidgets, 1));
	INIT_PREFSET(PREFSET_SERVICES, _("Services"), _init_services, PREFSET_NONE, g_new0(ModuleWidgets, 1));
	INIT_PREFSET(PREFSET_FILTERS, _("Filters"), _init_filters, PREFSET_NONE, g_new0(ModuleWidgets, 1));
	INIT_PREFSET(PREFSET_UTILITIES, _("Utilities"), _init_utilities, PREFSET_NONE, g_new0(ModuleWidgets, 1));
	INIT_PREFSET(PREFSET_IMPORTERS, _("Importers"), _init_importers, PREFSET_NONE, g_new0(ModuleWidgets, 1));
	INIT_PREFSET(PREFSET_ADVANCED, _("Advanced"), _init_advanced, PREFSET_NONE, NULL);
	INIT_PREFSET(PREFSET_PROXY, _("Internet Proxy"), _init_proxy, PREFSET_ADVANCED, NULL);
	INIT_PREFSET(PREFSET_SMILEY, _("Smileys"), _init_smileys, PREFSET_CHAT, g_new0(ModuleWidgets, 1));
	inited = TRUE;
}
#undef INIT_PREFSET

static gboolean _is_parent (GtkTreeModel *model, GtkTreePath *path,
			    GtkTreeIter *iter, gpointer data)
{
	PrefSet *child = data;
	PrefSet *parent;

	gtk_tree_model_get(model, iter, PANEL_POINTER_COL, &parent, -1);

	if (parent->offset == child->parent) {
		child->data = gtk_tree_iter_copy(iter);
		return TRUE;
	}

	return FALSE;
}

/* Add a prefset to the tree */
static void _add_prefset_to_tree(int num, PrefSet *pset, GtkTreeStore *store)
{
	GtkTreeIter iter;

	if (pset->parent != PREFSET_NONE)
		gtk_tree_model_foreach(GTK_TREE_MODEL(store), _is_parent, pset);

	gtk_tree_store_append(store, &iter, pset->data);
	if (pset->data)
		gtk_tree_iter_free(pset->data);

	gtk_tree_store_set(store, &iter,
			   PANEL_NAME_COL, pset->name,
			   PANEL_POINTER_COL, pset,
			   -1);

	pset->init(pset);
}

static gboolean _is_iter_for_offset (GtkTreeModel *model, GtkTreePath *path,
				     GtkTreeIter *iter, gpointer data)
{
	PrefSet *target = data;
	PrefSet *source = NULL;

	gtk_tree_model_get(model, iter, PANEL_POINTER_COL, &source, -1);

	if (source->offset == target->offset) {
		target->data = gtk_tree_iter_copy(iter);
		return TRUE;
	}

	return FALSE;
}

static gboolean selection_changed (GtkTreeSelection *selection, gpointer data)
{
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	PrefSet *pset = NULL;
	GtkContainer *content_area = data;
	GtkWidget *new_widget = NULL;

	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gtk_tree_model_get(model, &iter,
				   PANEL_POINTER_COL, &pset,
				   -1);
		new_widget = pset->panel;
	} else
		new_widget = dummy;

	if (pset && new_widget) {
		GList *children = gtk_container_get_children(content_area);

		if (children)
			gtk_container_remove(content_area, GTK_WIDGET(children->data));

		gtk_container_add(content_area, pset->panel);
		gtk_widget_show_all(pset->panel);
	}

	return FALSE;
}

static void _populate_prefs (GtkContainer *parent, PreferenceSet def)
{
	GtkWidget *tree = NULL;
	GtkWidget *hpane = NULL;
	GtkWidget *scrollwindow = NULL;
	GtkTreeStore *treestore = NULL;
	GtkCellRenderer *renderer = NULL;
	GtkTreeViewColumn *column = NULL;
	GtkTreeSelection *selection = NULL;
	GtkWidget *frame = NULL;

	int i = 0;

	scrollwindow = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwindow),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollwindow),
					    GTK_SHADOW_ETCHED_IN);

	gtk_widget_set_size_request( scrollwindow, 150, -1);

	/* 
	 * Setting up the tree model. We will store two components in the tree: the
	 * preference and the pointer to the corresponding panel
	 */
	treestore = gtk_tree_store_new(COL_COUNT, G_TYPE_STRING,
				       G_TYPE_POINTER, G_TYPE_INT, -1);

	tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(treestore));

	/* Setting up the TreeView view */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Options", renderer, "text",
							  PANEL_NAME_COL, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	gtk_container_add(GTK_CONTAINER(scrollwindow), tree);

	dummy = gtk_label_new(_("Select an item on the left to see its preferences"));
	gtk_label_set_justify(GTK_LABEL(dummy), GTK_JUSTIFY_CENTER);

	hpane = gtk_hpaned_new();

	frame = gtk_frame_new(NULL);
	gtk_widget_set_size_request(frame, 400, -1);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);

	gtk_paned_add1(GTK_PANED(hpane), scrollwindow);
	gtk_paned_add2(GTK_PANED(hpane), frame);
	gtk_container_child_set(GTK_CONTAINER(hpane), scrollwindow, "shrink", FALSE, NULL);
	gtk_container_child_set(GTK_CONTAINER(hpane), frame, "shrink", FALSE, NULL);

	gtk_container_add(GTK_CONTAINER(frame), dummy);
	g_object_ref(dummy);

	/*
	 * Initialize each major preference set. Each of those sets will
	 * initialize their respective preferences or preference subsets.
	 */
	for (i = 0; i < PREFSET_COUNT; i++)
		_add_prefset_to_tree(i, &prefset[i], treestore);

	gtk_container_add(parent, hpane);

	gtk_tree_view_expand_all(GTK_TREE_VIEW(tree));

	/* Select a tree branch based on default */
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
	gtk_tree_model_foreach(GTK_TREE_MODEL(treestore), _is_iter_for_offset, &prefset[def]);
	gtk_tree_selection_select_iter(selection, prefset[def].data);
	gtk_tree_iter_free(prefset[def].data);
	prefset[def].data = NULL;

	g_signal_connect(selection, "changed", G_CALLBACK(selection_changed), frame);

	gtk_widget_show_all(hpane);
}

static void _unref_all(void)
{
	int i = 0;

	g_object_unref(dummy);

	for (i = 0; i < PREFSET_COUNT; i++)
		if (prefset[i].panel)
			g_object_unref(prefset[i].panel);
}


/* Open the preference window. This is the entry point. */
void ay_prefs_window_open (PreferenceSet def)
{

	if (prefs_window)
		return;

	init_all_prefsets();

	prefs_window = gtk_dialog_new_with_buttons( _("Ayttm Preferences"),
						    NULL,
						    GTK_DIALOG_NO_SEPARATOR,   /* We'll see if we like this or not */
						    NULL);

	gtk_container_set_border_width(GTK_CONTAINER(prefs_window), 5);

	prefs_accel_group = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(prefs_window), prefs_accel_group);

	/* 32 bytes ought to be enough for anybody */
	prefnames = g_string_chunk_new(32);

	module_prefs = ay_prefs_sift_modules();
	_populate_prefs(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(prefs_window))),
			def);

	if (width > gdk_screen_height())
		width = gdk_screen_width() - 40;

	if (height > gdk_screen_height())
		height = gdk_screen_height() - 40;

	gtk_window_set_default_size(GTK_WINDOW(prefs_window), width, height);

	gtk_dialog_run(GTK_DIALOG(prefs_window));

	ayttm_prefs_write();

	gtk_widget_destroy(prefs_window);
	prefs_window = NULL;
	g_string_chunk_free(prefnames);
	prefnames = NULL;

	ay_prefs_modules_free(module_prefs);

	_unref_all();
}


/* PrefSet Initializers:
 * 
 * The functions below initialize the panel for each Preference set.
 * Selection changes in the tree will then show the appropriate panel
 * on the right pane.
 */

static void _prefset_apply_boolean (GtkToggleButton *button, const char *prefname)
{
	if (gtk_toggle_button_get_active(button))
		iSetLocalPref(prefname, 1);
	else
		iSetLocalPref(prefname, 0);
}

static void _prefset_apply_string (GtkEntry *text, GdkEvent *event, char *prefname)
{
	cSetLocalPref(prefname, gtk_entry_get_text(text));
}

static void _prefset_apply_int (GtkEntry *text, GdkEvent *event, char *prefname)
{
	iSetLocalPref(prefname, atoi(gtk_entry_get_text(text)));
}

static GtkWidget *ay_pref_check_button (const char *text, char *prefname, GtkWidget *container)
{
	GtkWidget *button = gtk_check_button_new_with_mnemonic(text);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), iGetLocalPref(prefname));

	if (container)
		gtk_box_pack_start(GTK_BOX(container), button, FALSE, FALSE, 0);

	g_signal_connect(button, "clicked", G_CALLBACK(_prefset_apply_boolean), 
			 g_string_chunk_insert_const(prefnames, prefname));

	return button;
}

static GtkWidget *ay_pref_int_entry (char *prefname)
{
	GtkWidget *entry = gtk_entry_new();
	char *buf = g_strdup_printf("%d", iGetLocalPref(prefname));

	gtk_entry_set_text(GTK_ENTRY(entry), buf);
	g_signal_connect(entry, "focus-out-event", G_CALLBACK(_prefset_apply_int),
			 g_string_chunk_insert_const(prefnames, prefname));

	g_free(buf);
	return entry;
}

static GtkWidget *ay_pref_text_entry (char *prefname)
{
	GtkWidget *entry = gtk_entry_new();

	gtk_entry_set_text(GTK_ENTRY(entry), cGetLocalPref(prefname));
	g_signal_connect(entry, "focus-out-event", G_CALLBACK(_prefset_apply_string),
			 g_string_chunk_insert_const(prefnames, prefname));

	return entry;
}

GtkWidget *_prep_top_container(PrefSet *me)
{
	GtkWidget *title_label = NULL;
	GtkWidget *hbox = NULL;
	GtkWidget *frame = NULL;
	GtkWidget *ret = NULL;
	char *title = g_strdup_printf("<big><b>%s</b></big>", me->name);

	me->panel = gtk_vbox_new(FALSE, 5);
	g_object_ref(me->panel);

	title_label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(title_label), title);
	gtk_label_set_justify(GTK_LABEL(title_label), GTK_JUSTIFY_LEFT);
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), title_label, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(me->panel), hbox, FALSE, FALSE, 2);
	g_free(title);
	
	frame = gtk_frame_new(NULL);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);

	gtk_box_pack_start(GTK_BOX(me->panel), frame, TRUE, TRUE, 0);

	ret = gtk_vbox_new(FALSE, 2);
	gtk_container_set_border_width(GTK_CONTAINER(ret), 5);
	gtk_container_add(GTK_CONTAINER(frame), ret);

	return ret;
}

static void s_toggle_checkbox(GtkToggleButton *button, gpointer data)
{
	gtk_widget_set_sensitive(GTK_WIDGET(data), gtk_toggle_button_get_active(button));
}

static void _toggle_chatwindow_raise(GtkToggleButton *button, gpointer data)
{
	if (!gtk_toggle_button_get_active(button))
		gtk_entry_set_text(GTK_ENTRY(data), "");
}

static void _toggle_spellcheck(GtkToggleButton *button, gpointer data)
{
	if (!gtk_toggle_button_get_active(button))
		gtk_entry_set_text(GTK_ENTRY(data), "");
}

static void _chat_font_set(GtkWidget *button, gpointer data)
{
	char *prefname = data;
	const char *font = gtk_font_button_get_font_name(GTK_FONT_BUTTON(button));

	cSetLocalPref(prefname, font);
}

static void _init_chat (PrefSet *me)
{
	GtkWidget *hbox = NULL;
	GtkWidget *label = NULL;
	GtkWidget *button = NULL;
	GtkWidget *top_container = NULL;
	GtkWidget *m_regex_entry = NULL;
	GtkWidget *m_dictionary_entry = NULL;
	GtkWidget *fontbutton = NULL;
	GtkWidget *info_frame = NULL;
	GtkWidget *vbox = NULL;

	top_container = _prep_top_container(me);

	ay_pref_check_button(_("Send idle/away status to servers"),
			     "do_send_idle_time", top_container);
	ay_pref_check_button(_("Show timestamps in chat window"),
			     "do_convo_timestamp", top_container);

        // raise button
	button = ay_pref_check_button(_("Raise chat-window when receiving a message"),
				      "do_raise_window", top_container);

        // regex dialog
	hbox = gtk_hbox_new( FALSE, 0 );
	
	label = gtk_label_new(_("Only on regex:"));
	gtk_widget_set_size_request(label, 150, -1);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	m_regex_entry = ay_pref_text_entry("regex_pattern");
	gtk_box_pack_start(GTK_BOX(hbox), m_regex_entry, TRUE, TRUE, 10);
	
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(s_toggle_checkbox), hbox);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(_toggle_chatwindow_raise),
			 m_regex_entry);

	s_toggle_checkbox(GTK_TOGGLE_BUTTON(button), hbox);
	
	gtk_box_pack_start(GTK_BOX(top_container), hbox, FALSE, FALSE, 5);

	ay_pref_check_button(_("Ignore unknown people"), "do_ignore_unknown", top_container);
	ay_pref_check_button(_("On the fly chat completion"), "do_auto_complete", top_container);

#ifdef HAVE_LIBENCHANT
	button = ay_pref_check_button(_("Use spell checking"), "do_spell_checking", top_container);
	
	hbox = gtk_hbox_new( FALSE, 0 );
	
	label = gtk_label_new( _("Language:") );
	gtk_widget_set_size_request(label, 150, -1);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 0 );
	
	m_dictionary_entry = ay_pref_text_entry("spell_dictionary");
	gtk_box_pack_start(GTK_BOX(hbox), m_dictionary_entry, TRUE, TRUE, 10);

	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(s_toggle_checkbox), hbox);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(_toggle_spellcheck),
			 m_dictionary_entry);

	s_toggle_checkbox(GTK_TOGGLE_BUTTON(button), hbox);
	
	gtk_box_pack_start( GTK_BOX(top_container), hbox, FALSE, FALSE, 0 );
#endif
	
	// message received prefs
	info_frame = gtk_frame_new(_( "In messages I receive:" ));
	
	vbox = gtk_vbox_new(FALSE, 3);
	gtk_container_add(GTK_CONTAINER(info_frame), vbox);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);

	ay_pref_check_button(_("Ignore fonts"), "do_ignore_font", vbox);
	ay_pref_check_button(_("Ignore foreground colors"), "do_ignore_fore", vbox);
	ay_pref_check_button(_("Ignore background colors"), "do_ignore_back", vbox);
	
	gtk_box_pack_start(GTK_BOX(top_container), info_frame, FALSE, FALSE, 10);
	
	// font selection
	hbox = gtk_hbox_new( FALSE, 0 );

	label = gtk_label_new( _("Font: ") );
	gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 0 );

	fontbutton = gtk_font_button_new_with_font(cGetLocalPref("FontFace"));
	gtk_font_button_set_show_size(GTK_FONT_BUTTON(fontbutton), TRUE);
	gtk_font_button_set_title(GTK_FONT_BUTTON(fontbutton),
				  _("Select font for chat conversations"));

	g_signal_connect(fontbutton, "font-set", G_CALLBACK(_chat_font_set),
			 g_string_chunk_insert_const(prefnames, "FontFace"));

	gtk_box_pack_start( GTK_BOX(hbox), fontbutton, FALSE, FALSE, 3 );

	gtk_box_pack_start(GTK_BOX(top_container), hbox, FALSE, FALSE, 0);
}

static void _init_chat_logs (PrefSet *me)
{
	GtkWidget *top_container;

	top_container = _prep_top_container(me);

	ay_pref_check_button( _("Save all conversations to logfiles"), "do_logging",
		       top_container );
	ay_pref_check_button( _("Restore last conversation when opening a chat window"), 
		       "do_restore_last_conv", top_container );
}



gboolean s_newkey_callback (GtkWidget *keybutton, GdkEventKey *event, void *data)
{
	ChatTabsWidgets *widgets = data;
	GtkWidget *label = GTK_BIN(keybutton)->child;
	GdkDeviceKey *the_key = g_object_get_data(G_OBJECT(keybutton), "accel");
	char *which = g_object_get_data(G_OBJECT(keybutton), "which");
	char *accel_string = NULL;

	// IF the user hits escape
	//	THEN cancel the hotkey selection
	if ( event->keyval == GDK_Escape )
	{
		accel_string = gtk_accelerator_name(the_key->keyval, the_key->modifiers);
		gtk_label_set_text(GTK_LABEL(label), accel_string);
		g_signal_handler_disconnect(keybutton, widgets->accel_key_tag);
		gtk_grab_remove(keybutton);
		widgets->accel_key_tag = 0;
		g_free(accel_string);
		
		return(TRUE);
	}
	
	/* remove stupid things like.. numlock scrolllock and capslock
	 * mod1 = alt, mod2 = numlock, mod3 = modeshift/altgr, mod4 = meta, mod5 = scrolllock */
	const int state = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK |
					  GDK_MOD1_MASK | GDK_MOD4_MASK);
	
	if (state == 0)
		return(TRUE);

	/* this unfortunately was the only way I could do this without using
	 * a key release event
	 */
	switch(event->keyval)
	{
	case GDK_Shift_L:
	case GDK_Shift_R:
	case GDK_Control_L:
	case GDK_Control_R:
	case GDK_Caps_Lock:
	case GDK_Shift_Lock:
	case GDK_Meta_L:
	case GDK_Meta_R:
	case GDK_Alt_L:
	case GDK_Alt_R:
	case GDK_Super_L:
	case GDK_Super_R:
	case GDK_Hyper_L:
	case GDK_Hyper_R:
		// don't let the user set a modifier as a hotkey
		break;
		
	case GDK_Return:
		// don't let the user set Return as a hotkey
		break;
		
	default:
	{
		
		the_key->keyval = event->keyval;
		the_key->modifiers = state;
		
		accel_string = gtk_accelerator_name(the_key->keyval, the_key->modifiers);

		gtk_label_set_text(GTK_LABEL(label), accel_string);

		cSetLocalPref(which, accel_string);
		g_signal_handler_disconnect(keybutton, widgets->accel_key_tag);
		gtk_grab_remove(keybutton);
		widgets->accel_key_tag = 0;

		g_free(accel_string);
	}
	break;
	}
	
	/* eat the event and make focus keys (arrows) not change the focus */
	return(TRUE);
}

// getnewkey
void	_chat_tab_getnewkey( GtkWidget *keybutton, void *data )
{
	GtkWidget *label = GTK_BIN(keybutton)->child;
	ChatTabsWidgets *widgets = data;
	
	if ( widgets->accel_key_tag == 0 )
	{
		gtk_label_set_text(GTK_LABEL(label), _("<Press modifier + key or escape to cancel>"));

		/* it's sad how this works: It grabs the events in the event mask
		 * of the widget the mouse is over, NOT the grabbed widget.
		 * Oh, and persistantly clicking makes the grab go away...
		 */
		gtk_grab_add(keybutton);

		widgets->accel_key_tag = g_signal_connect_after(keybutton, "key_press_event",
								G_CALLBACK(s_newkey_callback), data);
	}
}

static void _add_key_set(const char *inLabelString, GdkDeviceKey *key, GtkWidget *button,
			 GtkWidget *vbox, ChatTabsWidgets *widgets)
{
	GtkWidget *hbox = gtk_hbox_new(FALSE, 2);
	GtkWidget *label = gtk_label_new(inLabelString);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	gtk_misc_set_alignment(GTK_MISC( label ), 0.0, 0.5);
	gtk_widget_set_size_request(label, 100, 10);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);

	g_object_set_data(G_OBJECT(button), "accel", key);
	g_signal_connect(button, "clicked", G_CALLBACK(_chat_tab_getnewkey), widgets);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 5);
}

static void _tab_orientation_changed(GtkComboBox *widget, gpointer data)
{
	char *prefname = data;

	iSetLocalPref(prefname, gtk_combo_box_get_active(widget));
}

static void _init_chat_tabs (PrefSet *me)
{
	GtkWidget *hbox;
	GtkWidget *top_container;
	GtkWidget *button;
	GtkWidget *label;
	GtkWidget *m_hotkey_frame;
	GtkWidget *hotkey_vbox;
	ChatTabsWidgets *widgets = me->widgets;
	GtkWidget *taboptions = NULL;
	char *next_key_name = NULL;
	char *prev_key_name = NULL;
	int do_tabbed_chat_orient = iGetLocalPref("do_tabbed_chat_orient");

	top_container = _prep_top_container(me);

	button = ay_pref_check_button(_("Use Tabs in chat windows"),
				      "do_tabbed_chat", top_container);

	hbox = gtk_hbox_new( FALSE, 0 );
	
	label = gtk_label_new(_("Tab Orientation:"));
	gtk_widget_set_size_request(label, 150, -1);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	taboptions = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(taboptions), 10);
	gtk_box_pack_start(GTK_BOX(top_container), taboptions, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(s_toggle_checkbox), taboptions);
	s_toggle_checkbox(GTK_TOGGLE_BUTTON(button), taboptions);

	widgets->tab_orientation = gtk_combo_box_new_text();

	gtk_combo_box_append_text(GTK_COMBO_BOX(widgets->tab_orientation), _("Bottom"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(widgets->tab_orientation), _("Top"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(widgets->tab_orientation), _("Left"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(widgets->tab_orientation), _("Right"));

	if (do_tabbed_chat_orient >= 0)
		gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->tab_orientation),
					 do_tabbed_chat_orient);
	else
		gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->tab_orientation), 0);

	g_signal_connect(widgets->tab_orientation, "changed", 
			 G_CALLBACK(_tab_orientation_changed),
			 g_string_chunk_insert_const(prefnames, "do_tabbed_chat_orient"));

	gtk_box_pack_start(GTK_BOX(hbox), widgets->tab_orientation, TRUE, TRUE, 10);
	
	gtk_box_pack_start(GTK_BOX(taboptions), hbox, FALSE, FALSE, 10);

	// hotkeys
	next_key_name = cGetLocalPref("accel_next_tab");
	prev_key_name = cGetLocalPref("accel_prev_tab");
	gtk_accelerator_parse(next_key_name,
		&(widgets->tab_next_key.keyval), &(widgets->tab_next_key.modifiers));
	gtk_accelerator_parse(prev_key_name,
		&(widgets->tab_prev_key.keyval), &(widgets->tab_prev_key.modifiers));

	m_hotkey_frame = gtk_frame_new(_("Hotkeys"));
	gtk_container_set_border_width( GTK_CONTAINER(m_hotkey_frame), 3 );
	
	hotkey_vbox = gtk_vbox_new( FALSE, 4 );
	gtk_widget_show( hotkey_vbox );
	gtk_container_add( GTK_CONTAINER(m_hotkey_frame), hotkey_vbox );

	widgets->tab_prev_button = gtk_button_new_with_label(prev_key_name);
	g_object_set_data(G_OBJECT(widgets->tab_prev_button), "which", 
			  g_string_chunk_insert_const(prefnames, "accel_prev_tab"));
	widgets->tab_next_button = gtk_button_new_with_label(next_key_name);
	g_object_set_data(G_OBJECT(widgets->tab_next_button), "which", 
			  g_string_chunk_insert_const(prefnames, "accel_next_tab"));
	_add_key_set(_("Previous tab:"), &widgets->tab_prev_key, widgets->tab_prev_button,
		     hotkey_vbox, widgets);
	_add_key_set(_("Next tab:"), &widgets->tab_next_key, widgets->tab_next_button,
		     hotkey_vbox, widgets);

	gtk_box_pack_start( GTK_BOX(taboptions), m_hotkey_frame, FALSE, FALSE, 5 );
}

static void _init_sound (PrefSet *me)
{
	GtkWidget *top_container;

	top_container = _prep_top_container(me);

	ay_pref_check_button(_("Disable sounds when I am away"),
			     "do_no_sound_when_away", top_container);
	ay_pref_check_button(_("Disable sounds for Ignored people"),
			     "do_no_sound_for_ignore", top_container);
	ay_pref_check_button(_("Play sounds when people sign on or off"),
			     "do_online_sound", top_container);
	ay_pref_check_button(_("Play a sound when sending a message"),
			     "do_play_send", top_container);
	ay_pref_check_button(_("Play a sound when receiving a message"),
			     "do_play_receive", top_container);
	ay_pref_check_button(_("Play a special sound when receiving first message"),
			     "do_play_first", top_container);
}

static void _sound_file_set (GtkFileChooserButton *button, gpointer data)
{
	char *prefname = data;
	char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(button));

	cSetLocalPref(prefname, filename);
}

static GtkWidget *_sound_file_add_line (const char *name, const char *prefname,
					GtkWidget *table, int y)
{
	GtkWidget *label;
	GtkWidget *button;
	char *filename = cGetLocalPref(prefname);

	label = gtk_label_new(name);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, y, y + 1, GTK_FILL, 0, 0, 0);

	button = gtk_file_chooser_button_new(name, GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(button), filename);
	g_signal_connect(button, "file-set", G_CALLBACK(_sound_file_set),
			 g_string_chunk_insert_const(prefnames, prefname));

	gtk_table_attach(GTK_TABLE(table), button, 1, 3, y, y + 1, GTK_FILL | GTK_EXPAND, 0, 0, 3);

//	button = gtk_button_new_with_label(_("Preview"));
//	gtk_table_attach(GTK_TABLE(table), button, 3, 4, y, y + 1, GTK_FILL | GTK_EXPAND, 0, 5, 0);

	return button;
}

static void _soundvolume_changed(GtkAdjustment *adjustment, gpointer data)
{
	char *prefname = data;
	fSetLocalPref(prefname, adjustment->value);
}

GtkWidget *_volume_selection(const char *inLabelString, GtkWidget *vbox, char *prefname)
{
	GtkWidget *hbox = gtk_hbox_new(FALSE, 3);
	GtkWidget *label = gtk_label_new(inLabelString);
	GtkObject *adjustment = gtk_adjustment_new(fGetLocalPref(prefname), 
						   -40, 0, 1, 5, 0 );

	GtkWidget *widget = gtk_hscale_new(GTK_ADJUSTMENT(adjustment));

	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3 );

	g_signal_connect(adjustment, "value_changed", G_CALLBACK(_soundvolume_changed), 
			 g_string_chunk_insert_const(prefnames, prefname));
	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	return widget;
}

static void _init_sound_files (PrefSet *me)
{
	GtkWidget *top_container;
	GtkWidget *arrive;
	GtkWidget *away;
	GtkWidget *leave;
	GtkWidget*send;
	GtkWidget *receive;
	GtkWidget *firstmsg;
	GtkWidget *volume;
	GtkWidget *table;

	top_container = _prep_top_container(me);

	table = gtk_table_new(6, 4, FALSE);

	arrive = _sound_file_add_line(_("Contact signs on: "), "BuddyArriveFilename", table, 0);
	away = _sound_file_add_line(_("Contact goes away: "), "BuddyAwayFilename", table, 1);
	leave = _sound_file_add_line(_("Contact signs off: "), "BuddyLeaveFilename", table, 2);
	send = _sound_file_add_line(_("Message sent: "), "SendFilename", table, 3);
	receive = _sound_file_add_line(_("Message received: "), "ReceiveFilename", table, 4);
	firstmsg = _sound_file_add_line(_("First message received: "), "FirstMsgFilename", table, 5);

	gtk_box_pack_start(GTK_BOX(top_container), table, FALSE, FALSE, 10);

	volume = _volume_selection(_("Relative volume (dB)"), top_container, "SoundVolume");
}

static void _set_alt_browser_path(GtkFileChooserButton *button, gpointer data)
{
	char *prefname = data;
	char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(button));
	char *cmd = g_strdup_printf("%s %%s", filename);

	cSetLocalPref(prefname, cmd);

	g_free(cmd);
}

static void _init_advanced (PrefSet *me)
{
	GtkWidget *hbox;
	GtkWidget *top_container;
	GtkWidget *brbutton;
	GtkWidget *label = NULL;
	GtkWidget *button;
	GtkFileFilter *filter;
	char *alt_browser = NULL;
	char *sep = NULL;

	top_container = _prep_top_container(me);

	brbutton = ay_pref_check_button(_("Use alternate web browser"), "use_alternate_browser",
					top_container);
	hbox = gtk_hbox_new( FALSE, 0 );
	gtk_widget_show( hbox );
	
	label = gtk_label_new( _("Browser command:") );
	gtk_widget_set_size_request(label, 150, -1);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 0 );

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Application programs and scripts"));
	gtk_file_filter_add_mime_type(filter, "application/x-executable");
	gtk_file_filter_add_mime_type(filter, "application/x-symlink");
	gtk_file_filter_add_mime_type(filter, "application/x-perl");
	gtk_file_filter_add_mime_type(filter, "application/x-python");
	gtk_file_filter_add_mime_type(filter, "application/x-shellscript");
	gtk_file_filter_add_mime_type(filter, "application/x-tex");

	alt_browser = g_strdup(cGetLocalPref("alternate_browser"));
	if ((sep = strchr(alt_browser, ' ')))
		*sep = '\0';

	button = gtk_file_chooser_button_new(_("Select alternate web browser"), GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_file_chooser_set_use_preview_label(GTK_FILE_CHOOSER(button), FALSE);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(button), filter);
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(button), alt_browser);
	g_signal_connect(button, "file-set", G_CALLBACK(_set_alt_browser_path),
			 g_string_chunk_insert_const(prefnames, "alternate_browser"));
	gtk_box_pack_start( GTK_BOX(hbox), button, TRUE, TRUE, 10 );

	g_free(alt_browser);

	g_signal_connect( brbutton, "clicked", G_CALLBACK(s_toggle_checkbox), hbox);

	s_toggle_checkbox(GTK_TOGGLE_BUTTON(brbutton), hbox);

	gtk_box_pack_start( GTK_BOX(top_container), hbox, FALSE, FALSE, 0 );

	ay_pref_check_button(_("Enable debug messages"), "do_ayttm_debug", top_container );
	ay_pref_check_button(_("Show tooltips in status window"), "do_show_tooltips",
			     top_container);	
	ay_pref_check_button(_("Check for latest version when signing on all"), "do_version_check",
			     top_container);
}

static gboolean _proxy_type_changed(GtkComboBox *combo, gpointer data)
{
	char *prefname = g_object_get_data(G_OBJECT(combo), "prefname");
	int active = gtk_combo_box_get_active(combo);

	gtk_widget_set_sensitive(GTK_WIDGET(data), (active != PROXY_NONE));
	iSetLocalPref(prefname, active);

	return FALSE;
}

static void _init_proxy (PrefSet *me)
{
	GtkWidget *proxy_type;
	GtkWidget *proxy_server;
	GtkWidget *proxy_port;
	GtkWidget *proxy_user;
	GtkWidget *proxy_password;
	GtkWidget *do_proxy_auth;
	GtkWidget *top_container;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *frame;
	GtkWidget *table;
	GtkWidget *auth_table;

	top_container = _prep_top_container(me);

	hbox = gtk_hbox_new(FALSE, 5);
	label = gtk_label_new(_("Use a Proxy to connect to the internet: "));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	proxy_type = gtk_combo_box_new_text();
	g_object_set_data(G_OBJECT(proxy_type), "prefname",
			  g_string_chunk_insert_const(prefnames, "proxy_type"));

	gtk_combo_box_append_text(GTK_COMBO_BOX(proxy_type), _("None"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(proxy_type), _("HTTP"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(proxy_type), _("SOCKS4"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(proxy_type), _("SOCKS5"));

	gtk_combo_box_set_active(GTK_COMBO_BOX(proxy_type), iGetLocalPref("proxy_type"));
	gtk_box_pack_start(GTK_BOX(hbox), proxy_type, FALSE, FALSE, 0);
	
	gtk_box_pack_start(GTK_BOX(top_container), hbox, FALSE, FALSE, 5);

	frame = gtk_frame_new(_("Proxy details"));
	table = gtk_table_new(3, 2, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	
	label = gtk_label_new( _("Server:") );
	gtk_misc_set_alignment(GTK_MISC(label ), 1.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, 0, 0, 10, 0);

	proxy_server = ay_pref_text_entry("proxy_host");
	gtk_table_attach(GTK_TABLE(table), proxy_server, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);
	
	// port
	label = gtk_label_new( _("Port:") );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, 0, 0, 10, 0);

	proxy_port = ay_pref_int_entry("proxy_port");
	gtk_table_attach(GTK_TABLE(table), proxy_port, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);
	
	// authentication
	do_proxy_auth = ay_pref_check_button(_("Proxy requires authentication"),
					     "do_proxy_auth", NULL);
	gtk_table_attach(GTK_TABLE(table), do_proxy_auth, 0, 2, 2, 3, GTK_FILL, 0, 5, 0);

	auth_table = gtk_table_new(2, 2, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(auth_table), 20);
	gtk_table_attach(GTK_TABLE(table), auth_table, 0, 2, 3, 4, GTK_FILL, 0, 0, 0);

	g_signal_connect(G_OBJECT(do_proxy_auth), "clicked", G_CALLBACK(s_toggle_checkbox),
		auth_table);

	// user ID
	label = gtk_label_new(_("User ID:"));
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_table_attach(GTK_TABLE(auth_table), label, 0, 1, 0, 1, GTK_FILL, 0, 10, 0);

	proxy_user = ay_pref_text_entry("proxy_user");
	gtk_table_attach(GTK_TABLE(auth_table), proxy_user, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);

	// password
	label = gtk_label_new(_("Password:"));
	gtk_misc_set_alignment(GTK_MISC( label ), 1.0, 0.5);
	gtk_table_attach(GTK_TABLE(auth_table), label, 0, 1, 1, 2, GTK_FILL, 0, 10, 0);

	proxy_password = ay_pref_text_entry("proxy_password");
	gtk_entry_set_visibility( GTK_ENTRY(proxy_password), FALSE );
	gtk_table_attach(GTK_TABLE(auth_table), proxy_password, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);

	_proxy_type_changed(GTK_COMBO_BOX(proxy_type), table);
	g_signal_connect(G_OBJECT(proxy_type), "changed", G_CALLBACK(_proxy_type_changed), table);

	gtk_container_add(GTK_CONTAINER(frame), table);
	gtk_box_pack_start(GTK_BOX(top_container), frame, FALSE, FALSE, 5);
}

/* BEGIN generic stuff for the accounts and modules lists */

static void _input_list_to_prefs_apply(input_list *the_list)
{
	while ( the_list != NULL )
	{
		switch(the_list->type)
		{
		case EB_INPUT_CHECKBOX:
			break;
			
		case EB_INPUT_PASSWORD:
		case EB_INPUT_ENTRY:
		{
			GtkWidget *entry_widget = (GtkWidget *)the_list->widget.entry.entry;
			const char *text = gtk_entry_get_text(GTK_ENTRY(entry_widget));
			
			strncpy(the_list->widget.entry.value, text, MAX_PREF_LEN);
			
			gtk_entry_set_text(GTK_ENTRY(entry_widget), the_list->widget.entry.value);
		}
		break;
		
		case EB_INPUT_LIST:
		{
			GtkWidget *list_widget = (GtkWidget *)the_list->widget.listbox.widget;
			*the_list->widget.listbox.value = gtk_combo_box_get_active(
				GTK_COMBO_BOX(list_widget));
		}
		break;
		}
		
		the_list = the_list->next;
	}
}

static gboolean _input_list_to_prefs_dialog(const char *dialog_title, input_list *ilist,
					    char *message)
{
	GtkWidget *dialog;
	input_list *the_list = ilist;
	GtkResponseType result = GTK_RESPONSE_NONE;
	GtkWidget *content_area = NULL, *real_content_area = NULL;
	GtkWidget *frame = NULL;
	GtkAccelGroup *m_accel_group = gtk_accel_group_new();

	dialog = gtk_dialog_new_with_buttons(dialog_title,
					     GTK_WINDOW(prefs_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);

	gtk_window_add_accel_group(GTK_WINDOW(dialog), m_accel_group);

	real_content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	frame = gtk_frame_new(NULL);

	gtk_box_pack_start(GTK_BOX(real_content_area), frame, TRUE, TRUE, 0);

	gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);

	content_area = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(content_area), 5);

	gtk_container_add(GTK_CONTAINER(frame), content_area);

	if (message) {
		char *text = g_strdup_printf("<i><b>Note:</b> %s</i>", message);
		GtkWidget *label = gtk_label_new(NULL);
		gtk_label_set_markup(GTK_LABEL(label), text);
		gtk_misc_set_padding(GTK_MISC(label), 0, 10);
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_box_pack_start(GTK_BOX(content_area), label, FALSE, FALSE, 0);

		g_free(text);
	}

	while ( the_list != NULL )
	{
		switch ( the_list->type )
		{
		case EB_INPUT_HIDDEN:
			break;
			
		case EB_INPUT_CHECKBOX:
		{
			char *item_label = NULL;
			
			if (the_list->label != NULL)
				item_label = the_list->label;
			else
				item_label = the_list->name;
			
			gtkut_button(item_label, the_list->widget.checkbox.value, content_area, m_accel_group);
			the_list->widget.checkbox.saved_value = *(the_list->widget.checkbox.value);
		}
		break;
		
		case EB_INPUT_ENTRY:
		{
			GtkWidget *hbox = gtk_hbox_new(FALSE, 3);
			gtk_widget_show(hbox);
			
			char *item_label = NULL;
			
			if ( the_list->label != NULL )
				item_label = the_list->label;
			else
				item_label = the_list->name;
			
			GtkWidget *label = gtk_label_new_with_mnemonic(item_label);
			int key = gtk_label_get_mnemonic_keyval(GTK_LABEL(label));
			gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
			gtk_widget_set_size_request( label, 130, 15 );
			gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 0 );
			
			GtkWidget *widget = gtk_entry_new();
			gtk_widget_add_accelerator(widget, "grab_focus", m_accel_group,
						   key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
			the_list->widget.entry.entry = widget;
			gtk_entry_set_text(GTK_ENTRY(widget), the_list->widget.entry.value);
			gtk_editable_set_position(GTK_EDITABLE(widget), 0);
			gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);
			
			gtk_box_pack_start(GTK_BOX(content_area), hbox, FALSE, FALSE, 0);
			
		}
		break;
		
		case EB_INPUT_PASSWORD:
		{
			GtkWidget *hbox = gtk_hbox_new( FALSE, 3 );
			gtk_widget_show( hbox );
			
			char *item_label = NULL;
			
			if ( the_list->label != NULL )
				item_label = the_list->label;
			else
				item_label = the_list->name;
			
			GtkWidget *label = gtk_label_new_with_mnemonic( item_label );
			int key = gtk_label_get_mnemonic_keyval(GTK_LABEL(label));
			gtk_widget_show( label );
			gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
			gtk_widget_set_size_request( label, 130, 15 );
			gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 0 );
			
			GtkWidget *widget = gtk_entry_new();
			gtk_entry_set_visibility(GTK_ENTRY(widget), FALSE);
			gtk_widget_show( widget );
			gtk_widget_add_accelerator(widget, "grab_focus", m_accel_group,
						   key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
			the_list->widget.entry.entry = widget;
			gtk_entry_set_text( GTK_ENTRY(widget), the_list->widget.entry.value );
			gtk_editable_set_position( GTK_EDITABLE(widget), 0 );
			gtk_box_pack_start( GTK_BOX(hbox), widget, TRUE, TRUE, 0 );
			
			gtk_box_pack_start(GTK_BOX(content_area), hbox, FALSE, FALSE, 0);
			
		}
		break;
		
		case EB_INPUT_LIST:
		{
			GtkWidget *hbox = gtk_hbox_new( FALSE, 3 );
			gtk_widget_show( hbox );
			
			char *item_label = NULL;
			
			if ( the_list->label != NULL )
				item_label = the_list->label;
			else
				item_label = the_list->name;
			
			GtkWidget *label = gtk_label_new( item_label );
			int key = gtk_label_get_mnemonic_keyval(GTK_LABEL(label));
			gtk_widget_show( label );
			gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
			gtk_widget_set_size_request( label, 130, 15 );
			gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 0 );
			
			GtkWidget *menu = gtk_combo_box_new_text();
			gtk_widget_show(menu);
			the_list->widget.listbox.widget = menu;
			
			int i; LList *l;
			for(i=0, l=the_list->widget.listbox.list; l; l=l_list_next(l), i++) {
				char *label = (char *)l->data;
				gtk_combo_box_append_text(GTK_COMBO_BOX(menu), label);
			}
			
			gtk_combo_box_set_active(GTK_COMBO_BOX(menu), *the_list->widget.listbox.value);
			
			gtk_widget_add_accelerator(menu, "grab_focus", m_accel_group,
						   key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
			
			gtk_box_pack_start( GTK_BOX(hbox), menu, TRUE, TRUE, 0 );
			
			gtk_box_pack_start(GTK_BOX(content_area), hbox, FALSE, FALSE, 0);
			
		}
		break;
		
		default:
			assert( FALSE );
			break;
		}
		
		the_list = the_list->next;
	}

	gtk_widget_show_all(dialog);

	if ((result = gtk_dialog_run(GTK_DIALOG(dialog))) == GTK_RESPONSE_ACCEPT)
		_input_list_to_prefs_apply(ilist);

	gtk_widget_destroy(dialog);
	return result;
}

/* BEGIN Account Preferences */
enum {
	ACCOUNT_CONN_COL,
	ACCOUNT_PROT_COL,
	ACCOUNT_USER_COL,
	ACCOUNT_POINTER_COL,
	ACCOUNT_COL_COUNT
};

static void _account_list_append(void *item, void *data)
{
	eb_local_account *ela = item;
	GtkListStore *store = data;
	GtkTreeIter iter;

	gtk_list_store_append(store, &iter);

	gtk_list_store_set(store, &iter,
			   ACCOUNT_CONN_COL, ela->connecting | ela->connected,
			   ACCOUNT_PROT_COL, get_service_name(ela->service_id),
			   ACCOUNT_USER_COL, ela->handle,
			   ACCOUNT_POINTER_COL, ela,
			   -1);
}

static gboolean _account_connect_toggle (GtkCellRendererToggle *toggle, gchar *path,
					 gpointer data)
{
	GtkTreeModel *model = data;
	GtkTreeIter iter;
	gboolean connected = TRUE;
	eb_local_account *ela = NULL;

	gtk_tree_model_get_iter_from_string(model, &iter, path);

	gtk_tree_model_get(model, &iter,
			   ACCOUNT_CONN_COL, &connected,
			   ACCOUNT_POINTER_COL, &ela,
			   -1);

	if (!connected)
		RUN_SERVICE(ela)->login(ela);
	else
		RUN_SERVICE(ela)->logout(ela);

	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
			   ACCOUNT_CONN_COL, ela->connecting | ela->connected,
			   ACCOUNT_POINTER_COL, ela,
			   -1);

	return FALSE;
}

static gboolean account_prefs(GtkWidget *button, gpointer data)
{
	char *dialog_title;
	GtkTreeSelection *selection = data;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	eb_local_account *account = NULL;
	LList *accountprefs;
	char *buf;
	gboolean connected = FALSE;
	char *message = NULL;

	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return FALSE;

	gtk_tree_model_get(model, &iter,
			   ACCOUNT_CONN_COL, &connected,
			   ACCOUNT_POINTER_COL, &account,
			   -1);

	buf = g_strdup_printf("%s:%s", get_service_name(account->service_id),
				    account->handle);

	accountprefs = GetPref(buf);

	if (connected)
		message = _("Account will disconnect if preferences are saved");

	dialog_title = g_strdup_printf(_("Account Preferences for %s"), 
				       account->handle);

	if (_input_list_to_prefs_dialog(dialog_title, account->prefs, message) == GTK_RESPONSE_ACCEPT) {
		RUN_SERVICE(account)->logout(account);
		write_account_list();

		g_free(buf);
		buf = g_strdup_printf("%s:%s", get_service_name(account->service_id),
				      account->handle);

		SetPref(buf, accountprefs);

		/* TODO Update the chat menu */

		gtk_list_store_set(GTK_LIST_STORE(model), &iter,
				   ACCOUNT_USER_COL, account->handle,
				   -1);
	}

	g_free(dialog_title);
	g_free(buf);

	return FALSE;
}

static void account_selected (GtkTreeSelection *selection, gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	PrefSet *me = data;
	eb_local_account *account = NULL;
	AccountsWidgets *widgets = me->widgets;
	int tag = -1;

	tag = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widgets->pref_button), "clicktag"));
	if(tag)
		g_signal_handler_disconnect(widgets->pref_button, tag);
	
	gtk_widget_set_sensitive(widgets->pref_button, FALSE);

	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gtk_tree_model_get(model, &iter,
				   ACCOUNT_POINTER_COL, &account,
				   -1);

		tag = g_signal_connect(widgets->pref_button, "clicked", G_CALLBACK(account_prefs), selection);
		g_object_set_data(G_OBJECT(widgets->pref_button), "clicktag", GINT_TO_POINTER(tag));

		gtk_widget_set_sensitive(widgets->pref_button, TRUE);
	}
}

static gboolean account_doubleclick (GtkTreeView *tree, GtkTreePath *path,
				     GtkTreeViewColumn *column, gpointer data)
{
	account_prefs(NULL, gtk_tree_view_get_selection(tree));

	return FALSE;
}

static void _init_accounts (PrefSet *me)
{
	GtkListStore *store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *scrollwindow;
	GtkWidget *tree;
	GtkWidget *buttonbox;
	AccountsWidgets *widgets;
	GtkTreeSelection *selection = NULL;
	GtkWidget *top_container;

	top_container = _prep_top_container(me);

	scrollwindow = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwindow),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollwindow),
					    GTK_SHADOW_ETCHED_IN);

	store = gtk_list_store_new(ACCOUNT_COL_COUNT,
				   G_TYPE_BOOLEAN, G_TYPE_STRING,
				   G_TYPE_STRING, G_TYPE_POINTER,
				   -1);

	widgets = me->widgets;
	tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));

	/* Setting up the TreeView view */

	/* Placeholder hack to avoid doubleclicking from connecting */
	renderer = gtk_cell_renderer_spinner_new();
	column = gtk_tree_view_column_new_with_attributes("", renderer, NULL);
	g_object_set(G_OBJECT(renderer), "active", FALSE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	renderer = gtk_cell_renderer_toggle_new();
	column = gtk_tree_view_column_new_with_attributes(_("Connected"), renderer, 
							  "active", ACCOUNT_CONN_COL,
							  NULL);
	gtk_cell_renderer_toggle_set_radio(GTK_CELL_RENDERER_TOGGLE(renderer), FALSE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
	g_signal_connect(G_OBJECT(renderer), "toggled", G_CALLBACK(_account_connect_toggle), store);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Service"), renderer, "text",
							  ACCOUNT_PROT_COL, NULL);
	g_object_set(G_OBJECT(renderer),
		     "wrap-mode", PANGO_WRAP_WORD_CHAR,
		     "wrap-width", 300,
		     NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Username"), renderer,
							  "text", ACCOUNT_USER_COL, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	gtk_container_add(GTK_CONTAINER(scrollwindow), tree);
	gtk_box_pack_start(GTK_BOX(top_container), scrollwindow, TRUE, TRUE, 2);

	l_list_foreach(accounts, _account_list_append, store);

	buttonbox = gtk_hbox_new(FALSE, 0);

	widgets->pref_button = gtk_button_new_from_stock(GTK_STOCK_PREFERENCES);

	gtk_widget_set_sensitive(widgets->pref_button, FALSE);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	g_signal_connect(selection, "changed", G_CALLBACK(account_selected), me);
	g_signal_connect(tree, "row-activated", G_CALLBACK(account_doubleclick), me);

	gtk_box_pack_end(GTK_BOX(buttonbox), widgets->pref_button, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(top_container), buttonbox, FALSE, TRUE, 2);
}

/* BEGIN Module Preferences */

enum {
	MODULE_LOADED_COL,
	MODULE_NAME_COL,
	MODULE_POINTER_COL,
	MODULE_COL_COUNT
};

static void _module_list_set (GtkListStore *store, GtkTreeIter *iter,
			      eb_PLUGIN_INFO *pref)
{
	char *text = NULL;
	char *version = g_strdup(pref->pi.version);

	sscanf(pref->pi.version, "$ Revision:%s $", version);

	text = g_strdup_printf("<b>%s <small>(v%s)</small></b>"
			       "\n<small>%s</small>",
			       pref->pi.module_name,
			       version,
			       pref->pi.description);

	g_free(version);

	gtk_list_store_set(store, iter,
			   MODULE_LOADED_COL, (pref->status == PLUGIN_LOADED),
			   MODULE_NAME_COL, text,
			   MODULE_POINTER_COL, pref,
			   -1);
}

static void _module_list_append (void *item, void *data)
{
	eb_PLUGIN_INFO *pref = item;
	GtkListStore *store = data;
	GtkTreeIter iter;

	gtk_list_store_append(store, &iter);
	_module_list_set(store, &iter, pref);
}

static gboolean _module_prefs_populate (GtkWidget *button, gpointer data)
{
	eb_PLUGIN_INFO *module = data;
	char *dialog_title = g_strdup_printf(_("Preferences: %s"), 
					     module->pi.module_name);

	if (module->pi.prefs)
		_input_list_to_prefs_dialog(dialog_title, module->pi.prefs, NULL);

	g_free(dialog_title);

	return FALSE;
}

static void module_selected (GtkTreeSelection *selection, gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	PrefSet *me = data;
	eb_PLUGIN_INFO *module = NULL;
	ModuleWidgets *widgets = me->widgets;
	int tag = -1;

	tag = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widgets->pref_button), "clicktag"));
	if(tag)
		g_signal_handler_disconnect(widgets->pref_button, tag);
	
	gtk_widget_set_sensitive(widgets->pref_button, FALSE);

	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter,
				    MODULE_POINTER_COL, &module,
				    -1);

		tag = g_signal_connect(widgets->pref_button, "clicked", 
				       G_CALLBACK(_module_prefs_populate), module);
		g_object_set_data(G_OBJECT(widgets->pref_button), "clicktag", GINT_TO_POINTER(tag));

		gtk_widget_set_sensitive(widgets->pref_button, (module->pi.prefs != NULL));
	}
}

static gboolean module_doubleclick (GtkTreeView *tree, GtkTreePath *path,
				    GtkTreeViewColumn *column, gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(tree);
	eb_PLUGIN_INFO *module = NULL;
	
	gtk_tree_model_get_iter(model, &iter, path);

	gtk_tree_model_get(model, &iter,
			   MODULE_POINTER_COL, &module,
			   -1);

	_module_prefs_populate(NULL, module);

	return FALSE;
}

static void _module_load_toggle(GtkCellRendererToggle *toggle, gchar *path, gpointer data)
{
	GtkTreeModel *model = data;
	GtkTreeIter iter;
	gboolean loaded = TRUE;
	eb_PLUGIN_INFO *mod = NULL;
	int err = 0;
	char *name;

	gtk_tree_model_get_iter_from_string(model, &iter, path);

	gtk_tree_model_get(model, &iter, 
			   MODULE_LOADED_COL, &loaded,
			   MODULE_POINTER_COL, &mod,
			   -1);

	name = g_strdup(mod->name);

	if (loaded)
		err = unload_module_full_path(name);
	else
		err = load_module_full_path(name);

	eb_debug(DBG_CORE, "Module %s returned with error status %d\n", loaded?"load":"unload", err);

	mod = FindPluginByName(name);
	_module_list_set(GTK_LIST_STORE(model), &iter, mod);

	g_free(name);
}

static GtkWidget *_init_module_list (PrefSet *me, const char *name, 
			      LList *list, GtkWidget *top_container)
{
	GtkListStore *store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *scrollwindow;
	GtkWidget *buttonbox;
	GtkTreeSelection *selection;
	GtkWidget *tree;
	ModuleWidgets *widgets = me->widgets;

	scrollwindow = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwindow),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollwindow),
					    GTK_SHADOW_ETCHED_IN);

	store = gtk_list_store_new(MODULE_COL_COUNT,
				   G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER,
				   -1);

	tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));

	/* Setting up the TreeView view */

	/* Placeholder hack to avoid doubleclick from loading/unloading module */
	renderer = gtk_cell_renderer_spinner_new();
	column = gtk_tree_view_column_new_with_attributes("", renderer, NULL);
	g_object_set(G_OBJECT(renderer), "active", FALSE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	renderer = gtk_cell_renderer_toggle_new();
	column = gtk_tree_view_column_new_with_attributes(_("Loaded"), renderer, "active",
							  MODULE_LOADED_COL, NULL);
	gtk_cell_renderer_toggle_set_radio(GTK_CELL_RENDERER_TOGGLE(renderer), FALSE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
	g_signal_connect(G_OBJECT(renderer), "toggled", G_CALLBACK(_module_load_toggle), store);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(name, renderer, "markup",
							  MODULE_NAME_COL, NULL);
	g_object_set(G_OBJECT(renderer),
		     "wrap-mode", PANGO_WRAP_WORD_CHAR,
		     "wrap-width", 300,
		     NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	gtk_container_add(GTK_CONTAINER(scrollwindow), tree);

	gtk_box_pack_start(GTK_BOX(top_container), scrollwindow, TRUE, TRUE, 2);

	l_list_foreach(list, _module_list_append, store);

	buttonbox = gtk_hbox_new(FALSE, 0);

	widgets->pref_button = gtk_button_new_from_stock(GTK_STOCK_PREFERENCES);

	gtk_widget_set_sensitive(widgets->pref_button, FALSE);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	g_signal_connect(selection, "changed", G_CALLBACK(module_selected), me);
	g_signal_connect(tree, "row-activated", G_CALLBACK(module_doubleclick), me);

	gtk_box_pack_end(GTK_BOX(buttonbox), widgets->pref_button, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(top_container), buttonbox, FALSE, TRUE, 2);

	return tree;
}

static void _init_modules (PrefSet *me, const char *name, LList *list)
{
	GtkWidget *top_container;

	top_container = _prep_top_container(me);
	_init_module_list(me, name, list, top_container);
}

static void _init_services (PrefSet *me)
{
	_init_modules(me, _("Service"), module_prefs->services);
}

static void _init_filters (PrefSet *me)
{
	_init_modules(me, _("Filter"), module_prefs->filters);
}

static void _init_utilities (PrefSet *me)
{
	_init_modules(me, _("Utility"), module_prefs->utilities);
}

static void _init_importers (PrefSet *me)
{
	_init_modules(me, _("Importer"), module_prefs->importers);
}

static void _init_smileys (PrefSet *me)
{
	GtkWidget *top_container;
	GtkWidget *button;
	GtkWidget *tree;
	GtkWidget *label;
	char label_text[24];

	sprintf(label_text, "<b>%s</b>", _("Smiley Plugins"));

	top_container = _prep_top_container(me);

	button = ay_pref_check_button(_("Enable Smileys"),
				      "do_smiley", top_container);

	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), label_text);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_misc_set_padding(GTK_MISC(label), 0, 20);
	gtk_box_pack_start(GTK_BOX(top_container), label, FALSE, FALSE, 0);

	tree = _init_module_list(me, _("Plugin"), module_prefs->smileys, top_container);

	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(s_toggle_checkbox), tree);
}
