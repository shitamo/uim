/*

  Copyright (c) 2026 uim Project https://github.com/uim/uim

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

*/

/*
 * Widget tests for uim-dict-gtk4: WordListView, WordWindow and
 * WordListWindow.
 *
 * They need a display and are meant to run under xvfb-run (see
 * meson.build).  When no display can be opened the whole program is
 * reported as skipped.
 *
 * WordListView and WordWindow are driven by an in-memory fake backend
 * (test-fake-dict.c).  WordListWindow always opens a real backend through
 * uim_dict_open(), so its tests use Anthy with a private dictionary in a
 * temporary home directory.
 */

#include <config.h>

#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>

#include "canna-cclass.h"
#include "dict.h"
#include "util.h"
#include "word.h"
#include "word-list-view-gtk4.h"
#include "word-list-win-gtk4.h"
#include "word-win-gtk4.h"

#include "test-dict-common.h"
#include "test-fake-dict.h"

/* Column layout of WordListView's GtkListStore (private enum in
 * word-list-view-gtk4.c). */
enum {
  COL_WORD_TYPE = 0,
  COL_PHON,
  COL_DESC,
  COL_CCLASS_CODE,
  COL_FREQ,
  COL_OKURI,
  COL_EDITABLE,
  COL_WORD
};

/* Response ids of the buttons GtkAlertDialog puts on its message dialog
 * (the button index; see act_remove_word_cb()). */
#define REMOVE_CONFIRM_CANCEL 0
#define REMOVE_CONFIRM_REMOVE 1

/* Defined in main-gtk4.c, which is not linked into the tests. */
void
dict_window_destroy_cb(GtkWidget *widget, gpointer data)
{
}

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */

static void
spin(void)
{
  while (g_main_context_iteration(NULL, FALSE))
    ;
}

static gboolean
timeout_cb(gpointer data)
{
  *(gboolean *)data = TRUE;
  return G_SOURCE_REMOVE;
}

/* Iterate the main loop until @cond returns TRUE (or 5 seconds pass). */
static gboolean
wait_for(gboolean (*cond)(gpointer), gpointer data)
{
  gboolean timed_out = FALSE;
  guint id = g_timeout_add(5000, timeout_cb, &timed_out);

  while (!cond(data) && !timed_out)
    g_main_context_iteration(NULL, TRUE);

  if (!timed_out)
    g_source_remove(id);
  return cond(data);
}

static gboolean
is_null_cond(gpointer data)
{
  return *(gpointer *)data == NULL;
}

/* Find a toplevel window of @type; for message dialogs, @text (if not
 * NULL) must match the primary text. */
static GtkWindow *
find_toplevel(GType type, const gchar *text)
{
  GListModel *toplevels = gtk_window_get_toplevels();
  guint i, n = g_list_model_get_n_items(toplevels);
  GtkWindow *found = NULL;

  for (i = 0; i < n && !found; i++) {
    GtkWindow *window = g_list_model_get_item(toplevels, i);

    if (G_TYPE_CHECK_INSTANCE_TYPE(window, type)) {
      if (text && GTK_IS_MESSAGE_DIALOG(window)) {
        gchar *t = NULL;

        g_object_get(window, "text", &t, NULL);
        if (!g_strcmp0(t, text))
          found = window;
        g_free(t);
      } else {
        found = window;
      }
    }
    g_object_unref(window);
  }
  return found;
}

typedef struct
{
  GType type;
  const gchar *text;
  GtkWindow *result;
} find_data;

static gboolean
toplevel_cond(gpointer data)
{
  find_data *fd = data;

  fd->result = find_toplevel(fd->type, fd->text);
  return fd->result != NULL;
}

static GtkWindow *
wait_for_toplevel(GType type, const gchar *text)
{
  find_data fd = {type, text, NULL};

  wait_for(toplevel_cond, &fd);
  return fd.result;
}

/* Wait for the message dialog showing @text, check it and close it. */
static void
close_message(const gchar *text)
{
  GtkWindow *dialog = wait_for_toplevel(GTK_TYPE_MESSAGE_DIALOG, text);

  g_assert_nonnull(dialog);
  gtk_window_destroy(dialog);
  spin();
}

/* Destroy every toplevel left over by a test. */
static void
destroy_all_toplevels(void)
{
  GListModel *toplevels = gtk_window_get_toplevels();

  spin();
  while (g_list_model_get_n_items(toplevels) > 0) {
    GtkWindow *window = g_list_model_get_item(toplevels, 0);

    gtk_window_destroy(window);
    g_object_unref(window);
    spin();
  }
}

static gint
model_n_rows(WordListView *view)
{
  return gtk_tree_model_iter_n_children(view->model, NULL);
}

/* Row index of the entry whose phonetic and literal columns match, or -1. */
static gint
model_find_row(WordListView *view, const gchar *phon, const gchar *desc)
{
  GtkTreeIter iter;
  gint row = 0;

  if (!gtk_tree_model_get_iter_first(view->model, &iter))
    return -1;
  do {
    gchar *p, *d;
    gboolean match;

    gtk_tree_model_get(view->model, &iter, COL_PHON, &p, COL_DESC, &d, -1);
    match = !g_strcmp0(p, phon) && !g_strcmp0(d, desc);
    g_free(p);
    g_free(d);
    if (match)
      return row;
    row++;
  } while (gtk_tree_model_iter_next(view->model, &iter));

  return -1;
}

static void
select_row(WordListView *view, gint row)
{
  GtkTreePath *path = gtk_tree_path_new_from_indices(row, -1);

  gtk_tree_selection_select_path(view->selection, path);
  gtk_tree_path_free(path);
}

