/*
 *  Copyright (c) 2003,2004 Masahito Omote <omote@utyuuzin.net>
 *                2005-2026 uim Project https://github.com/uim/uim
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of authors nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>

#include <gtk/gtk.h>
#include <stdio.h>

#include "cclass-dialog-gtk4.h"
#include "canna-cclass.h"
#include "util.h"

#include "gettext.h"

/*
 * CclassItem: one row of the part-of-speech list (GListModel items
 * must be GObjects).
 */
#define CCLASS_ITEM_TYPE (cclass_item_get_type())
G_DECLARE_FINAL_TYPE(CclassItem, cclass_item, CCLASS, ITEM, GObject)

struct _CclassItem
{
  GObject parent_instance;

  gint id; /* index into the category_code table */
  gchar *pos;
  gchar *example;
};

G_DEFINE_FINAL_TYPE(CclassItem, cclass_item, G_TYPE_OBJECT)

static void
cclass_item_finalize(GObject *object)
{
  CclassItem *item = CCLASS_ITEM(object);

  g_free(item->pos);
  g_free(item->example);

  G_OBJECT_CLASS(cclass_item_parent_class)->finalize(object);
}

static void
cclass_item_class_init(CclassItemClass *klass)
{
  G_OBJECT_CLASS(klass)->finalize = cclass_item_finalize;
}

static void
cclass_item_init(CclassItem *item)
{
}

typedef enum {
  CCLASS_COLUMN_POS,
  CCLASS_COLUMN_EXAMPLE
} CclassColumn;

typedef struct
{
  GtkSingleSelection *selection;
  category_code *code;
  CclassDialogFunc callback;
  gpointer user_data;
  gboolean accepted;
} CclassDialogState;

static void
cclass_cell_setup_cb(GtkSignalListItemFactory *factory,
                     GObject *object,
                     gpointer data)
{
  GtkWidget *label = gtk_label_new(NULL);

  gtk_label_set_xalign(GTK_LABEL(label), 0.0);
  gtk_column_view_cell_set_child(GTK_COLUMN_VIEW_CELL(object), label);
}

static void
cclass_cell_bind_cb(GtkSignalListItemFactory *factory,
                    GObject *object,
                    gpointer data)
{
  GtkColumnViewCell *cell = GTK_COLUMN_VIEW_CELL(object);
  CclassItem *item = gtk_column_view_cell_get_item(cell);
  GtkWidget *label = gtk_column_view_cell_get_child(cell);

  if (GPOINTER_TO_INT(data) == CCLASS_COLUMN_POS)
    gtk_label_set_text(GTK_LABEL(label), item->pos);
  else
    gtk_label_set_text(GTK_LABEL(label), item->example);
}

static void
cclass_append_column(GtkColumnView *view,
                     const gchar *title,
                     CclassColumn column_id)
{
  GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
  GtkColumnViewColumn *column;

  g_signal_connect(factory,
                   "setup",
                   G_CALLBACK(cclass_cell_setup_cb),
                   GINT_TO_POINTER(column_id));
  g_signal_connect(factory,
                   "bind",
                   G_CALLBACK(cclass_cell_bind_cb),
                   GINT_TO_POINTER(column_id));

  column = gtk_column_view_column_new(title, factory);
  gtk_column_view_column_set_resizable(column, TRUE);
  gtk_column_view_append_column(view, column);
  g_object_unref(column);
}

/* Every way of closing the dialog (OK, Cancel, Escape, the window
 * manager's close button) ends up here, so the callback runs exactly
 * once, with NULL unless OK was pressed with a row selected -- the
 * same outcomes as the old GtkDialog "response" handler. */
static gboolean
cclass_dialog_close_request_cb(GtkWindow *dialog, CclassDialogState *state)
{
  gchar *result = NULL;

  if (state->accepted) {
    CclassItem *item = gtk_single_selection_get_selected_item(state->selection);

    if (item)
      result = g_strdup(state->code[item->id].code);
  }

  state->callback(result, state->user_data);

  /* let the default handler destroy the window */
  return FALSE;
}

static void
cclass_dialog_ok_clicked_cb(GtkButton *button, GtkWindow *dialog)
{
  CclassDialogState *state = g_object_get_data(G_OBJECT(dialog), "state");

  state->accepted = TRUE;
  gtk_window_close(dialog);
}

static void
cclass_dialog_cancel_clicked_cb(GtkButton *button, GtkWindow *dialog)
{
  gtk_window_close(dialog);
}

static void
cclass_dialog_state_free(gpointer data)
{
  CclassDialogState *state = data;

  g_object_unref(state->selection);
  g_free(state);
}

