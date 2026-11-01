/*

  Copyright (c) 2005-2026 uim Project https://github.com/uim/uim

  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of authors nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

  GTK 4 notes for this file:
  - GtkTreeView/GtkTreeStore/GtkListStore/GtkCellRendererText/GtkComboBox
    and GtkDialog are deprecated since GTK 4.10.  The ordered-list and
    key editors use GtkListView, the table editor uses GtkColumnView with
    GtkEditableLabel cells, all backed by a GListStore of UimPrefRow
    objects; choices use GtkDropDown over a GtkStringList; and the
    editor dialogs are plain modal GtkWindows with a "Close" button.
  - GtkContainer's generic add/remove is gone; every "add" call became
    the type-specific setter (gtk_scrolled_window_set_child(),
    gtk_grid_attach(), etc.), and gtk_widget_show()/show_all() were
    dropped since GTK 4 widgets are visible by default.
  - GTK_STOCK_* and GtkArrow are both gone; stock buttons became
    mnemonic-labelled or icon-name buttons, and arrow buttons now just
    use gtk_button_new_from_icon_name() directly.
  - GtkButtonBox is gone; replaced with a plain GtkBox.
  - gtk_dialog_run()'s blocking file-picker use is replaced with the
    modern async GtkFileDialog (>= GTK 4.10, matching this port's
    gtk4 >= 4.12 baseline).
  - GdkEventKey is gone; the "press any key to grab" flow now uses
    GtkEventControllerKey and reads the current GdkEvent via
    gtk_event_controller_get_current_event(), matching the pattern
    already used by gtk4/immodule/uim-im-context.c.
*/

#include <config.h>

#include "gtk-custom-widgets-gtk4.h"
#include <gdk/gdkkeysyms.h>

#include <string.h>
#include <stdlib.h>
#include <locale.h>

#include "uim/uim.h"
#include "uim/uim-custom.h"
#include "uim/gettext.h"

#include "key-util-gtk4.h"
#include "gtk-keytab.h"

#define DEFAULT_OLIST_WINDOW_WIDTH    480
#define DEFAULT_OLIST_WINDOW_HEIGHT   350
#define DEFAULT_KEYCONF_WINDOW_WIDTH  280
#define DEFAULT_KEYCONF_WINDOW_HEIGHT 220
#define DEFAULT_TABLE_WINDOW_WIDTH    340
#define DEFAULT_TABLE_WINDOW_HEIGHT   280
#define OBJECT_DATA_UIM_CUSTOM_SYM    "uim-pref-gtk::uim-custom-sym"

extern gboolean uim_pref_gtk_value_changed;

extern void
uim_pref_gtk_mark_value_changed(void);

static GtkSizeGroup *spin_button_sgroup = NULL;

static struct OListPrefWin
{
  GtkWidget *window;
  GtkWidget *list_view[2];
  GListStore *store[2];
  GtkSelectionModel *selection[2];
  GtkWidget *up_button;
  GtkWidget *down_button;
  GtkWidget *left_button;
  GtkWidget *right_button;
} olist_pref_win = {
  NULL,
  {NULL, NULL},
  {NULL, NULL},
  {NULL, NULL},
};
static struct KeyPrefWin
{
  GtkWidget *window;
  GtkWidget *list_view;
  GListStore *store;
  GtkSingleSelection *selection;
  GtkWidget *add_button;
  GtkWidget *remove_button;
  GtkWidget *keycode_entry;

  gint grabbed_key_val;
  gint grabbed_key_state;
} key_pref_win = {
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  0,
};

#define OBJECT_DATA_TABLE_STORE     "uim-pref-gtk::table-store"
#define OBJECT_DATA_TABLE_SELECTION "uim-pref-gtk::table-selection"
#define OBJECT_DATA_TABLE_LABEL     "uim-pref-gtk::table-label"
#define OBJECT_DATA_TABLE_N_COLUMNS "uim-pref-gtk::table-n-columns"
#define OBJECT_DATA_ROW             "uim-pref-gtk::row"

static void
uimpref_file_entry_class_init(UimPrefFileEntryClass *klass);
static void
uimpref_file_entry_init(UimPrefFileEntry *entry);

GType
uimpref_file_entry_get_type(void)
{
  static GType uimpref_file_entry_type = 0;

  if (!uimpref_file_entry_type) {
    static const GTypeInfo uimpref_file_entry_info = {
      sizeof(UimPrefFileEntryClass),
      NULL,
      NULL,
      (GClassInitFunc)uimpref_file_entry_class_init,
      NULL,
      NULL,
      sizeof(UimPrefFileEntry),
      0,
      (GInstanceInitFunc)uimpref_file_entry_init,
    };

    uimpref_file_entry_type = g_type_register_static(GTK_TYPE_ENTRY,
                                                     "UimPrefFileEntry",
                                                     &uimpref_file_entry_info,
                                                     0);
  }

  return uimpref_file_entry_type;
}

static void
uimpref_file_entry_class_init(UimPrefFileEntryClass *klass)
{
}

static void
uimpref_file_entry_init(UimPrefFileEntry *entry)
{
  entry->type = UCustomPathnameType_RegularFile;
}

GtkWidget *
uimpref_file_entry_new()
{
  return GTK_WIDGET(g_object_new(uimpref_file_entry_get_type(), NULL));
}

struct _UimPrefRow
{
  GObject parent_instance;
  guint n_columns;
  gchar **columns; /* n_columns entries, each of which may be NULL */
};

G_DEFINE_TYPE(UimPrefRow, uim_pref_row, G_TYPE_OBJECT)

static void
uim_pref_row_finalize(GObject *object)
{
  UimPrefRow *row = UIM_PREF_ROW(object);
  guint i;

  for (i = 0; i < row->n_columns; i++)
    g_free(row->columns[i]);
  g_free(row->columns);

  G_OBJECT_CLASS(uim_pref_row_parent_class)->finalize(object);
}

static void
uim_pref_row_class_init(UimPrefRowClass *klass)
{
  G_OBJECT_CLASS(klass)->finalize = uim_pref_row_finalize;
}

static void
uim_pref_row_init(UimPrefRow *row)
{
}

UimPrefRow *
uim_pref_row_new(guint n_columns)
{
  UimPrefRow *row = g_object_new(UIM_PREF_TYPE_ROW, NULL);

  row->n_columns = n_columns;
  row->columns = g_new0(gchar *, n_columns + 1);

  return row;
}

guint
uim_pref_row_get_n_columns(UimPrefRow *row)
{
  g_return_val_if_fail(UIM_PREF_IS_ROW(row), 0);

  return row->n_columns;
}

const char *
uim_pref_row_get(UimPrefRow *row, guint column)
{
  g_return_val_if_fail(UIM_PREF_IS_ROW(row), NULL);

  if (column >= row->n_columns)
    return NULL;

  return row->columns[column];
}

gboolean
uim_pref_row_set(UimPrefRow *row, guint column, const char *str)
{
  g_return_val_if_fail(UIM_PREF_IS_ROW(row), FALSE);

  if (column >= row->n_columns)
    return FALSE;

  g_free(row->columns[column]);
  row->columns[column] = g_strdup(str);

  return TRUE;
}

static void
label_factory_setup_cb(GtkSignalListItemFactory *factory,
                       GObject *object,
                       gpointer data)
{
  GtkWidget *label = gtk_label_new(NULL);

  gtk_label_set_xalign(GTK_LABEL(label), 0.0);
  gtk_list_item_set_child(GTK_LIST_ITEM(object), label);
}

static void
label_factory_bind_cb(GtkSignalListItemFactory *factory,
                      GObject *object,
                      gpointer data)
{
  GtkListItem *item = GTK_LIST_ITEM(object);
  UimPrefRow *row = gtk_list_item_get_item(item);
  const char *str = uim_pref_row_get(row, GPOINTER_TO_UINT(data));

  gtk_label_set_text(GTK_LABEL(gtk_list_item_get_child(item)), str ? str : "");
}

/* A factory showing one column of a UimPrefRow as a read-only label. */
GtkListItemFactory *
uim_pref_label_factory_new(guint column)
{
  GtkListItemFactory *factory = gtk_signal_list_item_factory_new();

  g_signal_connect(factory,
                   "setup",
                   G_CALLBACK(label_factory_setup_cb),
                   GUINT_TO_POINTER(column));
  g_signal_connect(factory,
                   "bind",
                   G_CALLBACK(label_factory_bind_cb),
                   GUINT_TO_POINTER(column));

  return factory;
}

static UimPrefRow *
olist_row_new(const char *label, const char *symbol)
{
  UimPrefRow *row = uim_pref_row_new(2);

  uim_pref_row_set(row, 0, label);
  uim_pref_row_set(row, 1, symbol);

  return row;
}

/*
 * Replacement for gtk_dialog_new_with_buttons(title, parent,
 * GTK_DIALOG_MODAL, _("_Close"), GTK_RESPONSE_CLOSE, NULL): a modal
 * window whose "Close" button and Escape key both close it, so every
 * way of dismissing it goes through "close-request".  The caller packs
 * its widgets into *content_area.
 */
static GtkWidget *
create_pref_dialog(GtkWidget *parent_widget,
                   const char *title,
                   gint width,
                   gint height,
                   GtkWidget **content_area)
{
  GtkWidget *dialog, *vbox, *button_box, *button;
  GtkEventController *shortcuts;
  GtkRoot *root;

  dialog = gtk_window_new();
  gtk_window_set_title(GTK_WINDOW(dialog), title);
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  root = gtk_widget_get_root(parent_widget);
  if (GTK_IS_WINDOW(root))
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(root));
  gtk_window_set_default_size(GTK_WINDOW(dialog), width, height);

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child(GTK_WINDOW(dialog), vbox);

  *content_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_vexpand(*content_area, TRUE);
  gtk_box_append(GTK_BOX(vbox), *content_area);

  button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_halign(button_box, GTK_ALIGN_END);
  gtk_widget_set_margin_start(button_box, 6);
  gtk_widget_set_margin_end(button_box, 6);
  gtk_widget_set_margin_top(button_box, 6);
  gtk_widget_set_margin_bottom(button_box, 6);
  gtk_box_append(GTK_BOX(vbox), button_box);

  button = gtk_button_new_with_mnemonic(_("_Close"));
  g_signal_connect_swapped(G_OBJECT(button),
                           "clicked",
                           G_CALLBACK(gtk_window_close),
                           dialog);
  gtk_box_append(GTK_BOX(button_box), button);

  shortcuts = gtk_shortcut_controller_new();
  gtk_shortcut_controller_add_shortcut(
    GTK_SHORTCUT_CONTROLLER(shortcuts),
    gtk_shortcut_new(gtk_keyval_trigger_new(GDK_KEY_Escape, 0),
                     gtk_named_action_new("window.close")));
  gtk_widget_add_controller(dialog, shortcuts);

  return dialog;
}