static gchar *
cclass_desc_utf8(const char *code)
{
  const char *desc = find_desc_from_code(code);

  g_assert_nonnull(desc);
  return eucjp_to_utf8(desc);
}

/* A fake dictionary holding three words in EUC-JP, like the EUC-JP Anthy
 * backend; WordListView must show them converted to UTF-8. */
static uim_dict *
new_eucjp_dict(void)
{
  uim_dict *dict = uim_dict_ref(fake_dict_new("Test dictionary", "EUC-JP"));
  struct
  {
    const char *phon, *desc, *code;
    int freq;
  } words[] = {
    {"あいう", "愛宇", "#JN", 50},
    {"てすと", "手素人", "#T35", 100},
    {"おく", "置く", "#K5", 300},
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS(words); i++) {
    gchar *phon = test_dict_eucjp(words[i].phon);
    gchar *desc = test_dict_eucjp(words[i].desc);

    fake_dict_store(dict,
                    phon,
                    desc,
                    find_desc_from_code(words[i].code),
                    words[i].code,
                    words[i].freq);
    g_free(phon);
    g_free(desc);
  }
  uim_dict_refresh(dict);

  return dict;
}

static void
fixture_teardown(void)
{
  destroy_all_toplevels();
  fake_dict_reset_stats();
}

/* ------------------------------------------------------------------ */
/* WordListView                                                        */
/* ------------------------------------------------------------------ */

static void
test_view_defaults(void)
{
  GtkWidget *widget = g_object_ref_sink(word_list_view_new());
  WordListView *view = WORD_LIST_VIEW(widget);

  g_assert_true(IS_WORD_LIST_VIEW(widget));
  g_assert_nonnull(view->view);
  g_assert_nonnull(view->model);
  g_assert_null(view->dict);
  g_assert_cmpint(model_n_rows(view), ==, 0);

  g_assert_false(word_list_view_get_visible_cclass_code_column(view));
  g_assert_false(word_list_view_get_visible_freq_column(view));
  g_assert_false(word_list_view_get_visible_okuri_column(view));
  g_assert_false(word_list_view_get_editable(view));
  g_assert_cmpint(word_list_view_get_selection_mode(view),
                  ==,
                  GTK_SELECTION_SINGLE);

  g_assert_false(gtk_tree_view_column_get_visible(view->word_type_column));
  g_assert_true(gtk_tree_view_column_get_visible(view->phon_column));
  g_assert_true(gtk_tree_view_column_get_visible(view->desc_column));
  g_assert_false(gtk_tree_view_column_get_visible(view->cclass_code_column));
  g_assert_false(gtk_tree_view_column_get_visible(view->freq_column));
  g_assert_false(gtk_tree_view_column_get_visible(view->okuri_column));

  word_list_view_set_visible_cclass_code_column(view, TRUE);
  word_list_view_set_visible_freq_column(view, TRUE);
  word_list_view_set_visible_okuri_column(view, TRUE);
  g_assert_true(gtk_tree_view_column_get_visible(view->cclass_code_column));
  g_assert_true(gtk_tree_view_column_get_visible(view->freq_column));
  g_assert_true(gtk_tree_view_column_get_visible(view->okuri_column));

  word_list_view_set_visible_freq_column(view, FALSE);
  g_assert_false(gtk_tree_view_column_get_visible(view->freq_column));

  word_list_view_set_selection_mode(view, GTK_SELECTION_MULTIPLE);
  g_assert_cmpint(gtk_tree_selection_get_mode(view->selection),
                  ==,
                  GTK_SELECTION_MULTIPLE);

  g_object_unref(widget);
}

static void
test_view_properties(void)
{
  GtkWidget *widget;
  WordListView *view;
  gboolean cclass_show, freq_show, okuri_show, editable;
  GtkSelectionMode mode;

  widget = word_list_view_new_with_attributes("cclass_code_show",
                                              TRUE,
                                              "freq_show",
                                              TRUE,
                                              "editable",
                                              TRUE,
                                              "selection_mode",
                                              GTK_SELECTION_MULTIPLE,
                                              NULL);
  g_object_ref_sink(widget);
  view = WORD_LIST_VIEW(widget);

  g_object_get(widget,
               "cclass_code_show",
               &cclass_show,
               "freq_show",
               &freq_show,
               "okuri_show",
               &okuri_show,
               "editable",
               &editable,
               "selection_mode",
               &mode,
               NULL);
  g_assert_true(cclass_show);
  g_assert_true(freq_show);
  g_assert_false(okuri_show);
  g_assert_true(editable);
  g_assert_cmpint(mode, ==, GTK_SELECTION_MULTIPLE);

  g_assert_true(gtk_tree_view_column_get_visible(view->cclass_code_column));
  g_assert_true(gtk_tree_view_column_get_visible(view->freq_column));
  g_assert_false(gtk_tree_view_column_get_visible(view->okuri_column));
  g_assert_cmpint(gtk_tree_selection_get_mode(view->selection),
                  ==,
                  GTK_SELECTION_MULTIPLE);

  g_object_set(widget, "okuri_show", TRUE, "freq_show", FALSE, NULL);
  g_assert_true(word_list_view_get_visible_okuri_column(view));
  g_assert_false(word_list_view_get_visible_freq_column(view));

  g_object_unref(widget);
}