void
cclass_dialog(GtkWindow *parent,
              gint pos_type,
              gint system,
              CclassDialogFunc callback,
              gpointer user_data)
{
  GtkWidget *dialog;
  GtkWidget *vbox, *button_box, *button;
  GtkWidget *scrollwin_pos;
  GtkWidget *columnview_pos;
  GtkEventController *shortcuts;
  GListStore *store;
  category_code *code = NULL;
  gint narrow_size = 0, i;
  CclassDialogState *state;

  /* GtkDialog is deprecated since GTK 4.10: build the equivalent
   * modal, transient, destroy-with-parent window by hand */
  dialog = gtk_window_new();
  gtk_window_set_title(GTK_WINDOW(dialog), _("Part of Speech"));
  gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
  gtk_widget_set_size_request(dialog, 400, 350);

  /* Escape closes the window, as it cancelled a GtkDialog */
  shortcuts = gtk_shortcut_controller_new();
  gtk_shortcut_controller_add_shortcut(
    GTK_SHORTCUT_CONTROLLER(shortcuts),
    gtk_shortcut_new(gtk_keyval_trigger_new(GDK_KEY_Escape, 0),
                     gtk_named_action_new("window.close")));
  gtk_widget_add_controller(dialog, shortcuts);

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child(GTK_WINDOW(dialog), vbox);

  scrollwin_pos = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin_pos),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW(scrollwin_pos), TRUE);
  gtk_widget_set_vexpand(scrollwin_pos, TRUE);
  gtk_widget_set_margin_top(scrollwin_pos, 10);
  gtk_widget_set_margin_bottom(scrollwin_pos, 10);
  gtk_widget_set_margin_start(scrollwin_pos, 10);
  gtk_widget_set_margin_end(scrollwin_pos, 10);
  gtk_box_append(GTK_BOX(vbox), scrollwin_pos);

  button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_halign(button_box, GTK_ALIGN_END);
  gtk_widget_set_margin_bottom(button_box, 10);
  gtk_widget_set_margin_start(button_box, 10);
  gtk_widget_set_margin_end(button_box, 10);
  gtk_box_append(GTK_BOX(vbox), button_box);

  button = gtk_button_new_with_mnemonic(_("_Cancel"));
  g_signal_connect(button,
                   "clicked",
                   G_CALLBACK(cclass_dialog_cancel_clicked_cb),
                   dialog);
  gtk_box_append(GTK_BOX(button_box), button);

  button = gtk_button_new_with_mnemonic(_("_OK"));
  g_signal_connect(button,
                   "clicked",
                   G_CALLBACK(cclass_dialog_ok_clicked_cb),
                   dialog);
  gtk_box_append(GTK_BOX(button_box), button);

  switch (pos_type) {
  case POS_SUBSTANTIVE:
    narrow_size = nr_substantive_code;
    code = substantive_code;
    break;
  case POS_ADVERB:
    narrow_size = nr_adverb_code;
    code = adverb_code;
    break;
  case POS_VERB:
    narrow_size = nr_verb_code;
    code = verb_code;
    break;
  case POS_ADJECTIVE:
    narrow_size = nr_adjective_code;
    code = adjective_code;
    break;
  case POS_ETC:
    narrow_size = nr_etc_code;
    code = etc_code;
    break;
  }

  store = g_list_store_new(CCLASS_ITEM_TYPE);
  for (i = 0; i < narrow_size; i++) {
    if (code[i].type & system) {
      CclassItem *item = g_object_new(CCLASS_ITEM_TYPE, NULL);

      item->id = i;
      item->pos = eucjp_to_utf8(code[i].desc);
      item->example = eucjp_to_utf8(code[i].example);
      g_list_store_append(store, item);
      g_object_unref(item);
    }
  }

  state = g_new0(CclassDialogState, 1);
  /* nothing is selected initially, as with GtkTreeSelection */
  state->selection = gtk_single_selection_new(G_LIST_MODEL(store));
  gtk_single_selection_set_autoselect(state->selection, FALSE);
  gtk_single_selection_set_can_unselect(state->selection, TRUE);
  state->code = code;
  state->callback = callback;
  state->user_data = user_data;
  g_object_set_data_full(G_OBJECT(dialog),
                         "state",
                         state,
                         cclass_dialog_state_free);

  columnview_pos =
    gtk_column_view_new(GTK_SELECTION_MODEL(g_object_ref(state->selection)));
  gtk_column_view_set_reorderable(GTK_COLUMN_VIEW(columnview_pos), FALSE);
  cclass_append_column(GTK_COLUMN_VIEW(columnview_pos),
                       _("Part of Speech"),
                       CCLASS_COLUMN_POS);
  cclass_append_column(GTK_COLUMN_VIEW(columnview_pos),
                       _("Example"),
                       CCLASS_COLUMN_EXAMPLE);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrollwin_pos),
                                columnview_pos);

  g_signal_connect(dialog,
                   "close-request",
                   G_CALLBACK(cclass_dialog_close_request_cb),
                   state);
  gtk_window_present(GTK_WINDOW(dialog));
}