static GtkWidget *
create_list_view(GtkSelectionModel *selection, GtkWidget *scrolled_window)
{
  GtkWidget *view;

  view = gtk_list_view_new(selection, uim_pref_label_factory_new(0));
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), view);

  return view;
}

/*
 * Returns the number of selected rows of selection, and stores the
 * position of the first selected row in *first when first is non-NULL.
 */
static guint
get_selected_rows(GtkSelectionModel *selection, guint *first)
{
  GtkBitset *set = gtk_selection_model_get_selection(selection);
  guint n = (guint)gtk_bitset_get_size(set);

  if (first)
    *first = gtk_bitset_get_minimum(set);
  gtk_bitset_unref(set);

  return n;
}

/* Swaps the rows at position and position + 1 of store. */
static void
list_store_swap_with_next(GListStore *store, guint position)
{
  gpointer items[2];

  items[0] = g_list_model_get_item(G_LIST_MODEL(store), position + 1);
  items[1] = g_list_model_get_item(G_LIST_MODEL(store), position);
  g_list_store_splice(store, position, 2, items, 2);
  g_object_unref(items[0]);
  g_object_unref(items[1]);
}

static void
custom_check_button_toggled_cb(GtkCheckButton *button, gpointer user_data)
{
  const char *custom_sym;
  struct uim_custom *custom;
  uim_bool rv;
  gboolean active;

  custom_sym = g_object_get_data(G_OBJECT(button), OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_if_fail(custom_sym);

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom && custom->type == UCustom_Bool);

  active = gtk_check_button_get_active(GTK_CHECK_BUTTON(button));
  custom->value->as_bool = active;
  rv = uim_custom_set(custom);

  if (rv) {
    uim_pref_gtk_mark_value_changed();
  } else {
    g_printerr("Failed to set bool value for \"%s\".\n", custom->symbol);
    /* FIXME! reset the widget */
  }

  uim_custom_free(custom);
}

static void
sync_value_bool(GtkCheckButton *button)
{
  const char *custom_sym;
  struct uim_custom *custom;

  g_signal_handlers_block_by_func(
    G_OBJECT(button),
    (gpointer)(uintptr_t)custom_check_button_toggled_cb,
    NULL);

  custom_sym = g_object_get_data(G_OBJECT(button), OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_if_fail(custom_sym);

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom && custom->type == UCustom_Bool);

  gtk_widget_set_sensitive(GTK_WIDGET(button), custom->is_active);
  /* GTK4's GtkCheckButton no longer subclasses GtkToggleButton (it is
   * a standalone GtkWidget now), so the toggle-button accessors don't
   * apply to it; use the GtkCheckButton ones instead. */
  gtk_check_button_set_active(GTK_CHECK_BUTTON(button), custom->value->as_bool);

  uim_custom_free(custom);

  g_signal_handlers_unblock_by_func(
    G_OBJECT(button),
    (gpointer)(uintptr_t)custom_check_button_toggled_cb,
    NULL);
}

static void
update_custom_type_bool_cb(void *ptr, const char *custom_sym)
{
  sync_value_bool(GTK_CHECK_BUTTON(ptr));
}

static void
add_custom_type_bool(GtkWidget *vbox, struct uim_custom *custom)
{
  GtkWidget *hbox;
  GtkWidget *check_button;
  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

  check_button = gtk_check_button_new_with_label(custom->label);
  g_object_set_data_full(G_OBJECT(check_button),
                         OBJECT_DATA_UIM_CUSTOM_SYM,
                         g_strdup(custom->symbol),
                         (GDestroyNotify)g_free);

  sync_value_bool(GTK_CHECK_BUTTON(check_button));

  g_signal_connect(G_OBJECT(check_button),
                   "toggled",
                   G_CALLBACK(custom_check_button_toggled_cb),
                   NULL);
  uim_custom_cb_add(custom->symbol, check_button, update_custom_type_bool_cb);

  gtk_box_append(GTK_BOX(hbox), check_button);
  gtk_box_append(GTK_BOX(vbox), hbox);
}

static void
custom_spin_button_value_changed(GtkSpinButton *spin, gpointer user_data)
{
  const char *custom_sym;
  struct uim_custom *custom;
  uim_bool rv;

  custom_sym = g_object_get_data(G_OBJECT(spin), OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_if_fail(custom_sym);

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom && custom->type == UCustom_Int);

  custom->value->as_int = gtk_spin_button_get_value(spin);
  rv = uim_custom_set(custom);

  if (rv) {
    uim_pref_gtk_mark_value_changed();
  } else {
    g_printerr("Failed to set int value for \"%s\".\n", custom->symbol);
    /* FIXME! reset the widget */
  }

  uim_custom_free(custom);
}

static void
sync_value_int(GtkSpinButton *spin)
{
  const char *custom_sym;
  struct uim_custom *custom;
  GtkAdjustment *adj;

  g_signal_handlers_block_by_func(
    G_OBJECT(spin),
    (gpointer)(uintptr_t)custom_spin_button_value_changed,
    NULL);

  adj = gtk_spin_button_get_adjustment(spin);

  custom_sym = g_object_get_data(G_OBJECT(spin), OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_if_fail(custom_sym);

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom && custom->type == UCustom_Int);

  gtk_widget_set_sensitive(gtk_widget_get_parent(GTK_WIDGET(spin)),
                           custom->is_active);
  if (custom->range->as_int.min != (int)gtk_adjustment_get_lower(adj) ||
      custom->range->as_int.max != (int)gtk_adjustment_get_upper(adj))
    gtk_spin_button_set_range(spin,
                              custom->range->as_int.min,
                              custom->range->as_int.max);
  if (custom->value->as_int != (int)gtk_spin_button_get_value(spin)) {
    gtk_spin_button_set_value(spin, custom->value->as_int);
  }

  uim_custom_free(custom);

  g_signal_handlers_unblock_by_func(
    G_OBJECT(spin),
    (gpointer)(uintptr_t)custom_spin_button_value_changed,
    NULL);
}

static void
update_custom_type_int_cb(void *ptr, const char *custom_sym)
{
  sync_value_int(GTK_SPIN_BUTTON(ptr));
}

static void
add_custom_type_integer(GtkWidget *vbox, struct uim_custom *custom)
{
  GtkWidget *hbox;
  GtkWidget *label;
  GtkAdjustment *adjustment;
  GtkWidget *spin;

  if (!spin_button_sgroup)
    spin_button_sgroup = gtk_size_group_new(GTK_SIZE_GROUP_VERTICAL);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

  label = gtk_label_new(custom->label);
  gtk_box_append(GTK_BOX(hbox), label);

  adjustment = (GtkAdjustment *)gtk_adjustment_new(
    custom->value->as_int,     /* initial size */
    custom->range->as_int.min, /* minimum */
    custom->range->as_int.max, /* maximum */
    1.0,
    10.0,
    0);
  spin = gtk_spin_button_new(adjustment, 1.0, 0);
  gtk_size_group_add_widget(spin_button_sgroup, spin);
  gtk_widget_set_hexpand(label, TRUE);
  gtk_box_append(GTK_BOX(hbox), spin);

  gtk_box_append(GTK_BOX(vbox), hbox);

  g_object_set_data_full(G_OBJECT(adjustment),
                         OBJECT_DATA_UIM_CUSTOM_SYM,
                         g_strdup(custom->symbol),
                         (GDestroyNotify)g_free);
  g_object_set_data_full(G_OBJECT(spin),
                         OBJECT_DATA_UIM_CUSTOM_SYM,
                         g_strdup(custom->symbol),
                         (GDestroyNotify)g_free);

  sync_value_int(GTK_SPIN_BUTTON(spin));

  g_signal_connect(G_OBJECT(spin),
                   "value-changed",
                   G_CALLBACK(custom_spin_button_value_changed),
                   NULL);
  uim_custom_cb_add(custom->symbol, spin, update_custom_type_int_cb);
}

static void
custom_entry_changed_cb(GtkEntry *entry, gpointer user_data)
{
  const char *custom_sym;
  struct uim_custom *custom;
  uim_bool rv;
  const char *str;

  custom_sym = g_object_get_data(G_OBJECT(entry), OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_if_fail(custom_sym);

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom && (custom->type == UCustom_Str ||
                              custom->type == UCustom_Pathname));

  str = gtk_editable_get_text(GTK_EDITABLE(entry));

  if (custom->type == UCustom_Str) {
    free(custom->value->as_str);
    custom->value->as_str = strdup(str);
    rv = uim_custom_set(custom);
  } else if (custom->type == UCustom_Pathname) {
    free(custom->value->as_pathname->str);
    custom->value->as_pathname->str = strdup(str);
    rv = uim_custom_set(custom);
  } else {
    rv = UIM_FALSE;
  }

  if (rv) {
    uim_pref_gtk_mark_value_changed();
  } else {
    g_printerr("Failed to set str value for \"%s\".\n", custom->symbol);
    /* FIXME! reset the widget */
  }

  uim_custom_free(custom);
}

static void
sync_value_string(GtkEntry *entry)
{
  const char *custom_sym;
  struct uim_custom *custom;

  g_signal_handlers_block_by_func(G_OBJECT(entry),
                                  (gpointer)(uintptr_t)custom_entry_changed_cb,
                                  NULL);

  custom_sym = g_object_get_data(G_OBJECT(entry), OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_if_fail(custom_sym);

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom && (custom->type == UCustom_Str ||
                              custom->type == UCustom_Pathname));

  gtk_widget_set_sensitive(gtk_widget_get_parent(GTK_WIDGET(entry)),
                           custom->is_active);
  if (custom->type == UCustom_Str) {
    /* Avoid a redundant set_text() when the value hasn't actually
     * changed: unlike GTK3's gtk_entry_set_text(), GTK4's
     * gtk_editable_set_text() always resets the cursor to the start,
     * even when re-setting identical text. Since this function is
     * invoked as this very entry's own change-notification callback
     * (see custom_entry_changed_cb -> uim_custom_set ->
     * update_custom_type_string_cb), setting the text unconditionally
     * would move the cursor to column 0 after every keystroke. */
    if (g_strcmp0(gtk_editable_get_text(GTK_EDITABLE(entry)),
                  custom->value->as_str) != 0)
      gtk_editable_set_text(GTK_EDITABLE(entry), custom->value->as_str);
  } else if (custom->type == UCustom_Pathname) {
    if (g_strcmp0(gtk_editable_get_text(GTK_EDITABLE(entry)),
                  custom->value->as_pathname->str) != 0)
      gtk_editable_set_text(GTK_EDITABLE(entry),
                            custom->value->as_pathname->str);
  }

  uim_custom_free(custom);

  g_signal_handlers_unblock_by_func(
    G_OBJECT(entry),
    (gpointer)(uintptr_t)custom_entry_changed_cb,
    NULL);
}