static void
test_view_show_words(void)
{
  uim_dict *dict = new_eucjp_dict();
  GtkWidget *window, *widget;
  WordListView *view;
  GtkTreeIter iter;
  GList *all, *node;
  uim_word *w;
  gchar *phon, *desc, *cclass, *expected;
  gint type, freq, row;
  gboolean okuri, editable;

  window = gtk_window_new();
  widget = word_list_view_new_with_attributes("cclass_code_show",
                                              TRUE,
                                              "freq_show",
                                              TRUE,
                                              NULL);
  view = WORD_LIST_VIEW(widget);
  gtk_window_set_child(GTK_WINDOW(window), widget);

  word_list_view_set_dict(view, dict);
  g_assert_true(view->dict == dict);
  g_assert_cmpuint(dict->ref_count, ==, 2);

  gtk_window_present(GTK_WINDOW(window));
  spin();
  g_assert_true(gtk_widget_get_mapped(widget));
  g_assert_true(gtk_widget_get_realized(GTK_WIDGET(view->view)));

  /* one row per word, in word list order, converted to UTF-8 */
  g_assert_cmpint(model_n_rows(view), ==, 3);
  row = model_find_row(view, "てすと", "手素人");
  g_assert_cmpint(row, ==, 1);
  g_assert_cmpint(model_find_row(view, "あいう", "愛宇"), ==, 0);
  g_assert_cmpint(model_find_row(view, "おく", "置く"), ==, 2);

  g_assert_true(gtk_tree_model_iter_nth_child(view->model, &iter, NULL, 1));
  gtk_tree_model_get(view->model,
                     &iter,
                     COL_WORD_TYPE,
                     &type,
                     COL_PHON,
                     &phon,
                     COL_DESC,
                     &desc,
                     COL_CCLASS_CODE,
                     &cclass,
                     COL_FREQ,
                     &freq,
                     COL_OKURI,
                     &okuri,
                     COL_EDITABLE,
                     &editable,
                     COL_WORD,
                     &w,
                     -1);
  g_assert_cmpint(type, ==, WORD_TYPE_ANTHY);
  g_assert_cmpstr(phon, ==, "てすと");
  g_assert_cmpstr(desc, ==, "手素人");
  expected = cclass_desc_utf8("#T35");
  g_assert_cmpstr(cclass, ==, expected);
  g_assert_cmpstr(cclass, ==, "名詞(語幹,格助接続)");
  g_assert_cmpint(freq, ==, 100);
  g_assert_false(okuri);
  g_assert_false(editable);
  g_assert_true(w == dict->word_list->next);
  g_free(phon);
  g_free(desc);
  g_free(cclass);
  g_free(expected);

  /* the data list hands back the backend's word objects */
  all = word_list_view_get_all_data_list(view);
  g_assert_cmpuint(g_list_length(all), ==, 3);
  for (node = all, w = dict->word_list; node && w;
       node = node->next, w = w->next)
    g_assert_true(node->data == w);
  g_list_free(all);

  /* nothing is selected yet */
  g_assert_null(word_list_view_get_selected_data_list(view));

  /* destroying the widget drops its reference to the dictionary */
  gtk_window_destroy(GTK_WINDOW(window));
  spin();
  g_assert_cmpuint(dict->ref_count, ==, 1);
  g_assert_cmpint(fake_dict_stats_data.n_close, ==, 0);

  uim_dict_unref(dict);
  g_assert_cmpint(fake_dict_stats_data.n_close, ==, 1);
  fixture_teardown();
}

static void
test_view_selection_and_remove(void)
{
  uim_dict *dict = new_eucjp_dict();
  GtkWidget *widget;
  WordListView *view;
  GList *selected;

  widget =
    g_object_ref_sink(word_list_view_new_with_attributes("selection_mode",
                                                         GTK_SELECTION_MULTIPLE,
                                                         NULL));
  view = WORD_LIST_VIEW(widget);
  word_list_view_set_dict(view, dict);
  g_assert_cmpint(model_n_rows(view), ==, 3);

  select_row(view, 0);
  select_row(view, 2);
  selected = word_list_view_get_selected_data_list(view);
  g_assert_cmpuint(g_list_length(selected), ==, 2);
  g_assert_true(selected->data == dict->word_list);
  g_assert_true(selected->next->data == dict->word_list->next->next);
  g_list_free(selected);

  /* removing the selection only touches the view, not the backend */
  word_list_view_remove_selected_data(view);
  g_assert_cmpint(model_n_rows(view), ==, 1);
  g_assert_cmpint(model_find_row(view, "てすと", "手素人"), ==, 0);
  g_assert_cmpint(gtk_tree_selection_count_selected_rows(view->selection),
                  ==,
                  0);
  g_assert_null(word_list_view_get_selected_data_list(view));
  g_assert_cmpuint(fake_dict_n_stored(dict), ==, 3);
  g_assert_cmpint(fake_dict_stats_data.n_remove, ==, 0);

  /* removing with nothing selected is a no-op */
  word_list_view_remove_selected_data(view);
  g_assert_cmpint(model_n_rows(view), ==, 1);

  /* refreshing reloads everything from the backend */
  word_list_view_refresh(view);
  g_assert_cmpint(model_n_rows(view), ==, 3);

  word_list_view_clear(view);
  g_assert_cmpint(model_n_rows(view), ==, 0);
  g_assert_true(view->dict == dict);

  g_object_unref(widget);
  uim_dict_unref(dict);
  g_assert_cmpint(fake_dict_stats_data.n_close, ==, 1);
  fixture_teardown();
}