static void
update_custom_type_string_cb(void *ptr, const char *custom_sym)
{
  sync_value_string(GTK_ENTRY(ptr));
}

static void
add_custom_type_string(GtkWidget *vbox, struct uim_custom *custom)
{
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *entry;

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

  label = gtk_label_new(custom->label);
  gtk_box_append(GTK_BOX(hbox), label);

  entry = gtk_entry_new();
  gtk_widget_set_hexpand(entry, TRUE);
  gtk_box_append(GTK_BOX(hbox), entry);
  gtk_box_append(GTK_BOX(vbox), hbox);

  g_object_set_data_full(G_OBJECT(entry),
                         OBJECT_DATA_UIM_CUSTOM_SYM,
                         g_strdup(custom->symbol),
                         (GDestroyNotify)g_free);

  sync_value_string(GTK_ENTRY(entry));

  g_signal_connect(G_OBJECT(entry),
                   "changed",
                   G_CALLBACK(custom_entry_changed_cb),
                   NULL);
  uim_custom_cb_add(custom->symbol, entry, update_custom_type_string_cb);
}

static void
pathname_file_dialog_cb(GObject *source, GAsyncResult *result, gpointer data)
{
  GtkEntry *entry = GTK_ENTRY(data);
  GFile *file;
  GError *error = NULL;

  if (UIMPREF_FILE_ENTRY(entry)->type == UCustomPathnameType_Directory)
    file = gtk_file_dialog_select_folder_finish(GTK_FILE_DIALOG(source),
                                                result,
                                                &error);
  else
    file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);

  if (file) {
    char *filename = g_file_get_path(file);

    if (filename) {
      gtk_editable_set_text(GTK_EDITABLE(entry), filename);
      g_free(filename);
    }
    g_object_unref(file);
  } else if (error) {
    g_error_free(error);
  }
}

static void
custom_pathname_button_clicked_cb(GtkWidget *button, GtkWidget *entry)
{
  GtkFileDialog *dialog = gtk_file_dialog_new();
  GtkRoot *root = gtk_widget_get_root(button);

  gtk_file_dialog_set_title(dialog, _("Specify file"));

  if (UIMPREF_FILE_ENTRY(entry)->type == UCustomPathnameType_Directory)
    gtk_file_dialog_select_folder(dialog,
                                  GTK_IS_WINDOW(root) ? GTK_WINDOW(root) : NULL,
                                  NULL,
                                  pathname_file_dialog_cb,
                                  entry);
  else
    gtk_file_dialog_open(dialog,
                         GTK_IS_WINDOW(root) ? GTK_WINDOW(root) : NULL,
                         NULL,
                         pathname_file_dialog_cb,
                         entry);

  g_object_unref(dialog);
}

static void
add_custom_type_pathname(GtkWidget *vbox, struct uim_custom *custom)
{
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *button;
  const char *button_label;

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

  label = gtk_label_new(custom->label);
  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
  gtk_label_set_xalign(GTK_LABEL(label), 0);
  gtk_label_set_yalign(GTK_LABEL(label), 0);
  gtk_box_append(GTK_BOX(vbox), label);

  entry = uimpref_file_entry_new();
  UIMPREF_FILE_ENTRY(entry)->type = custom->value->as_pathname->type;
  gtk_widget_set_hexpand(entry, TRUE);
  gtk_box_append(GTK_BOX(hbox), entry);

  /* Since both pathname type opens the file dialog to select an item
   * rather than open it, the label should always be "Select..." here.
   * The type is obvious for uses even if the button label does not
   * indicate it. Information about the action the button causes is
   * more important. Even if a better label has been found, it should
   * not contain the term 'directory' since GNOME uses 'folder' for
   * it.  -- YamaKen 2006-01-21 */
  switch (custom->value->as_pathname->type) {
  case UCustomPathnameType_Directory:
    button_label = N_("Select...");
    break;
  case UCustomPathnameType_RegularFile:
  default:
    button_label = N_("Select...");
    break;
  }
  button = gtk_button_new_with_label(dgettext(GETTEXT_PACKAGE, button_label));
  gtk_box_append(GTK_BOX(hbox), button);

  gtk_box_append(GTK_BOX(vbox), hbox);

  g_object_set_data_full(G_OBJECT(entry),
                         OBJECT_DATA_UIM_CUSTOM_SYM,
                         g_strdup(custom->symbol),
                         (GDestroyNotify)g_free);

  sync_value_string(GTK_ENTRY(entry));

  g_signal_connect(G_OBJECT(entry),
                   "changed",
                   G_CALLBACK(custom_entry_changed_cb),
                   NULL);

  g_signal_connect(G_OBJECT(button),
                   "clicked",
                   G_CALLBACK(custom_pathname_button_clicked_cb),
                   entry);
  uim_custom_cb_add(custom->symbol, entry, update_custom_type_string_cb);
}

static void
custom_drop_down_selected_changed(GObject *drop_down,
                                  GParamSpec *pspec,
                                  gpointer user_data)
{
  const char *custom_sym;
  struct uim_custom *custom;
  struct uim_custom_choice *choice = NULL;
  guint i, num;
  uim_bool rv;

  custom_sym = g_object_get_data(drop_down, OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_if_fail(custom_sym);

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom_sym && custom->type == UCustom_Choice);

  num = gtk_drop_down_get_selected(GTK_DROP_DOWN(drop_down));

  /* check length of custom->range->as_choice.valid_items */
  for (i = 0; custom->range->as_choice.valid_items &&
              custom->range->as_choice.valid_items[i];
       i++) {
    if (i == num) {
      choice = custom->range->as_choice.valid_items[i];
    }
  }
  g_return_if_fail(choice);

  free(custom->value->as_choice->symbol);
  free(custom->value->as_choice->label);
  free(custom->value->as_choice->desc);

  custom->value->as_choice->symbol =
    choice->symbol ? strdup(choice->symbol) : NULL;
  custom->value->as_choice->label =
    choice->label ? strdup(choice->label) : NULL;
  custom->value->as_choice->desc = choice->desc ? strdup(choice->desc) : NULL;

  rv = uim_custom_set(custom);

  if (rv) {
    uim_pref_gtk_mark_value_changed();
  } else {
    g_printerr("Failed to set str value for \"%s\".\n", custom->symbol);
    /* FIXME! reset the widget */
  }

  uim_custom_free(custom);
}

static void
sync_value_choice(GtkDropDown *drop_down)
{
  const char *custom_sym;
  struct uim_custom *custom;
  struct uim_custom_choice **item;
  GtkStringList *list;
  GPtrArray *labels;
  guint i = 0, default_index = 0;
  gchar *default_symbol;

  custom_sym =
    g_object_get_data(G_OBJECT(drop_down), OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_if_fail(custom_sym);

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom && custom->type == UCustom_Choice);

  item = custom->range->as_choice.valid_items;
  if (item == NULL || *item == NULL) {
    uim_custom_free(custom);
    return;
  }

  g_signal_handlers_block_by_func(
    G_OBJECT(drop_down),
    (gpointer)(uintptr_t)custom_drop_down_selected_changed,
    NULL);

  default_symbol = custom->value->as_choice->symbol;

  labels = g_ptr_array_new();
  while (*item) {
    g_ptr_array_add(labels, (*item)->label);
    if (!strcmp(default_symbol, (*item)->symbol))
      default_index = i;
    i++;
    item++;
  }
  g_ptr_array_add(labels, NULL);

  list = GTK_STRING_LIST(gtk_drop_down_get_model(drop_down));
  gtk_string_list_splice(list,
                         0,
                         g_list_model_get_n_items(G_LIST_MODEL(list)),
                         (const char *const *)labels->pdata);
  g_ptr_array_free(labels, TRUE);

  gtk_drop_down_set_selected(drop_down, default_index);

  gtk_widget_set_sensitive(gtk_widget_get_parent(GTK_WIDGET(drop_down)),
                           custom->is_active);

  g_signal_handlers_unblock_by_func(
    G_OBJECT(drop_down),
    (gpointer)(uintptr_t)custom_drop_down_selected_changed,
    NULL);

  uim_custom_free(custom);
}

static void
update_custom_type_choice_cb(void *ptr, const char *custom_sym)
{
  sync_value_choice(GTK_DROP_DOWN(ptr));
}

static void
add_custom_type_choice(GtkWidget *vbox, struct uim_custom *custom)
{
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *drop_down;

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_append(GTK_BOX(vbox), hbox);

  label = gtk_label_new(custom->label);
  gtk_box_append(GTK_BOX(hbox), label);

  drop_down = gtk_drop_down_new(G_LIST_MODEL(gtk_string_list_new(NULL)), NULL);
  gtk_box_append(GTK_BOX(hbox), drop_down);

  g_object_set_data_full(G_OBJECT(drop_down),
                         OBJECT_DATA_UIM_CUSTOM_SYM,
                         g_strdup(custom->symbol),
                         (GDestroyNotify)g_free);

  sync_value_choice(GTK_DROP_DOWN(drop_down));

  g_signal_connect(G_OBJECT(drop_down),
                   "notify::selected",
                   G_CALLBACK(custom_drop_down_selected_changed),
                   NULL);
  uim_custom_cb_add(custom->symbol, drop_down, update_custom_type_choice_cb);
}

static void
sync_value_olist(GtkEntry *entry)
{
  const char *custom_sym;
  struct uim_custom *custom;
  struct uim_custom_choice *item;
  GString *str;
  gint i;

  custom_sym = g_object_get_data(G_OBJECT(entry), OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_if_fail(custom_sym);

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom && custom->type == UCustom_OrderedList);

  str = g_string_new("");

  if (custom->value->as_olist) {
    for (item = custom->value->as_olist[0], i = 0; item;
         item = custom->value->as_olist[++i]) {
      if (i != 0)
        g_string_append(str, ", ");
      g_string_append_printf(str, "\"%s\"", item->label);
    }
  } else {
    /* error message */
  }

  gtk_editable_set_text(GTK_EDITABLE(entry), str->str);
  gtk_widget_set_sensitive(gtk_widget_get_parent(GTK_WIDGET(entry)),
                           custom->is_active);

  g_string_free(str, TRUE);
  uim_custom_free(custom);
}