static void
test_view_refresh_reflects_backend(void)
{
  uim_dict *dict = uim_dict_ref(fake_dict_new("Test dictionary", "UTF-8"));
  GtkWidget *widget;
  WordListView *view;
  uim_word *word = NULL;

  widget = g_object_ref_sink(word_list_view_new());
  view = WORD_LIST_VIEW(widget);
  word_list_view_set_dict(view, dict);
  g_assert_cmpint(model_n_rows(view), ==, 0);

  word_append(&word,
              WORD_TYPE_ANTHY,
              "UTF-8",
              "かんじ",
              "漢字",
              "名詞",
              "#T35",
              10,
              0,
              NULL);
  g_assert_cmpint(uim_dict_add_word(dict, word), ==, 1);

  /* not shown until the view is refreshed */
  g_assert_cmpint(model_n_rows(view), ==, 0);
  word_list_view_refresh(view);
  g_assert_cmpint(model_n_rows(view), ==, 1);
  g_assert_cmpint(model_find_row(view, "かんじ", "漢字"), ==, 0);

  g_assert_cmpint(uim_dict_remove_word(dict, word), ==, 1);
  word_list_view_refresh(view);
  g_assert_cmpint(model_n_rows(view), ==, 0);
  word_free_list(word);

  g_object_unref(widget);
  uim_dict_unref(dict);
  fixture_teardown();
}

static void
test_view_editable(void)
{
  uim_dict *dict = new_eucjp_dict();
  GtkWidget *widget;
  WordListView *view;
  GtkTreeIter iter;
  gboolean editable;

  widget = g_object_ref_sink(word_list_view_new());
  view = WORD_LIST_VIEW(widget);
  word_list_view_set_dict(view, dict);

  word_list_view_set_editable(view, TRUE);
  g_assert_true(word_list_view_get_editable(view));
  g_assert_true(gtk_tree_model_get_iter_first(view->model, &iter));
  do {
    gtk_tree_model_get(view->model, &iter, COL_EDITABLE, &editable, -1);
    g_assert_true(editable);
  } while (gtk_tree_model_iter_next(view->model, &iter));

  /* rows added later inherit the flag */
  word_list_view_refresh(view);
  g_assert_true(gtk_tree_model_get_iter_first(view->model, &iter));
  gtk_tree_model_get(view->model, &iter, COL_EDITABLE, &editable, -1);
  g_assert_true(editable);

  word_list_view_set_editable(view, FALSE);
  gtk_tree_model_get(view->model, &iter, COL_EDITABLE, &editable, -1);
  g_assert_false(editable);

  g_object_unref(widget);
  uim_dict_unref(dict);
  fixture_teardown();
}

/* ------------------------------------------------------------------ */
/* WordWindow                                                          */
/* ------------------------------------------------------------------ */

static const gchar *
entry_text(GtkWidget *entry)
{
  return gtk_editable_get_text(GTK_EDITABLE(entry));
}

static void
word_added_cb(WordWindow *window, gpointer data)
{
  (*(int *)data)++;
}

static void
test_word_window_defaults(void)
{
  uim_dict *dict = uim_dict_ref(fake_dict_new("Test dictionary", "UTF-8"));
  GtkWidget *widget;
  WordWindow *window;
  struct
  {
    int pos;
    const char *code;
  } defaults[] = {
    {POS_SUBSTANTIVE, "#T00"},
    {POS_VERB, "#K5"},
    {POS_ADJECTIVE, "#KY"},
    {POS_ADVERB, "#T07"},
    {POS_ETC, "#KJ"},
    {POS_SUBSTANTIVE, "#T00"},
  };
  guint i;

  widget = word_window_new(WORD_WINDOW_MODE_ADD, dict);
  window = WORD_WINDOW(widget);
  g_assert_true(IS_WORD_WINDOW(widget));
  g_assert_true(window->dict == dict);
  g_assert_cmpuint(dict->ref_count, ==, 2);

  gtk_window_present(GTK_WINDOW(widget));
  spin();
  g_assert_true(gtk_widget_get_mapped(widget));

  g_assert_cmpstr(gtk_window_get_title(GTK_WINDOW(widget)),
                  ==,
                  "Add a word (Test dictionary)");
  g_assert_cmpstr(entry_text(window->phon), ==, "");
  g_assert_cmpstr(entry_text(window->desc), ==, "");
  g_assert_cmpint(
    gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(window->freq)),
    ==,
    1);
  g_assert_true(gtk_widget_get_sensitive(window->freq));
  g_assert_true(gtk_widget_get_visible(window->continuance));
  g_assert_false(word_window_is_continuance_mode(window));
  g_assert_false(gtk_editable_get_editable(GTK_EDITABLE(window->cclass_code)));

  /* choosing a broad part of speech resets the narrow one to the first
   * entry of that category */
  for (i = 0; i < G_N_ELEMENTS(defaults); i++) {
    gchar *expected = cclass_desc_utf8(defaults[i].code);

    gtk_combo_box_set_active(GTK_COMBO_BOX(window->combobox_pos_broad),
                             defaults[i].pos);
    g_assert_cmpstr(entry_text(window->cclass_code), ==, expected);
    g_free(expected);
  }

  /* Cancel releases the dictionary and destroys the window */
  g_object_add_weak_pointer(G_OBJECT(widget), (gpointer *)&widget);
  gtk_dialog_response(GTK_DIALOG(window), GTK_RESPONSE_CANCEL);
  g_assert_true(wait_for(is_null_cond, &widget));
  g_assert_cmpuint(dict->ref_count, ==, 1);
  g_assert_cmpint(fake_dict_stats_data.n_add, ==, 0);

  uim_dict_unref(dict);
  fixture_teardown();
}

static void
test_word_window_edit_mode(void)
{
  uim_dict *dict = uim_dict_ref(fake_dict_new("Test dictionary", "UTF-8"));
  GtkWidget *widget;
  WordWindow *window;
  uim_word *word = NULL;
  gchar *cclass = cclass_desc_utf8("#K5r");

  word_append(&word,
              WORD_TYPE_ANTHY,
              "UTF-8",
              "かく",
              "書く",
              cclass,
              "#K5r",
              42,
              0,
              NULL);

  widget = word_window_new(WORD_WINDOW_MODE_EDIT, dict);
  window = WORD_WINDOW(widget);
  g_assert_cmpstr(gtk_window_get_title(GTK_WINDOW(widget)),
                  ==,
                  "Edit the word (Test dictionary)");
  g_assert_false(gtk_widget_get_visible(window->continuance));

  word_window_set_word(window, word);
  g_assert_cmpstr(entry_text(window->phon), ==, "かく");
  g_assert_cmpstr(entry_text(window->desc), ==, "書く");
  g_assert_cmpint(
    gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(window->freq)),
    ==,
    42);
  g_assert_cmpint(
    gtk_combo_box_get_active(GTK_COMBO_BOX(window->combobox_pos_broad)),
    ==,
    POS_VERB);
  /* the narrow part of speech is the word's, not the category default */
  g_assert_cmpstr(entry_text(window->cclass_code), ==, cclass);
  g_assert_cmpstr(entry_text(window->cclass_code), ==, "か行5段:連名");

  /* Clear empties the fields and goes back to the first category */
  gtk_dialog_response(GTK_DIALOG(window), WORD_WINDOW_RESPONSE_CLEAR);
  g_assert_cmpstr(entry_text(window->phon), ==, "");
  g_assert_cmpstr(entry_text(window->desc), ==, "");
  g_assert_cmpint(
    gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(window->freq)),
    ==,
    1);
  g_assert_cmpint(
    gtk_combo_box_get_active(GTK_COMBO_BOX(window->combobox_pos_broad)),
    ==,
    POS_SUBSTANTIVE);

  gtk_dialog_response(GTK_DIALOG(window), GTK_RESPONSE_CANCEL);
  spin();

  word_free_list(word);
  g_free(cclass);
  uim_dict_unref(dict);
  fixture_teardown();
}

static void
test_word_window_canna_freq(void)
{
  uim_dict *dict =
    uim_dict_ref(fake_dict_new("Canna private dictionary", "EUC-JP"));
  GtkWidget *widget = word_window_new(WORD_WINDOW_MODE_ADD, dict);

  /* Canna has no word frequency */
  g_assert_false(gtk_widget_get_sensitive(WORD_WINDOW(widget)->freq));

  gtk_dialog_response(GTK_DIALOG(widget), GTK_RESPONSE_CANCEL);
  spin();
  uim_dict_unref(dict);
  fixture_teardown();
}

static void
test_word_window_close_button(void)
{
  uim_dict *dict = uim_dict_ref(fake_dict_new("Test dictionary", "UTF-8"));
  GtkWidget *widget;

  widget = word_window_new(WORD_WINDOW_MODE_ADD, dict);
  gtk_window_present(GTK_WINDOW(widget));
  spin();
  g_assert_cmpuint(dict->ref_count, ==, 2);

  /* the window manager's close button: GtkDialog emits
   * GTK_RESPONSE_DELETE_EVENT and destroys the window by itself */
  g_object_add_weak_pointer(G_OBJECT(widget), (gpointer *)&widget);
  gtk_window_close(GTK_WINDOW(widget));
  g_assert_true(wait_for(is_null_cond, &widget));

  /* the window's reference to the dictionary is released */
  g_assert_cmpuint(dict->ref_count, ==, 1);
  g_assert_cmpint(fake_dict_stats_data.n_close, ==, 0);

  uim_dict_unref(dict);
  g_assert_cmpint(fake_dict_stats_data.n_close, ==, 1);
  fixture_teardown();
}

static void
fill_word_window(
  WordWindow *window, const gchar *phon, const gchar *desc, int pos, int freq)
{
  gtk_editable_set_text(GTK_EDITABLE(window->phon), phon);
  gtk_editable_set_text(GTK_EDITABLE(window->desc), desc);
  gtk_combo_box_set_active(GTK_COMBO_BOX(window->combobox_pos_broad), pos);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(window->freq), freq);
}

static void
test_word_window_add(void)
{
  uim_dict *dict = uim_dict_ref(fake_dict_new("Test dictionary", "UTF-8"));
  GtkWidget *widget;
  WordWindow *window;
  uim_word *added, *stored;
  int n_added = 0;

  widget = word_window_new(WORD_WINDOW_MODE_ADD, dict);
  window = WORD_WINDOW(widget);
  g_signal_connect(widget, "word-added", G_CALLBACK(word_added_cb), &n_added);
  gtk_window_present(GTK_WINDOW(widget));
  spin();

  fill_word_window(window, "おく", "置く", POS_VERB, 300);
  g_object_add_weak_pointer(G_OBJECT(widget), (gpointer *)&widget);
  gtk_dialog_response(GTK_DIALOG(window), WORD_WINDOW_RESPONSE_ADD);

  /* the backend got the word ... */
  g_assert_cmpint(fake_dict_stats_data.n_add, ==, 1);
  added = fake_dict_stats_data.last_added;
  g_assert_nonnull(added);
  g_assert_cmpint(added->type, ==, WORD_TYPE_ANTHY);
  g_assert_cmpstr(added->charset, ==, "UTF-8");
  g_assert_cmpstr(added->phon, ==, "おく");
  g_assert_cmpstr(added->desc, ==, "置く");
  /* the narrow part of speech is mapped back to its code; the
   * description is handed over in EUC-JP (the cclass tables' charset) */
  g_assert_cmpstr(added->cclass_native, ==, "#K5");
  g_assert_cmpstr(added->cclass_code, ==, find_desc_from_code("#K5"));
  g_assert_cmpint(added->freq, ==, 300);
  stored = fake_dict_find_stored(dict, "おく", "置く");
  g_assert_nonnull(stored);

  /* ... the window announced it and reported success ... */
  g_assert_cmpint(n_added, ==, 1);
  close_message("Word registration succeeded.");

  /* ... and, not being in continuance mode, went away */
  g_assert_true(wait_for(is_null_cond, &widget));
  g_assert_cmpuint(dict->ref_count, ==, 1);

  uim_dict_unref(dict);
  fixture_teardown();
}