static void
olist_pref_list_view_set_value(GtkEntry *olist_entry,
                               gboolean left_view,
                               gboolean right_view)
{
  const char *custom_sym;
  struct uim_custom *custom;
  struct uim_custom_choice *item;
  GListStore *store;
  UimPrefRow *row;
  gint i;

  custom_sym =
    g_object_get_data(G_OBJECT(olist_entry), OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_if_fail(custom_sym);

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom && custom->type == UCustom_OrderedList);

  /* left list view */
  if (left_view) {
    store = olist_pref_win.store[0];
    g_list_store_remove_all(store);

    for (item = custom->value->as_olist[0], i = 0; item;
         item = custom->value->as_olist[++i]) {
      row = olist_row_new(item->label, item->symbol);
      g_list_store_append(store, row);
      g_object_unref(row);
    }
  }

  /* right list view */
  if (right_view) {
    store = olist_pref_win.store[1];
    g_list_store_remove_all(store);

    for (item = custom->range->as_olist.valid_items[0], i = 0; item;
         item = custom->range->as_olist.valid_items[++i]) {
      struct uim_custom_choice *item2;
      gint j;
      gboolean enabled = FALSE;

      for (item2 = custom->value->as_olist[0], j = 0; item2;
           item2 = custom->value->as_olist[++j]) {
        if (!strcmp(item->symbol, item2->symbol)) {
          enabled = TRUE;
          break;
        }
      }

      if (!enabled) {
        row = olist_row_new(item->label, item->symbol);
        g_list_store_append(store, row);
        g_object_unref(row);
      }
    }
  }

  uim_custom_free(custom);
}

static void
set_olist_buttons_sensitive(GtkEntry *olist_entry)
{
  const char *custom_sym;
  struct uim_custom *custom;
  guint num, idx;
  gboolean is_selected1 = FALSE, is_selected2 = FALSE;
  gboolean is_multiple = FALSE, is_first = FALSE, is_end = FALSE;

  custom_sym =
    g_object_get_data(G_OBJECT(olist_entry), OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_if_fail(custom_sym);

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom && custom->type == UCustom_OrderedList);

  num = get_selected_rows(olist_pref_win.selection[0], &idx);
  if (num > 0) {
    is_selected1 = TRUE;

    if (num > 1) {
      is_multiple = TRUE;
    } else {
      if (idx == 0)
        is_first = TRUE;

      if (idx + 1 >=
          g_list_model_get_n_items(G_LIST_MODEL(olist_pref_win.store[0])))
        is_end = TRUE;
    }
  }

  num = get_selected_rows(olist_pref_win.selection[1], NULL);
  if (num > 0)
    is_selected2 = TRUE;

  gtk_widget_set_sensitive(olist_pref_win.up_button,
                           is_selected1 && !is_first && !is_multiple);
  gtk_widget_set_sensitive(olist_pref_win.down_button,
                           is_selected1 && !is_end && !is_multiple);
  gtk_widget_set_sensitive(olist_pref_win.right_button, is_selected1);
  gtk_widget_set_sensitive(olist_pref_win.left_button, is_selected2);

  uim_custom_free(custom);
}

/*
 * Moves the single selected row of the "Enabled" list one step up
 * (up == TRUE) or down (up == FALSE), keeping it selected.
 */
static void
olist_pref_move_selected(GtkEntry *olist_entry, gboolean up)
{
  const char *custom_sym;
  struct uim_custom *custom;
  struct uim_custom_choice *choice;
  guint num, idx, n_items, upper;
  uim_bool urv;

  custom_sym =
    g_object_get_data(G_OBJECT(olist_entry), OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_if_fail(custom_sym);

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom && custom->type == UCustom_OrderedList);

  num = get_selected_rows(olist_pref_win.selection[0], &idx);
  n_items = g_list_model_get_n_items(G_LIST_MODEL(olist_pref_win.store[0]));
  if (num != 1 || (up && idx == 0) || (!up && idx + 1 >= n_items)) {
    uim_custom_free(custom);
    return;
  }

  /* the upper one of the two rows to be swapped */
  upper = up ? idx - 1 : idx;

  /* real move */
  choice = custom->value->as_olist[upper];
  custom->value->as_olist[upper] = custom->value->as_olist[upper + 1];
  custom->value->as_olist[upper + 1] = choice;
  urv = uim_custom_set(custom);

  if (urv != UIM_FALSE) {
    uim_pref_gtk_mark_value_changed();

    /* sync the view */
    list_store_swap_with_next(olist_pref_win.store[0], upper);
    gtk_list_view_scroll_to(GTK_LIST_VIEW(olist_pref_win.list_view[0]),
                            up ? idx - 1 : idx + 1,
                            GTK_LIST_SCROLL_SELECT,
                            NULL);
    set_olist_buttons_sensitive(olist_entry);
    sync_value_olist(GTK_ENTRY(olist_entry));
  }

  uim_custom_free(custom);
}

static void
olist_pref_up_button_clicked_cb(GtkWidget *widget, GtkEntry *olist_entry)
{
  olist_pref_move_selected(olist_entry, TRUE);
}

static void
olist_pref_down_button_clicked_cb(GtkWidget *widget, GtkEntry *olist_entry)
{
  olist_pref_move_selected(olist_entry, FALSE);
}

static void
olist_pref_left_button_clicked_cb(GtkWidget *widget, GtkEntry *olist_entry)
{
  const char *custom_sym;
  struct uim_custom *custom;
  GtkBitset *set;
  GtkBitsetIter bitset_iter;
  gboolean valid;
  guint pos;
  gint num;
  uim_bool urv;

  custom_sym =
    g_object_get_data(G_OBJECT(olist_entry), OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_if_fail(custom_sym);

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom && custom->type == UCustom_OrderedList);

  set = gtk_selection_model_get_selection(olist_pref_win.selection[1]);
  if (gtk_bitset_is_empty(set)) {
    gtk_bitset_unref(set);
    uim_custom_free(custom);
    return;
  }

  for (num = 0; custom->value->as_olist[num]; num++)
    ;

  for (valid = gtk_bitset_iter_init_first(&bitset_iter, set, &pos); valid;
       valid = gtk_bitset_iter_next(&bitset_iter, &pos)) {
    UimPrefRow *row;
    const char *symbol;
    struct uim_custom_choice *choice = NULL;
    gint i;

    row = g_list_model_get_item(G_LIST_MODEL(olist_pref_win.store[1]), pos);
    if (!row)
      continue;
    symbol = uim_pref_row_get(row, 1);

    for (i = 0; symbol && custom->range->as_olist.valid_items[i]; i++) {
      if (!strcmp(custom->range->as_olist.valid_items[i]->symbol, symbol)) {
        choice = custom->range->as_olist.valid_items[i];
        break;
      }
    }

    g_object_unref(row);

    if (!choice)
      continue;

    num++;
    custom->value->as_olist =
      realloc(custom->value->as_olist,
              sizeof(struct uim_custom_choice *) * (num + 1));
    custom->value->as_olist[num - 1] = malloc(sizeof(struct uim_custom_choice));
    custom->value->as_olist[num - 1]->symbol =
      choice->symbol ? strdup(choice->symbol) : NULL;
    custom->value->as_olist[num - 1]->label =
      choice->label ? strdup(choice->label) : NULL;
    custom->value->as_olist[num - 1]->desc =
      choice->desc ? strdup(choice->desc) : NULL;
    custom->value->as_olist[num] = NULL;
  }
  gtk_bitset_unref(set);

  urv = uim_custom_set(custom);

  if (urv != UIM_FALSE) {
    sync_value_olist(GTK_ENTRY(olist_entry));
    olist_pref_list_view_set_value(GTK_ENTRY(olist_entry), TRUE, TRUE);
    /* rebuilding the lists drops the selection without emitting
     * "selection-changed", so refresh the buttons here */
    set_olist_buttons_sensitive(olist_entry);
    uim_pref_gtk_mark_value_changed();
  } else {
    /* error message */
  }

  uim_custom_free(custom);
}

static void
olist_pref_right_button_clicked_cb(GtkWidget *widget, GtkEntry *olist_entry)
{
  const char *custom_sym;
  struct uim_custom *custom;
  struct uim_custom_choice *choice;
  GtkBitset *set;
  GtkBitsetIter bitset_iter;
  gboolean valid;
  guint idx;
  gint num;
  uim_bool urv;

  custom_sym =
    g_object_get_data(G_OBJECT(olist_entry), OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_if_fail(custom_sym);

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom && custom->type == UCustom_OrderedList);

  set = gtk_selection_model_get_selection(olist_pref_win.selection[0]);
  if (gtk_bitset_is_empty(set)) {
    gtk_bitset_unref(set);
    uim_custom_free(custom);
    return;
  }

  for (num = 0; custom->value->as_olist[num]; num++)
    ;

  /* remove from the last one so that the remaining indices stay valid */
  for (valid = gtk_bitset_iter_init_last(&bitset_iter, set, &idx); valid;
       valid = gtk_bitset_iter_previous(&bitset_iter, &idx)) {
    if ((gint)idx >= num)
      continue;

    choice = custom->value->as_olist[idx];
    free(choice->symbol);
    free(choice->label);
    free(choice->desc);
    free(choice);

    memmove(custom->value->as_olist + idx,
            custom->value->as_olist + idx + 1,
            sizeof(struct uim_custom_choice *) * (num - idx));
    custom->value->as_olist = realloc(custom->value->as_olist,
                                      sizeof(struct uim_custom_choice *) * num);
    num--;
  }
  gtk_bitset_unref(set);

  urv = uim_custom_set(custom);

  if (urv != UIM_FALSE) {
    sync_value_olist(GTK_ENTRY(olist_entry));
    olist_pref_list_view_set_value(GTK_ENTRY(olist_entry), TRUE, TRUE);
    /* rebuilding the lists drops the selection without emitting
     * "selection-changed", so refresh the buttons here */
    set_olist_buttons_sensitive(olist_entry);
    uim_pref_gtk_mark_value_changed();
    /* FIXME! reset the selection */
  } else {
    /* error message */
  }

  uim_custom_free(custom);
}

static void
olist_pref_selection_changed(GtkSelectionModel *selection,
                             guint position,
                             guint n_items,
                             GtkEntry *olist_entry)
{
  set_olist_buttons_sensitive(olist_entry);
}

static GtkWidget *
create_olist_list_view(GtkWidget *vbox,
                       const char *title,
                       int index,
                       GtkEntry *olist_entry)
{
  GtkWidget *label, *scrwin;

  label = gtk_label_new(title);
  gtk_widget_set_margin_top(label, 4);
  gtk_widget_set_margin_bottom(label, 4);
  gtk_box_append(GTK_BOX(vbox), label);

  scrwin = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW(scrwin), TRUE);
  gtk_widget_set_vexpand(scrwin, TRUE);
  gtk_box_append(GTK_BOX(vbox), scrwin);

  olist_pref_win.store[index] = g_list_store_new(UIM_PREF_TYPE_ROW);
  olist_pref_win.selection[index] = GTK_SELECTION_MODEL(
    gtk_multi_selection_new(G_LIST_MODEL(olist_pref_win.store[index])));
  g_signal_connect(G_OBJECT(olist_pref_win.selection[index]),
                   "selection-changed",
                   G_CALLBACK(olist_pref_selection_changed),
                   olist_entry);
  olist_pref_win.list_view[index] =
    create_list_view(olist_pref_win.selection[index], scrwin);

  return olist_pref_win.list_view[index];
}