static void
test_word_window_add_then_cancel(void)
{
  uim_dict *dict = uim_dict_ref(fake_dict_new("Test dictionary", "UTF-8"));
  GtkWidget *widget;
  WordWindow *window;

  widget = word_window_new(WORD_WINDOW_MODE_ADD, dict);
  window = WORD_WINDOW(widget);
  gtk_window_present(GTK_WINDOW(widget));
  spin();
  g_object_add_weak_pointer(G_OBJECT(widget), (gpointer *)&widget);

  /* Add schedules the window's destruction; a second closing response
   * before that happens must not release the dictionary twice */
  fill_word_window(window, "おく", "置く", POS_VERB, 1);
  gtk_dialog_response(GTK_DIALOG(window), WORD_WINDOW_RESPONSE_ADD);
  gtk_dialog_response(GTK_DIALOG(window), GTK_RESPONSE_CANCEL);
  close_message("Word registration succeeded.");
  g_assert_true(wait_for(is_null_cond, &widget));

  g_assert_cmpuint(dict->ref_count, ==, 1);
  g_assert_cmpint(fake_dict_stats_data.n_close, ==, 0);
  g_assert_cmpuint(fake_dict_n_stored(dict), ==, 1);

  uim_dict_unref(dict);
  fixture_teardown();
}

static void
test_word_window_add_continuance(void)
{
  uim_dict *dict = uim_dict_ref(fake_dict_new("Test dictionary", "UTF-8"));
  GtkWidget *widget;
  WordWindow *window;
  int n_added = 0;

  widget = word_window_new(WORD_WINDOW_MODE_ADD, dict);
  window = WORD_WINDOW(widget);
  g_signal_connect(widget, "word-added", G_CALLBACK(word_added_cb), &n_added);
  gtk_check_button_set_active(GTK_CHECK_BUTTON(window->continuance), TRUE);
  g_assert_true(word_window_is_continuance_mode(window));
  gtk_window_present(GTK_WINDOW(widget));
  spin();

  g_object_add_weak_pointer(G_OBJECT(widget), (gpointer *)&widget);

  fill_word_window(window, "あ", "亜", POS_SUBSTANTIVE, 5);
  gtk_dialog_response(GTK_DIALOG(window), WORD_WINDOW_RESPONSE_ADD);
  close_message("Word registration succeeded.");

  /* the window stays, cleared for the next word */
  g_assert_nonnull(widget);
  g_assert_cmpstr(entry_text(window->phon), ==, "");
  g_assert_cmpstr(entry_text(window->desc), ==, "");
  g_assert_cmpuint(dict->ref_count, ==, 2);

  fill_word_window(window, "い", "伊", POS_ADVERB, 6);
  gtk_dialog_response(GTK_DIALOG(window), WORD_WINDOW_RESPONSE_ADD);
  close_message("Word registration succeeded.");
  g_assert_nonnull(widget);

  g_assert_cmpint(n_added, ==, 2);
  g_assert_cmpuint(fake_dict_n_stored(dict), ==, 2);
  g_assert_nonnull(fake_dict_find_stored(dict, "あ", "亜"));
  g_assert_cmpstr(fake_dict_find_stored(dict, "い", "伊")->cclass_native,
                  ==,
                  "#T07");

  gtk_dialog_response(GTK_DIALOG(window), GTK_RESPONSE_CANCEL);
  g_assert_true(wait_for(is_null_cond, &widget));
  g_assert_cmpuint(dict->ref_count, ==, 1);

  uim_dict_unref(dict);
  fixture_teardown();
}

static void
test_word_window_add_validation(void)
{
  uim_dict *dict = uim_dict_ref(fake_dict_new("Test dictionary", "UTF-8"));
  GtkWidget *widget;
  WordWindow *window;
  int n_added = 0;

  widget = word_window_new(WORD_WINDOW_MODE_ADD, dict);
  window = WORD_WINDOW(widget);
  g_signal_connect(widget, "word-added", G_CALLBACK(word_added_cb), &n_added);
  gtk_window_present(GTK_WINDOW(widget));
  spin();

  /* no phonetic */
  fill_word_window(window, "", "空", POS_SUBSTANTIVE, 1);
  gtk_dialog_response(GTK_DIALOG(window), WORD_WINDOW_RESPONSE_ADD);
  close_message("Phonetic is empty!");

  /* no literal */
  fill_word_window(window, "から", "", POS_SUBSTANTIVE, 1);
  gtk_dialog_response(GTK_DIALOG(window), WORD_WINDOW_RESPONSE_ADD);
  close_message("Literal is empty!");

  /* nothing reached the backend and the input is kept for correction */
  g_assert_cmpint(fake_dict_stats_data.n_add, ==, 0);
  g_assert_cmpint(n_added, ==, 0);
  g_assert_cmpstr(entry_text(window->phon), ==, "から");
  g_assert_true(find_toplevel(WORD_WINDOW_TYPE, NULL) == GTK_WINDOW(widget));

  gtk_dialog_response(GTK_DIALOG(window), GTK_RESPONSE_CANCEL);
  spin();
  uim_dict_unref(dict);
  fixture_teardown();
}