static void
choose_olist_clicked_cb(GtkWidget *widget, GtkEntry *olist_entry)
{
  const char *custom_sym;
  struct uim_custom *custom;
  GtkWidget *dialog, *content_area, *hbox, *vbox, *grid;
  GtkWidget *button;

  custom_sym =
    g_object_get_data(G_OBJECT(olist_entry), OBJECT_DATA_UIM_CUSTOM_SYM);
  ;
  g_return_if_fail(custom_sym);
  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom);

  dialog = create_pref_dialog(GTK_WIDGET(olist_entry),
                              custom->label,
                              DEFAULT_OLIST_WINDOW_WIDTH,
                              DEFAULT_OLIST_WINDOW_HEIGHT,
                              &content_area);

  uim_custom_free(custom);

  olist_pref_win.window = dialog;

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_margin_start(hbox, 4);
  gtk_widget_set_margin_end(hbox, 4);
  gtk_widget_set_margin_top(hbox, 4);
  gtk_widget_set_margin_bottom(hbox, 4);
  gtk_widget_set_vexpand(hbox, TRUE);
  gtk_box_append(GTK_BOX(content_area), hbox);

  /* left list view */
  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_hexpand(vbox, TRUE);
  gtk_box_append(GTK_BOX(hbox), vbox);
  create_olist_list_view(vbox, _("Enabled"), 0, olist_entry);

  /* button area */
  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_margin_start(vbox, 4);
  gtk_widget_set_margin_end(vbox, 4);
  gtk_box_append(GTK_BOX(hbox), vbox);

  grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(grid), 3);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 3);
  gtk_widget_set_valign(grid, GTK_ALIGN_CENTER);
  gtk_widget_set_vexpand(vbox, TRUE);
  gtk_box_append(GTK_BOX(vbox), grid);

  /* up button */
  button = gtk_button_new_from_icon_name("go-up-symbolic");
  olist_pref_win.up_button = button;
  gtk_widget_set_hexpand(button, TRUE);
  gtk_widget_set_vexpand(button, TRUE);
  gtk_grid_attach(GTK_GRID(grid), button, 1, 0, 1, 1);
  g_signal_connect(G_OBJECT(button),
                   "clicked",
                   G_CALLBACK(olist_pref_up_button_clicked_cb),
                   olist_entry);

  /* down button */
  button = gtk_button_new_from_icon_name("go-down-symbolic");
  olist_pref_win.down_button = button;
  gtk_widget_set_hexpand(button, TRUE);
  gtk_widget_set_vexpand(button, TRUE);
  gtk_grid_attach(GTK_GRID(grid), button, 1, 2, 1, 1);
  g_signal_connect(G_OBJECT(button),
                   "clicked",
                   G_CALLBACK(olist_pref_down_button_clicked_cb),
                   olist_entry);

  /* left button */
  button = gtk_button_new_from_icon_name("go-previous-symbolic");
  olist_pref_win.left_button = button;
  gtk_widget_set_hexpand(button, TRUE);
  gtk_widget_set_vexpand(button, TRUE);
  gtk_grid_attach(GTK_GRID(grid), button, 0, 1, 1, 1);
  g_signal_connect(G_OBJECT(button),
                   "clicked",
                   G_CALLBACK(olist_pref_left_button_clicked_cb),
                   olist_entry);

  /* right button */
  button = gtk_button_new_from_icon_name("go-next-symbolic");
  olist_pref_win.right_button = button;
  gtk_widget_set_hexpand(button, TRUE);
  gtk_widget_set_vexpand(button, TRUE);
  gtk_grid_attach(GTK_GRID(grid), button, 2, 1, 1, 1);
  g_signal_connect(G_OBJECT(button),
                   "clicked",
                   G_CALLBACK(olist_pref_right_button_clicked_cb),
                   olist_entry);

  /* right list view */
  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_hexpand(vbox, TRUE);
  gtk_box_append(GTK_BOX(hbox), vbox);
  create_olist_list_view(vbox, _("Disabled"), 1, olist_entry);

  /* set value */
  olist_pref_list_view_set_value(olist_entry, TRUE, TRUE);
  set_olist_buttons_sensitive(olist_entry);

  /* show dialog */
  gtk_window_present(GTK_WINDOW(dialog));
}

static void
update_custom_type_olist_cb(void *ptr, const char *custom_sym)
{
  sync_value_olist(GTK_ENTRY(ptr));
}

static void
add_custom_type_orderedlist(GtkWidget *vbox, struct uim_custom *custom)
{
  GtkWidget *hbox;
  GtkWidget *label, *entry, *button;

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_append(GTK_BOX(vbox), hbox);

  label = gtk_label_new(custom->label);
  gtk_box_append(GTK_BOX(hbox), label);

  entry = gtk_entry_new();
  gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
  gtk_widget_set_hexpand(entry, TRUE);
  gtk_box_append(GTK_BOX(hbox), entry);

  button = gtk_button_new_with_label(_("Edit..."));
  gtk_box_append(GTK_BOX(hbox), button);

  g_object_set_data_full(G_OBJECT(entry),
                         OBJECT_DATA_UIM_CUSTOM_SYM,
                         g_strdup(custom->symbol),
                         (GDestroyNotify)g_free);

  sync_value_olist(GTK_ENTRY(entry));

  g_signal_connect(G_OBJECT(button),
                   "clicked",
                   G_CALLBACK(choose_olist_clicked_cb),
                   entry);
  uim_custom_cb_add(custom->symbol, entry, update_custom_type_olist_cb);
}

static void
key_pref_selection_changed(GObject *selection, GParamSpec *pspec, gpointer data)
{
  gboolean selected =
    gtk_single_selection_get_selected(GTK_SINGLE_SELECTION(selection)) !=
    GTK_INVALID_LIST_POSITION;

  gtk_widget_set_sensitive(key_pref_win.remove_button, selected);
}

static void
key_pref_set_value(gint ukey, gint umod)
{
  GString *keystr;
  const char *sym;

  keystr = g_string_new("");
  /*
   * Ignore Shift modifier for printable char keys for
   * easy-to-recognize key configuration.  uim-custom performs
   * implicit shift key encoding/decoding appropriately.
   */
  if (((ukey >= 256) || !g_ascii_isgraph(ukey)) && (umod & UMod_Shift))
    g_string_append(keystr, "<Shift>");
  if (umod & UMod_Control)
    g_string_append(keystr, "<Control>");
  if (umod & UMod_Alt)
    g_string_append(keystr, "<Alt>");
  if (umod & UMod_Meta)
    g_string_append(keystr, "<Meta>");
  if (umod & UMod_Super)
    g_string_append(keystr, "<Super>");
  if (umod & UMod_Hyper)
    g_string_append(keystr, "<Hyper>");

  switch (ukey) {
  case 0x20:
    /*
     * "space" is not proper uim keysym and only exists for user
     * convenience. It is converted to " " by uim-custom
     */
    g_string_append(keystr, "space");
    break;
  default:
    if (ukey < 128) {
      g_string_append_printf(keystr, "%c", ukey);
    } else if ((sym = uim_pref_get_keysym(ukey))) {
      g_string_append(keystr, sym);
    } else {
      /* UKey_Other */
    }
    break;
  }

  gtk_editable_set_text(GTK_EDITABLE(key_pref_win.keycode_entry), keystr->str);

  key_pref_win.grabbed_key_val = 0;
  key_pref_win.grabbed_key_state = 0;

  g_string_free(keystr, TRUE);
}

static gboolean
grab_win_key_pressed_cb(GtkEventControllerKey *controller,
                        guint keyval,
                        guint keycode,
                        GdkModifierType state,
                        gpointer data)
{
  GdkEvent *event =
    gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));

  im_uim_convert_keyevent(event,
                          &key_pref_win.grabbed_key_val,
                          &key_pref_win.grabbed_key_state);

  return TRUE;
}

static gboolean
grab_win_key_released_cb(GtkEventControllerKey *controller,
                         guint keyval,
                         guint keycode,
                         GdkModifierType state,
                         GtkWindow *grab_window)
{
  GdkEvent *event =
    gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));

  key_pref_set_value(key_pref_win.grabbed_key_val,
                     key_pref_win.grabbed_key_state);

  im_uim_convert_keyevent(event,
                          &key_pref_win.grabbed_key_val,
                          &key_pref_win.grabbed_key_state);

  gtk_window_destroy(grab_window);

  return TRUE;
}

static gboolean
key_choose_entry_key_pressed_cb(GtkEventControllerKey *controller,
                                guint keyval,
                                guint keycode,
                                GdkModifierType state,
                                gpointer data)
{
  GdkEvent *event =
    gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
  int ukey, umod;

  im_uim_convert_keyevent(event, &ukey, &umod);
  key_pref_set_value(ukey, umod);

  return TRUE;
}

static gboolean
key_choose_entry_key_released_cb(GtkEventControllerKey *controller,
                                 guint keyval,
                                 guint keycode,
                                 GdkModifierType state,
                                 gpointer data)
{
  GdkEvent *event =
    gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
  int ukey, umod;

  im_uim_convert_keyevent(event, &ukey, &umod);

  return TRUE;
}

static void
choose_key_button_clicked_cb(GtkWidget *widget, GtkEntry *key_entry)
{
  GtkWidget *dialog, *label;
  GtkEventController *key_controller;
  GtkRoot *root;

  dialog = gtk_window_new();
  gtk_window_set_title(GTK_WINDOW(dialog), _("Grabbing keys"));
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  root = gtk_widget_get_root(widget);
  if (GTK_IS_WINDOW(root))
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(root));
  gtk_window_set_default_size(GTK_WINDOW(dialog), 320, -1);

  label = gtk_label_new(_("Press keys to grab (e.g. <Control>a)"));
  gtk_widget_set_margin_start(label, 16);
  gtk_widget_set_margin_end(label, 16);
  gtk_widget_set_margin_top(label, 16);
  gtk_widget_set_margin_bottom(label, 16);
  gtk_window_set_child(GTK_WINDOW(dialog), label);

  key_controller = gtk_event_controller_key_new();
  gtk_widget_add_controller(dialog, key_controller);
  g_signal_connect(key_controller,
                   "key-pressed",
                   G_CALLBACK(grab_win_key_pressed_cb),
                   NULL);
  g_signal_connect(key_controller,
                   "key-released",
                   G_CALLBACK(grab_win_key_released_cb),
                   dialog);

  gtk_window_present(GTK_WINDOW(dialog));
}

static void
sync_value_key(GtkEntry *entry)
{
  GString *str;
  const char *custom_sym;
  struct uim_custom *custom;
  struct uim_custom_key *key;
  gint i;

  custom_sym = g_object_get_data(G_OBJECT(entry), OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_if_fail(custom_sym);

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom && custom->type == UCustom_Key);

  str = g_string_new("");

  if (custom->value->as_key) {
    for (key = custom->value->as_key[0], i = 0; key;
         key = custom->value->as_key[++i]) {
      if (i != 0)
        g_string_append(str, ", ");
      g_string_append_printf(str, "\"%s\"", key->literal);
    }
  } else {
    /* error message */
  }

  gtk_editable_set_text(GTK_EDITABLE(entry), str->str);
  gtk_widget_set_sensitive(gtk_widget_get_parent(GTK_WIDGET(entry)),
                           custom->is_active);

  g_string_free(str, TRUE);
  uim_custom_free(custom);
}

static void
key_pref_add_button_clicked_cb(GtkWidget *widget, GtkEntry *key_entry)
{
  const char *custom_sym;
  struct uim_custom *custom;
  const char *key_code;
  GString *str;
  UimPrefRow *row;
  gint num;
  uim_bool rv;

  g_return_if_fail(GTK_IS_ENTRY(key_entry));

  custom_sym =
    g_object_get_data(G_OBJECT(key_entry), OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_if_fail(custom_sym);

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom && custom->type == UCustom_Key);

  key_code = gtk_editable_get_text(GTK_EDITABLE(key_pref_win.keycode_entry));
  if (!key_code || !*key_code) {
    uim_custom_free(custom);
    return;
  }

  str = g_string_new("");

  g_string_append(str, key_code);

  for (num = 0; custom->value->as_key[num]; num++)
    ;
  num++;

  custom->value->as_key =
    realloc(custom->value->as_key, sizeof(struct uim_custom_key *) * (num + 1));

  custom->value->as_key[num - 1] = malloc(sizeof(struct uim_custom_key));
  custom->value->as_key[num - 1]->type = UCustomKey_Regular;
  custom->value->as_key[num - 1]->editor_type = UCustomKeyEditor_Basic;
  custom->value->as_key[num - 1]->literal = strdup(str->str);
  custom->value->as_key[num - 1]->label = strdup("");
  custom->value->as_key[num - 1]->desc = strdup("");
  custom->value->as_key[num] = NULL;

  rv = uim_custom_set(custom);

  if (rv != UIM_FALSE) {
    uim_pref_gtk_mark_value_changed();
    row = uim_pref_row_new(1);
    uim_pref_row_set(row, 0, str->str);
    g_list_store_append(key_pref_win.store, row);
    g_object_unref(row);
    sync_value_key(key_entry);
  } else {
    g_printerr("Failed to set key value for \"%s\".\n", custom->symbol);
  }

  g_string_free(str, TRUE);
  uim_custom_free(custom);
}

static void
key_pref_remove_button_clicked_cb(GtkWidget *widget, GtkEntry *key_entry)
{
  const char *custom_sym;
  struct uim_custom *custom;
  guint idx;
  gint num;
  uim_bool rv;

  g_return_if_fail(GTK_IS_ENTRY(key_entry));

  custom_sym =
    g_object_get_data(G_OBJECT(key_entry), OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_if_fail(custom_sym);

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom && custom->type == UCustom_Key);

  idx = gtk_single_selection_get_selected(key_pref_win.selection);
  if (idx == GTK_INVALID_LIST_POSITION) {
    uim_custom_free(custom);
    return;
  }

  for (num = 0; custom->value->as_key[num]; num++)
    ;

  if (num > 0 && (gint)idx < num) {
    free(custom->value->as_key[idx]->literal);
    free(custom->value->as_key[idx]->label);
    free(custom->value->as_key[idx]->desc);
    free(custom->value->as_key[idx]);

    memmove(custom->value->as_key + idx,
            custom->value->as_key + idx + 1,
            sizeof(struct uim_custom_key *) * (num - idx));
    custom->value->as_key =
      realloc(custom->value->as_key, sizeof(struct uim_custom_key *) * num);

    rv = uim_custom_set(custom);

    if (rv != UIM_FALSE) {
      uim_pref_gtk_mark_value_changed();
      g_list_store_remove(key_pref_win.store, idx);

      sync_value_key(key_entry);
    } else {
      g_printerr("Failed to set key value for \"%s\".\n", custom->symbol);
    }
  } else {
    /* error */
  }

  gtk_widget_set_sensitive(
    key_pref_win.remove_button,
    gtk_single_selection_get_selected(key_pref_win.selection) !=
      GTK_INVALID_LIST_POSITION);

  uim_custom_free(custom);
}

static void
key_val_entry_changed_cb(GtkEntry *entry, GtkEntry *key_entry)
{
  const char *str = gtk_editable_get_text(GTK_EDITABLE(entry));

  /* FIXME! validate key value */
  gtk_widget_set_sensitive(key_pref_win.add_button, str && *str);
}

static void
choose_key_clicked_cb(GtkWidget *widget, GtkEntry *key_entry)
{
  GtkWidget *dialog, *content_area, *scrwin, *hbox, *vbox, *label, *button,
    *entry;
  GtkEventController *entry_key_controller;
  const char *custom_sym;
  struct uim_custom *custom;
  struct uim_custom_key *key;
  gint i;
  gchar title[256];

  g_return_if_fail(GTK_IS_ENTRY(key_entry));

  custom_sym =
    g_object_get_data(G_OBJECT(key_entry), OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_if_fail(custom_sym);

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom && custom->type == UCustom_Key);

  g_snprintf(title, sizeof(title), _("%s - key configuration"), custom->label);

  /* setup key pref dialog */
  dialog = create_pref_dialog(GTK_WIDGET(key_entry),
                              title,
                              DEFAULT_KEYCONF_WINDOW_WIDTH,
                              DEFAULT_KEYCONF_WINDOW_HEIGHT,
                              &content_area);
  key_pref_win.window = dialog;

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_margin_start(hbox, 4);
  gtk_widget_set_margin_end(hbox, 4);
  gtk_widget_set_margin_top(hbox, 4);
  gtk_widget_set_margin_bottom(hbox, 4);
  gtk_widget_set_vexpand(hbox, TRUE);
  gtk_box_append(GTK_BOX(content_area), hbox);

  scrwin = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW(scrwin), TRUE);
  gtk_widget_set_hexpand(scrwin, TRUE);
  gtk_box_append(GTK_BOX(hbox), scrwin);

  key_pref_win.store = g_list_store_new(UIM_PREF_TYPE_ROW);
  key_pref_win.selection =
    gtk_single_selection_new(G_LIST_MODEL(key_pref_win.store));
  /* start with nothing selected, as GtkTreeSelection did */
  gtk_single_selection_set_autoselect(key_pref_win.selection, FALSE);
  gtk_single_selection_set_can_unselect(key_pref_win.selection, TRUE);
  gtk_single_selection_set_selected(key_pref_win.selection,
                                    GTK_INVALID_LIST_POSITION);
  g_signal_connect(G_OBJECT(key_pref_win.selection),
                   "notify::selected",
                   G_CALLBACK(key_pref_selection_changed),
                   key_entry);
  key_pref_win.list_view =
    create_list_view(GTK_SELECTION_MODEL(key_pref_win.selection), scrwin);

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_margin_start(vbox, 4);
  gtk_box_append(GTK_BOX(hbox), vbox);

  button = gtk_button_new_from_icon_name("list-add-symbolic");
  key_pref_win.add_button = button;
  gtk_box_append(GTK_BOX(vbox), button);
  g_signal_connect(G_OBJECT(button),
                   "clicked",
                   G_CALLBACK(key_pref_add_button_clicked_cb),
                   key_entry);
  gtk_widget_set_sensitive(button, FALSE);

  button = gtk_button_new_from_icon_name("list-remove-symbolic");
  key_pref_win.remove_button = button;
  gtk_box_append(GTK_BOX(vbox), button);
  g_signal_connect(G_OBJECT(button),
                   "clicked",
                   G_CALLBACK(key_pref_remove_button_clicked_cb),
                   key_entry);
  gtk_widget_set_sensitive(button, FALSE);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_margin_start(hbox, 4);
  gtk_widget_set_margin_end(hbox, 4);
  gtk_widget_set_margin_top(hbox, 4);
  gtk_widget_set_margin_bottom(hbox, 4);
  gtk_box_append(GTK_BOX(content_area), hbox);

  label = gtk_label_new(_("Key:"));
  gtk_box_append(GTK_BOX(hbox), label);

  entry = gtk_entry_new();
  gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
  key_pref_win.keycode_entry = entry;
  gtk_widget_set_size_request(GTK_WIDGET(entry), 100, -1);
  gtk_widget_set_hexpand(entry, TRUE);
  gtk_box_append(GTK_BOX(hbox), entry);
  g_signal_connect(G_OBJECT(entry),
                   "changed",
                   G_CALLBACK(key_val_entry_changed_cb),
                   key_entry);

  entry_key_controller = gtk_event_controller_key_new();
  gtk_widget_add_controller(entry, entry_key_controller);
  g_signal_connect(entry_key_controller,
                   "key-pressed",
                   G_CALLBACK(key_choose_entry_key_pressed_cb),
                   key_entry);
  g_signal_connect(entry_key_controller,
                   "key-released",
                   G_CALLBACK(key_choose_entry_key_released_cb),
                   key_entry);

  button = gtk_button_new_with_label(_("Grab..."));
  gtk_box_append(GTK_BOX(hbox), button);
  g_signal_connect(G_OBJECT(button),
                   "clicked",
                   G_CALLBACK(choose_key_button_clicked_cb),
                   key_entry);

  /* set value */
  if (custom->value->as_key) {
    for (key = custom->value->as_key[0], i = 0; key;
         key = custom->value->as_key[++i]) {
      UimPrefRow *row = uim_pref_row_new(1);

      uim_pref_row_set(row, 0, key->literal);
      g_list_store_append(key_pref_win.store, row);
      g_object_unref(row);
    }
  } else {
    /* error message */
  }

  /* show dialog */
  gtk_window_present(GTK_WINDOW(dialog));

  /* clean */
  uim_custom_free(custom);
}

static void
update_custom_type_key_cb(void *ptr, const char *custom_sym)
{
  sync_value_key(GTK_ENTRY(ptr));
}

static void
add_custom_type_key(GtkWidget *vbox, struct uim_custom *custom)
{
  GtkWidget *hbox;
  GtkWidget *label, *entry, *button;

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  label = gtk_label_new(custom->label);
  gtk_box_append(GTK_BOX(hbox), label);

  entry = gtk_entry_new();
  gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
  g_object_set_data_full(G_OBJECT(entry),
                         OBJECT_DATA_UIM_CUSTOM_SYM,
                         g_strdup(custom->symbol),
                         (GDestroyNotify)g_free);
  gtk_widget_set_hexpand(entry, TRUE);
  gtk_box_append(GTK_BOX(hbox), entry);

  sync_value_key(GTK_ENTRY(entry));

  button = gtk_button_new_with_label(_("Edit..."));
  gtk_box_append(GTK_BOX(hbox), button);
  g_signal_connect(G_OBJECT(button),
                   "clicked",
                   G_CALLBACK(choose_key_clicked_cb),
                   entry);
  uim_custom_cb_add(custom->symbol, entry, update_custom_type_key_cb);

  gtk_box_append(GTK_BOX(vbox), hbox);
}