static void
test_word_window_add_failure(void)
{
  uim_dict *dict = uim_dict_ref(fake_dict_new("Test dictionary", "UTF-8"));
  GtkWidget *widget;
  WordWindow *window;
  int n_added = 0;

  fake_dict_set_fail(dict, 1);
  widget = word_window_new(WORD_WINDOW_MODE_ADD, dict);
  window = WORD_WINDOW(widget);
  g_signal_connect(widget, "word-added", G_CALLBACK(word_added_cb), &n_added);
  gtk_window_present(GTK_WINDOW(widget));
  spin();

  g_object_add_weak_pointer(G_OBJECT(widget), (gpointer *)&widget);
  fill_word_window(window, "おく", "置く", POS_VERB, 1);
  gtk_dialog_response(GTK_DIALOG(window), WORD_WINDOW_RESPONSE_ADD);

  g_assert_cmpint(fake_dict_stats_data.n_add, ==, 1);
  g_assert_cmpint(n_added, ==, 0);
  close_message("Word registration failed.");
  g_assert_true(wait_for(is_null_cond, &widget));
  g_assert_cmpuint(fake_dict_n_stored(dict), ==, 0);

  uim_dict_unref(dict);
  fixture_teardown();
}

/* ------------------------------------------------------------------ */
/* WordListWindow (Anthy, temporary private dictionary)                */
/* ------------------------------------------------------------------ */

#ifdef USE_ANTHY
#  define ANTHY_FIXTURE                                                        \
    "あいう #JN*50 愛宇\n"                                                \
    "てすと #T35*100 手素人\n"

static gboolean
action_enabled(WordListWindow *window, const gchar *name)
{
  return g_action_group_get_action_enabled(G_ACTION_GROUP(window->actions),
                                           name);
}

static void
activate_action(WordListWindow *window, const gchar *name)
{
  g_action_group_activate_action(G_ACTION_GROUP(window->actions), name, NULL);
}

static WordListWindow *
open_word_list_window(void)
{
  GtkWidget *widget;
  WordListWindow *window;
  WordListView *view;

  widget = word_list_window_new(DICT_ENUM_DICTIONARY_TYPE_ANTHY);
  window = WORD_LIST_WINDOW(widget);
  view = WORD_LIST_VIEW(window->word_list);
  g_assert_nonnull(view->dict);
  gtk_window_present(GTK_WINDOW(widget));
  spin();

  return window;
}

static void
test_list_window_show(void)
{
  WordListWindow *window;
  WordListView *view;
  GtkTreeIter iter;
  gchar *cclass;
  gint freq;

  if (!test_dict_anthy_setup(ANTHY_FIXTURE))
    return;

  window = open_word_list_window();
  view = WORD_LIST_VIEW(window->word_list);

  g_assert_true(gtk_widget_get_mapped(GTK_WIDGET(window)));
  g_assert_cmpint(dict_get_dictionary_type(GTK_WIDGET(window)),
                  ==,
                  DICT_ENUM_DICTIONARY_TYPE_ANTHY);
  g_assert_cmpstr(view->dict->identifier, ==, "Anthy private dictionary");
  g_assert_true(word_list_view_get_visible_cclass_code_column(view));
  g_assert_true(word_list_view_get_visible_freq_column(view));

  /* the private dictionary is listed */
  g_assert_cmpint(model_n_rows(view), ==, 2);
  g_assert_cmpint(model_find_row(view, "あいう", "愛宇"), >=, 0);
  g_assert_cmpint(model_find_row(view, "てすと", "手素人"), >=, 0);

  g_assert_true(
    gtk_tree_model_iter_nth_child(view->model,
                                  &iter,
                                  NULL,
                                  model_find_row(view, "てすと", "手素人")));
  gtk_tree_model_get(view->model,
                     &iter,
                     COL_CCLASS_CODE,
                     &cclass,
                     COL_FREQ,
                     &freq,
                     -1);
  g_assert_cmpstr(cclass, ==, "名詞(語幹,格助接続)");
  g_assert_cmpint(freq, ==, 100);
  g_free(cclass);

  /* edit/remove need a selection */
  g_assert_true(action_enabled(window, "add-word"));
  g_assert_false(action_enabled(window, "edit-word"));
  g_assert_false(action_enabled(window, "remove-word"));

  select_row(view, 0);
  g_assert_true(action_enabled(window, "edit-word"));
  g_assert_true(action_enabled(window, "remove-word"));

  gtk_tree_selection_unselect_all(view->selection);
  g_assert_false(action_enabled(window, "remove-word"));

  /* activating remove without a selection does nothing */
  activate_action(window, "remove-word");
  spin();
  g_assert_null(find_toplevel(GTK_TYPE_MESSAGE_DIALOG, NULL));

  fixture_teardown();
}

static void
test_list_window_add_word(void)
{
  WordListWindow *window;
  WordListView *view;
  WordWindow *word_window;
  gchar *contents;

  if (!test_dict_anthy_setup(ANTHY_FIXTURE))
    return;

  window = open_word_list_window();
  view = WORD_LIST_VIEW(window->word_list);

  activate_action(window, "add-word");
  word_window = (WordWindow *)wait_for_toplevel(WORD_WINDOW_TYPE, NULL);
  g_assert_nonnull(word_window);
  g_assert_true(word_window->dict == view->dict);
  g_assert_true(gtk_window_get_transient_for(GTK_WINDOW(word_window)) ==
                GTK_WINDOW(window));
  g_assert_true(gtk_window_get_modal(GTK_WINDOW(word_window)));

  fill_word_window(word_window, "おく", "置く", POS_VERB, 300);
  gtk_dialog_response(GTK_DIALOG(word_window), WORD_WINDOW_RESPONSE_ADD);
  close_message("Word registration succeeded.");

  /* the list was refreshed from the dictionary */
  g_assert_cmpint(model_n_rows(view), ==, 3);
  g_assert_cmpint(model_find_row(view, "おく", "置く"), >=, 0);

  contents = test_dict_anthy_read_private_dic();
  g_assert_nonnull(strstr(contents, "おく #K5*300 置く"));
  g_free(contents);

  fixture_teardown();
}