static gboolean
table_pref_dialog_close_request_cb(GtkWindow *dialog, GtkWidget *column_view)
{
  gpointer table_label =
    g_object_get_data(G_OBJECT(column_view), OBJECT_DATA_TABLE_LABEL);
  GListModel *model = G_LIST_MODEL(
    g_object_get_data(G_OBJECT(column_view), OBJECT_DATA_TABLE_STORE));
  const char *custom_sym;
  struct uim_custom *custom;
  char ***custom_table;
  guint n_rows;
  guint row;
  int column;
  uim_bool rv;

  custom_sym =
    g_object_get_data(G_OBJECT(table_label), OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_val_if_fail(custom_sym, FALSE);

  custom = uim_custom_get(custom_sym);
  g_return_val_if_fail(custom && custom->type == UCustom_Table, FALSE);

  custom_table = custom->value->as_table;
  for (row = 0; custom_table[row]; row++) {
    for (column = 0; custom_table[row][column]; column++) {
      free(custom_table[row][column]);
    }
    free(custom_table[row]);
  }
  free(custom_table);

  n_rows = g_list_model_get_n_items(model);
  custom_table = (char ***)malloc(sizeof(char **) * (n_rows + 1));
  custom_table[n_rows] = NULL;

  for (row = 0; row < n_rows; row++) {
    UimPrefRow *item = g_list_model_get_item(model, row);
    int n_columns = (int)uim_pref_row_get_n_columns(item);
    int n_columnsForRow = n_columns;

    /* the number of column may differ from row to row */
    for (column = 0; column < n_columns; column++) {
      if (!uim_pref_row_get(item, column)) {
        n_columnsForRow = column;
        break;
      }
    }
    custom_table[row] = (char **)malloc(sizeof(char *) * (n_columnsForRow + 1));
    custom_table[row][n_columnsForRow] = NULL;
    for (column = 0; column < n_columnsForRow; column++) {
      const gchar *str = uim_pref_row_get(item, column);
      custom_table[row][column] = strdup(str ? str : "");
    }
    g_object_unref(item);
  }
  custom->value->as_table = custom_table;

  rv = uim_custom_set(custom);

  if (!rv) {
    g_printerr("Failed to set table value for \"%s\".\n", custom->symbol);
    /* FIXME! reset the widget */
  }
  uim_custom_free(custom);

  return FALSE; /* let the window be destroyed */
}

static void
sync_value_table(GtkLabel *label)
{
  const char *custom_sym;
  struct uim_custom *custom;

  g_return_if_fail(label);

  custom_sym = g_object_get_data(G_OBJECT(label), OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_if_fail(custom_sym);

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom && custom->type == UCustom_Table);

  gtk_widget_set_sensitive(gtk_widget_get_parent(GTK_WIDGET(label)),
                           custom->is_active);
  uim_custom_free(custom);
}

static void
table_pref_cell_editing_changed(GObject *editable_label,
                                GParamSpec *pspec,
                                gpointer data)
{
  UimPrefRow *row = g_object_get_data(editable_label, OBJECT_DATA_ROW);
  guint column = GPOINTER_TO_UINT(data);
  const char *old_text, *new_text;

  if (!row ||
      gtk_editable_label_get_editing(GTK_EDITABLE_LABEL(editable_label)))
    return;

  old_text = uim_pref_row_get(row, column);
  new_text = gtk_editable_get_text(GTK_EDITABLE(editable_label));
  if (!strcmp(old_text ? old_text : "", new_text))
    return; /* not edited, or the edit was cancelled */

  if (uim_pref_row_set(row, column, new_text))
    uim_pref_gtk_mark_value_changed();
}

static void
table_pref_cell_setup_cb(GtkSignalListItemFactory *factory,
                         GObject *object,
                         gpointer data)
{
  GtkWidget *editable_label = gtk_editable_label_new("");

  g_signal_connect(G_OBJECT(editable_label),
                   "notify::editing",
                   G_CALLBACK(table_pref_cell_editing_changed),
                   data);
  gtk_list_item_set_child(GTK_LIST_ITEM(object), editable_label);
}

static void
table_pref_cell_bind_cb(GtkSignalListItemFactory *factory,
                        GObject *object,
                        gpointer data)
{
  GtkListItem *item = GTK_LIST_ITEM(object);
  GtkWidget *editable_label = gtk_list_item_get_child(item);
  UimPrefRow *row = gtk_list_item_get_item(item);
  const char *str = uim_pref_row_get(row, GPOINTER_TO_UINT(data));

  g_object_set_data(G_OBJECT(editable_label), OBJECT_DATA_ROW, NULL);
  gtk_editable_set_text(GTK_EDITABLE(editable_label), str ? str : "");
  g_object_set_data(G_OBJECT(editable_label), OBJECT_DATA_ROW, row);
}

static void
table_pref_cell_unbind_cb(GtkSignalListItemFactory *factory,
                          GObject *object,
                          gpointer data)
{
  GtkWidget *editable_label = gtk_list_item_get_child(GTK_LIST_ITEM(object));

  g_object_set_data(G_OBJECT(editable_label), OBJECT_DATA_ROW, NULL);
}

static GtkWidget *
create_table_column_view(struct uim_custom *custom)
{
  GtkSingleSelection *selection;
  GtkWidget *column_view;
  GListStore *store;
  char ***custom_table;
  gint n_columns;
  int row;
  int column;
  struct uim_custom_choice **item;

  custom_table = custom->value->as_table;
  /* the number of column may differ from row to row */
  n_columns = -1;
  for (row = 0; custom_table[row]; row++) {
    for (column = 0; custom_table[row][column]; column++) {
      if (n_columns < column)
        n_columns = column;
    }
  }
  n_columns++;

  store = g_list_store_new(UIM_PREF_TYPE_ROW);
  for (row = 0; custom_table[row]; row++) {
    UimPrefRow *table_row = uim_pref_row_new(n_columns);
    /* cells after the first missing one stay NULL */
    for (column = 0; column < n_columns && custom_table[row][column]; column++)
      uim_pref_row_set(table_row, column, custom_table[row][column]);
    g_list_store_append(store, table_row);
    g_object_unref(table_row);
  }

  selection = gtk_single_selection_new(G_LIST_MODEL(store));
  /* start with nothing selected, as GtkTreeSelection did */
  gtk_single_selection_set_autoselect(selection, FALSE);
  gtk_single_selection_set_can_unselect(selection, TRUE);
  gtk_single_selection_set_selected(selection, GTK_INVALID_LIST_POSITION);

  column_view = gtk_column_view_new(GTK_SELECTION_MODEL(selection));
  g_object_set_data(G_OBJECT(column_view), OBJECT_DATA_TABLE_STORE, store);
  g_object_set_data(G_OBJECT(column_view),
                    OBJECT_DATA_TABLE_SELECTION,
                    selection);
  g_object_set_data(G_OBJECT(column_view),
                    OBJECT_DATA_TABLE_N_COLUMNS,
                    GUINT_TO_POINTER(n_columns));

  column = 0;
  for (item = custom->range->as_table_header.valid_items; *item; item++) {
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
    GtkColumnViewColumn *view_column;

    g_signal_connect(factory,
                     "setup",
                     G_CALLBACK(table_pref_cell_setup_cb),
                     GUINT_TO_POINTER(column));
    g_signal_connect(factory,
                     "bind",
                     G_CALLBACK(table_pref_cell_bind_cb),
                     GUINT_TO_POINTER(column));
    g_signal_connect(factory,
                     "unbind",
                     G_CALLBACK(table_pref_cell_unbind_cb),
                     GUINT_TO_POINTER(column));

    view_column = gtk_column_view_column_new((*item)->label, factory);
    gtk_column_view_append_column(GTK_COLUMN_VIEW(column_view), view_column);
    g_object_unref(view_column);
    column++;
  }
  return column_view;
}

static void
table_pref_add_button_clicked_cb(GtkWidget *widget, GtkWidget *column_view)
{
  GListStore *store =
    g_object_get_data(G_OBJECT(column_view), OBJECT_DATA_TABLE_STORE);
  guint n_columns = GPOINTER_TO_UINT(
    g_object_get_data(G_OBJECT(column_view), OBJECT_DATA_TABLE_N_COLUMNS));
  UimPrefRow *row;
  guint column, n_rows;

  row = uim_pref_row_new(n_columns);
  for (column = 0; column < n_columns; column++)
    uim_pref_row_set(row, column, "");
  g_list_store_append(store, row);
  g_object_unref(row);

  n_rows = g_list_model_get_n_items(G_LIST_MODEL(store));
  gtk_column_view_scroll_to(GTK_COLUMN_VIEW(column_view),
                            n_rows - 1,
                            NULL,
                            GTK_LIST_SCROLL_NONE,
                            NULL);
  uim_pref_gtk_mark_value_changed();
}

static void
table_pref_remove_button_clicked_cb(GtkWidget *widget, GtkWidget *column_view)
{
  GListStore *store =
    g_object_get_data(G_OBJECT(column_view), OBJECT_DATA_TABLE_STORE);
  GtkSingleSelection *selection =
    g_object_get_data(G_OBJECT(column_view), OBJECT_DATA_TABLE_SELECTION);
  guint idx = gtk_single_selection_get_selected(selection);

  if (idx == GTK_INVALID_LIST_POSITION ||
      idx >= g_list_model_get_n_items(G_LIST_MODEL(store)))
    return;

  g_list_store_remove(store, idx);
  uim_pref_gtk_mark_value_changed();
}

static void
table_pref_move_button_clicked_cb(GtkWidget *widget,
                                  GtkWidget *column_view,
                                  gboolean up)
{
  GListStore *store =
    g_object_get_data(G_OBJECT(column_view), OBJECT_DATA_TABLE_STORE);
  GtkSingleSelection *selection =
    g_object_get_data(G_OBJECT(column_view), OBJECT_DATA_TABLE_SELECTION);
  guint idx = gtk_single_selection_get_selected(selection);
  guint n_rows = g_list_model_get_n_items(G_LIST_MODEL(store));

  if (idx == GTK_INVALID_LIST_POSITION || idx >= n_rows)
    return;
  if ((up && idx == 0) || (!up && idx + 1 >= n_rows))
    return;

  /* sync the view, keeping the moved row selected */
  list_store_swap_with_next(store, up ? idx - 1 : idx);
  gtk_column_view_scroll_to(GTK_COLUMN_VIEW(column_view),
                            up ? idx - 1 : idx + 1,
                            NULL,
                            GTK_LIST_SCROLL_SELECT,
                            NULL);
  uim_pref_gtk_mark_value_changed();
}

static void
table_pref_up_button_clicked_cb(GtkWidget *widget, GtkWidget *column_view)
{
  table_pref_move_button_clicked_cb(widget, column_view, TRUE);
}

static void
table_pref_down_button_clicked_cb(GtkWidget *widget, GtkWidget *column_view)
{
  table_pref_move_button_clicked_cb(widget, column_view, FALSE);
}

static void
create_table_button(GtkWidget *vbox,
                    const char *icon_name,
                    void(func)(GtkWidget *, GtkWidget *),
                    GtkWidget *column_view)
{
  GtkWidget *button = gtk_button_new_from_icon_name(icon_name);
  gtk_box_append(GTK_BOX(vbox), button);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(func), column_view);
}

static void
choose_table_clicked_cb(GtkWidget *widget, GtkWidget *table_label)
{
  GtkWidget *dialog;
  GtkWidget *content_area;
  GtkWidget *hbox;
  const char *custom_sym;
  struct uim_custom *custom;
  GtkWidget *column_view;
  GtkWidget *scrwin;
  GtkWidget *vbox;

  custom_sym =
    g_object_get_data(G_OBJECT(table_label), OBJECT_DATA_UIM_CUSTOM_SYM);
  g_return_if_fail(custom_sym);

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom && custom->type == UCustom_Table);

  dialog = create_pref_dialog(table_label,
                              dgettext(GETTEXT_PACKAGE, custom->label),
                              DEFAULT_TABLE_WINDOW_WIDTH,
                              DEFAULT_TABLE_WINDOW_HEIGHT,
                              &content_area);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_margin_start(hbox, 4);
  gtk_widget_set_margin_end(hbox, 4);
  gtk_widget_set_margin_top(hbox, 4);
  gtk_widget_set_margin_bottom(hbox, 4);
  gtk_widget_set_vexpand(hbox, TRUE);
  gtk_box_append(GTK_BOX(content_area), hbox);

  column_view = create_table_column_view(custom);
  g_object_set_data(G_OBJECT(column_view),
                    OBJECT_DATA_TABLE_LABEL,
                    table_label);
  /* the table is written back however the dialog gets closed, just as
   * GtkDialog's "response" covered both the button and the close request */
  g_signal_connect(G_OBJECT(dialog),
                   "close-request",
                   G_CALLBACK(table_pref_dialog_close_request_cb),
                   column_view);

  scrwin = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW(scrwin), TRUE);
  gtk_widget_set_hexpand(scrwin, TRUE);
  gtk_box_append(GTK_BOX(hbox), scrwin);

  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrwin), column_view);

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_margin_start(vbox, 4);
  gtk_widget_set_valign(vbox, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(hbox), vbox);

  create_table_button(vbox,
                      "list-add-symbolic",
                      table_pref_add_button_clicked_cb,
                      column_view);
  create_table_button(vbox,
                      "list-remove-symbolic",
                      table_pref_remove_button_clicked_cb,
                      column_view);
  create_table_button(vbox,
                      "go-up-symbolic",
                      table_pref_up_button_clicked_cb,
                      column_view);
  create_table_button(vbox,
                      "go-down-symbolic",
                      table_pref_down_button_clicked_cb,
                      column_view);

  gtk_window_present(GTK_WINDOW(dialog));

  uim_custom_free(custom);
}

static void
update_custom_type_table_cb(void *ptr, const char *custom_sym)
{
  sync_value_table(GTK_LABEL(ptr));
}

static void
add_custom_type_table(GtkWidget *vbox, struct uim_custom *custom)
{
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *button;

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

  label = gtk_label_new(custom->label);
  g_object_set_data_full(G_OBJECT(label),
                         OBJECT_DATA_UIM_CUSTOM_SYM,
                         g_strdup(custom->symbol),
                         (GDestroyNotify)g_free);
  uim_custom_cb_add(custom->symbol, label, update_custom_type_table_cb);
  gtk_box_append(GTK_BOX(hbox), label);

  button = gtk_button_new_with_label(_("Edit..."));
  gtk_box_append(GTK_BOX(hbox), button);
  g_signal_connect(G_OBJECT(button),
                   "clicked",
                   G_CALLBACK(choose_table_clicked_cb),
                   label);

  gtk_box_append(GTK_BOX(vbox), hbox);

  sync_value_table(GTK_LABEL(label));
}

void
uim_pref_gtk_add_custom(GtkWidget *vbox, const char *custom_sym)
{
  struct uim_custom *custom;
  custom = uim_custom_get(custom_sym);

  if (custom) {
    /* Should be rewritten with table. */
    switch (custom->type) {
    case UCustom_Bool:
      add_custom_type_bool(vbox, custom);
      break;
    case UCustom_Int:
      add_custom_type_integer(vbox, custom);
      break;
    case UCustom_Str:
      add_custom_type_string(vbox, custom);
      break;
    case UCustom_Pathname:
      add_custom_type_pathname(vbox, custom);
      break;
    case UCustom_Choice:
      add_custom_type_choice(vbox, custom);
      break;
    case UCustom_OrderedList:
      add_custom_type_orderedlist(vbox, custom);
      break;
    case UCustom_Key:
      add_custom_type_key(vbox, custom);
      break;
    case UCustom_Table:
      add_custom_type_table(vbox, custom);
      break;
    default:
      g_printerr("Invalid custom type: %d\n", custom->type);
      break;
    }
  } else {
    g_printerr("Failed to get uim_custom object for %s.\n", custom_sym);
  }

  uim_custom_free(custom);
}

void
uim_pref_gtk_set_default_value(GtkWidget *widget)
{
  const char *custom_sym;
  struct uim_custom *custom;
  union uim_custom_value *value, *defval;
  uim_bool rv;
  gint i, num;
  int type;

  custom_sym = g_object_get_data(G_OBJECT(widget), OBJECT_DATA_UIM_CUSTOM_SYM);
  if (!custom_sym)
    return;

  custom = uim_custom_get(custom_sym);
  g_return_if_fail(custom);

  value = custom->value;
  defval = custom->default_value;
  type = custom->type;

  switch (type) {
  case UCustom_Bool:
    value->as_bool = defval->as_bool;
    break;
  case UCustom_Int:
    value->as_int = defval->as_int;
    break;
  case UCustom_Str:
    free(value->as_str);
    value->as_str = strdup(defval->as_str);
    break;
  case UCustom_Pathname:
    free(value->as_pathname->str);
    value->as_pathname->str = strdup(defval->as_pathname->str);
    value->as_pathname->type = defval->as_pathname->type;
    break;
  case UCustom_Choice:
    free(value->as_choice->symbol);
    free(value->as_choice->label);
    free(value->as_choice->desc);
    value->as_choice->symbol = strdup(defval->as_choice->symbol);
    value->as_choice->label = strdup(defval->as_choice->label);
    value->as_choice->desc = strdup(defval->as_choice->desc);
    break;
  case UCustom_OrderedList:
    for (num = 0; defval->as_olist[num]; num++)
      ;
    for (i = 0; value->as_olist[i]; i++) {
      free(value->as_olist[i]->symbol);
      free(value->as_olist[i]->label);
      free(value->as_olist[i]->desc);
      free(value->as_olist[i]);
    }
    value->as_olist =
      realloc(value->as_olist, sizeof(struct uim_custom_choice *) * (num + 1));
    for (i = 0; i < num; i++) {
      value->as_olist[i] = malloc(sizeof(struct uim_custom_choice));
      value->as_olist[i]->symbol = strdup(defval->as_olist[i]->symbol);
      value->as_olist[i]->label = strdup(defval->as_olist[i]->label);
      value->as_olist[i]->desc = strdup(defval->as_olist[i]->desc);
    }
    value->as_olist[num] = NULL;
    break;
  case UCustom_Key:
    for (num = 0; defval->as_key[num]; num++)
      ;
    for (i = 0; value->as_key[i]; i++) {
      free(value->as_key[i]->literal);
      free(value->as_key[i]->label);
      free(value->as_key[i]->desc);
      free(value->as_key[i]);
    }
    value->as_key =
      realloc(value->as_key, sizeof(struct uim_custom_key *) * (num + 1));
    for (i = 0; i < num; i++) {
      value->as_key[i] = malloc(sizeof(struct uim_custom_key));
      *value->as_key[i] = *defval->as_key[i];
      value->as_key[i]->literal = strdup(defval->as_key[i]->literal);
      value->as_key[i]->label = strdup(defval->as_key[i]->label);
      value->as_key[i]->desc = strdup(defval->as_key[i]->desc);
    }
    value->as_key[num] = NULL;
    break;
  case UCustom_Table:
    {
      char ***custom_table = value->as_table;
      char ***default_table = defval->as_table;
      int row;
      int column;
      for (row = 0; custom_table[row]; row++) {
        for (column = 0; custom_table[row][column]; column++) {
          free(custom_table[row][column]);
        }
        free(custom_table[row]);
      }
      default_table = defval->as_table;
      for (row = 0; default_table[row]; row++)
        ;
      custom_table = (char ***)malloc(sizeof(char **) * (row + 1));
      custom_table[row] = 0;

      value->as_table = custom_table;

      for (row = 0; default_table[row]; row++) {
        /* the number of column may differ from row to row */
        for (column = 0; default_table[row][column]; column++)
          ;
        custom_table[row] = (char **)malloc(sizeof(char *) * (column + 1));
        custom_table[row][column] = 0;
        for (column = 0; default_table[row][column]; column++)
          custom_table[row][column] = strdup(default_table[row][column]);
      }
    }
    break;
  default:
    uim_custom_free(custom);
    return;
  }

  rv = uim_custom_set(custom);
  if (rv == UIM_FALSE) {
    g_printerr("Failed to set value for \"%s\".\n", custom->symbol);
    uim_custom_free(custom);
    return;
  }
  uim_custom_free(custom);

  switch (type) {
  case UCustom_Bool:
    sync_value_bool(GTK_CHECK_BUTTON(widget));
    break;
  case UCustom_Int:
    sync_value_int(GTK_SPIN_BUTTON(widget));
    break;
  case UCustom_Str:
    sync_value_string(GTK_ENTRY(widget));
    break;
  case UCustom_Pathname:
    sync_value_string(GTK_ENTRY(widget));
    break;
  case UCustom_Choice:
    sync_value_choice(GTK_DROP_DOWN(widget));
    break;
  case UCustom_OrderedList:
    sync_value_olist(GTK_ENTRY(widget));
    break;
  case UCustom_Key:
    sync_value_key(GTK_ENTRY(widget));
    break;
  case UCustom_Table:
    sync_value_table(GTK_LABEL(widget));
    break;
  default:
    break;
  }
  return;
}