static void
test_list_window_edit_word(void)
{
  WordListWindow *window;
  WordListView *view;
  WordWindow *word_window;

  if (!test_dict_anthy_setup(ANTHY_FIXTURE))
    return;

  window = open_word_list_window();
  view = WORD_LIST_VIEW(window->word_list);

  select_row(view, model_find_row(view, "てすと", "手素人"));
  activate_action(window, "edit-word");
  word_window = (WordWindow *)wait_for_toplevel(WORD_WINDOW_TYPE, NULL);
  g_assert_nonnull(word_window);
  g_assert_cmpstr(entry_text(word_window->phon), ==, "てすと");
  g_assert_cmpstr(entry_text(word_window->desc), ==, "手素人");
  g_assert_cmpstr(entry_text(word_window->cclass_code),
                  ==,
                  "名詞(語幹,格助接続)");
  g_assert_cmpint(
    gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(word_window->freq)),
    ==,
    100);

  gtk_dialog_response(GTK_DIALOG(word_window), GTK_RESPONSE_CANCEL);
  spin();
  fixture_teardown();
}

static void
test_list_window_remove_word(void)
{
  WordListWindow *window;
  WordListView *view;
  GtkWindow *confirm;
  gchar *contents;

  if (!test_dict_anthy_setup(ANTHY_FIXTURE))
    return;

  window = open_word_list_window();
  view = WORD_LIST_VIEW(window->word_list);

  /* cancelling the confirmation keeps the word */
  select_row(view, model_find_row(view, "てすと", "手素人"));
  activate_action(window, "remove-word");
  confirm = wait_for_toplevel(GTK_TYPE_MESSAGE_DIALOG,
                              "Are you sure to remove the selected word?");
  g_assert_nonnull(confirm);
  gtk_dialog_response(GTK_DIALOG(confirm), REMOVE_CONFIRM_CANCEL);
  spin();
  g_assert_cmpint(model_n_rows(view), ==, 2);
  contents = test_dict_anthy_read_private_dic();
  g_assert_nonnull(strstr(contents, "手素人"));
  g_free(contents);

  /* confirming removes it from the dictionary and from the list */
  select_row(view, model_find_row(view, "てすと", "手素人"));
  activate_action(window, "remove-word");
  confirm = wait_for_toplevel(GTK_TYPE_MESSAGE_DIALOG,
                              "Are you sure to remove the selected word?");
  g_assert_nonnull(confirm);
  gtk_dialog_response(GTK_DIALOG(confirm), REMOVE_CONFIRM_REMOVE);
  close_message("Word deletion succeeded.");

  g_assert_cmpint(model_n_rows(view), ==, 1);
  g_assert_cmpint(model_find_row(view, "てすと", "手素人"), ==, -1);
  g_assert_cmpint(model_find_row(view, "あいう", "愛宇"), ==, 0);
  g_assert_false(action_enabled(window, "remove-word"));

  contents = test_dict_anthy_read_private_dic();
  g_assert_null(strstr(contents, "手素人"));
  g_assert_nonnull(strstr(contents, "愛宇"));
  g_free(contents);

  fixture_teardown();
}
#endif /* USE_ANTHY */

int
main(int argc, char *argv[])
{
  int ret;

  g_test_init(&argc, &argv, NULL);
  test_dict_setup_home();

  if (!gtk_init_check()) {
    /* TAP: skip the whole program */
    printf("1..0 # SKIP cannot open a display (run under xvfb-run)\n");
    test_dict_cleanup_home();
    return 0;
  }

  g_test_add_func("/dict/gtk/word-list-view/defaults", test_view_defaults);
  g_test_add_func("/dict/gtk/word-list-view/properties", test_view_properties);
  g_test_add_func("/dict/gtk/word-list-view/show-words", test_view_show_words);
  g_test_add_func("/dict/gtk/word-list-view/selection-and-remove",
                  test_view_selection_and_remove);
  g_test_add_func("/dict/gtk/word-list-view/refresh-reflects-backend",
                  test_view_refresh_reflects_backend);
  g_test_add_func("/dict/gtk/word-list-view/editable", test_view_editable);

  g_test_add_func("/dict/gtk/word-window/defaults", test_word_window_defaults);
  g_test_add_func("/dict/gtk/word-window/edit-mode",
                  test_word_window_edit_mode);
  g_test_add_func("/dict/gtk/word-window/canna-freq",
                  test_word_window_canna_freq);
  g_test_add_func("/dict/gtk/word-window/close-button",
                  test_word_window_close_button);
  g_test_add_func("/dict/gtk/word-window/add", test_word_window_add);
  g_test_add_func("/dict/gtk/word-window/add-then-cancel",
                  test_word_window_add_then_cancel);
  g_test_add_func("/dict/gtk/word-window/add-continuance",
                  test_word_window_add_continuance);
  g_test_add_func("/dict/gtk/word-window/add-validation",
                  test_word_window_add_validation);
  g_test_add_func("/dict/gtk/word-window/add-failure",
                  test_word_window_add_failure);

#ifdef USE_ANTHY
  g_test_add_func("/dict/gtk/word-list-window/show", test_list_window_show);
  g_test_add_func("/dict/gtk/word-list-window/add-word",
                  test_list_window_add_word);
  g_test_add_func("/dict/gtk/word-list-window/edit-word",
                  test_list_window_edit_word);
  g_test_add_func("/dict/gtk/word-list-window/remove-word",
                  test_list_window_remove_word);
#endif

  ret = g_test_run();
  test_dict_cleanup_home();
  return ret;
}
