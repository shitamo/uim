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
 * GTK tests for gtk-custom-widgets-gtk4.c: the widget that
 * uim_pref_gtk_add_custom() builds for each custom type, how it shows
 * the current value, and how operating it changes the custom value.
 *
 * The file under test is included so that its static callbacks and
 * dialog state (olist_pref_win, key_pref_win) can be reached. The
 * program needs a display; meson runs it under xvfb-run when that is
 * available, and it reports itself as skipped when there is none.
 */

#include <config.h>

#include "pref-test-common.h"

#include "../gtk-custom-widgets-gtk4.c"

/* Normally defined in gtk-gtk4.c, which owns the Apply button. */
gboolean uim_pref_gtk_value_changed = FALSE;
static int mark_value_changed_count = 0;

void
uim_pref_gtk_mark_value_changed(void)
{
  uim_pref_gtk_value_changed = TRUE;
  mark_value_changed_count++;
}

typedef struct
{
  GtkWidget *window;
  GtkWidget *vbox;
} Fixture;

static void
reset_fixture_values(void)
{
  uim_scm_eval_c_string("(for-each (lambda (sym) (custom-set-value! sym "
                        "(custom-default-value sym)))"
                        " (custom-collect-by-group 'pref-test))");
}

static void
fixture_set_up(Fixture *fixture, gconstpointer data)
{
  char **syms, **sym;

  reset_fixture_values();
  uim_pref_gtk_value_changed = FALSE;
  mark_value_changed_count = 0;

  fixture->window = gtk_window_new();
  fixture->vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_window_set_child(GTK_WINDOW(fixture->window), fixture->vbox);

  syms = uim_custom_collect_by_group("pref-test");
  for (sym = syms; *sym; sym++)
    uim_pref_gtk_add_custom(fixture->vbox, *sym);
  uim_custom_symbol_list_free(syms);

  gtk_window_present(GTK_WINDOW(fixture->window));

  /* building the widgets must not count as a change */
  g_assert_cmpint(mark_value_changed_count, ==, 0);
}

static void
destroy_all_toplevels(void)
{
  GListModel *toplevels = gtk_window_get_toplevels();

  while (g_list_model_get_n_items(toplevels) > 0) {
    GtkWindow *window = g_list_model_get_item(toplevels, 0);

    gtk_window_destroy(window);
    g_object_unref(window);
  }
}

static void
fixture_tear_down(Fixture *fixture, gconstpointer data)
{
  /* the widgets are gone after this, so drop their update callbacks */
  uim_custom_cb_remove(NULL);
  destroy_all_toplevels();
  while (g_main_context_iteration(NULL, FALSE))
    ;
  memset(&olist_pref_win, 0, sizeof(olist_pref_win));
  memset(&key_pref_win, 0, sizeof(key_pref_win));
}

static GtkWidget *
find_custom_widget_in(GtkWidget *widget, const char *custom_sym)
{
  const char *sym;
  GtkWidget *child;

  sym = g_object_get_data(G_OBJECT(widget), OBJECT_DATA_UIM_CUSTOM_SYM);
  if (sym && !strcmp(sym, custom_sym))
    return widget;

  for (child = gtk_widget_get_first_child(widget); child;
       child = gtk_widget_get_next_sibling(child)) {
    GtkWidget *found = find_custom_widget_in(child, custom_sym);
    if (found)
      return found;
  }
  return NULL;
}

static GtkWidget *
find_custom_widget(Fixture *fixture, const char *custom_sym)
{
  GtkWidget *widget = find_custom_widget_in(fixture->vbox, custom_sym);

  g_assert_nonnull(widget);
  return widget;
}

/* The first descendant of widget (or widget itself) of the given type. */
static GtkWidget *
find_widget_by_type(GtkWidget *widget, GType type)
{
  GtkWidget *child;

  if (G_TYPE_CHECK_INSTANCE_TYPE(widget, type))
    return widget;
  for (child = gtk_widget_get_first_child(widget); child;
       child = gtk_widget_get_next_sibling(child)) {
    GtkWidget *found = find_widget_by_type(child, type);
    if (found)
      return found;
  }
  return NULL;
}

/* The GtkButton next to a custom widget, i.e. "Edit..." or "Select...". */
static GtkWidget *
sibling_button(GtkWidget *widget)
{
  GtkWidget *sibling;

  for (sibling = gtk_widget_get_next_sibling(widget); sibling;
       sibling = gtk_widget_get_next_sibling(sibling))
    if (GTK_IS_BUTTON(sibling))
      return sibling;
  g_assert_not_reached();
  return NULL;
}

/* The text of the GtkLabel that precedes widget in its row. */
static const char *
row_label(GtkWidget *widget)
{
  GtkWidget *label = gtk_widget_get_first_child(gtk_widget_get_parent(widget));

  g_assert_true(GTK_IS_LABEL(label));
  return gtk_label_get_text(GTK_LABEL(label));
}

static void
set_value(const char *custom_sym, const char *literal)
{
  char *expr =
    g_strdup_printf("(custom-set-value! '%s %s)", custom_sym, literal);
  uim_lisp rv = uim_scm_eval_c_string(expr);

  g_assert_true(uim_scm_c_bool(rv));
  g_free(expr);
}

static void
click(GtkWidget *button)
{
  g_assert_true(GTK_IS_BUTTON(button));
  g_signal_emit_by_name(button, "clicked");
}

static int
model_n_rows(GtkTreeView *view)
{
  return gtk_tree_model_iter_n_children(gtk_tree_view_get_model(view), NULL);
}

static char *
model_get_string(GtkTreeView *view, int row, int column)
{
  GtkTreeModel *model = gtk_tree_view_get_model(view);
  GtkTreeIter iter;
  char *str = NULL;

  g_assert_true(gtk_tree_model_iter_nth_child(model, &iter, NULL, row));
  gtk_tree_model_get(model, &iter, column, &str, -1);
  return str;
}

#define assert_model_string(view, row, column, expected)                       \
  do {                                                                         \
    char *str_ = model_get_string((view), (row), (column));                    \
    g_assert_cmpstr(str_, ==, (expected));                                     \
    g_free(str_);                                                              \
  } while (0)

static void
select_row(GtkTreeView *view, int row)
{
  GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
  GtkTreePath *path = gtk_tree_path_new_from_indices(row, -1);

  gtk_tree_selection_unselect_all(selection);
  gtk_tree_selection_select_path(selection, path);
  gtk_tree_path_free(path);
}

static void
assert_editable_text(GtkWidget *widget, const char *expected)
{
  g_assert_cmpstr(gtk_editable_get_text(GTK_EDITABLE(widget)), ==, expected);
}

static void
test_bool(Fixture *fixture, gconstpointer data)
{
  GtkWidget *button = find_custom_widget(fixture, "pref-test-bool");

  g_assert_true(GTK_IS_CHECK_BUTTON(button));
  g_assert_cmpstr(gtk_check_button_get_label(GTK_CHECK_BUTTON(button)),
                  ==,
                  "Test boolean");
  g_assert_true(gtk_check_button_get_active(GTK_CHECK_BUTTON(button)));
  g_assert_true(gtk_widget_get_sensitive(button));

  /* user toggles the check button */
  gtk_check_button_set_active(GTK_CHECK_BUTTON(button), FALSE);
  pref_test_assert_value("pref-test-bool", "#f");
  g_assert_cmpint(mark_value_changed_count, ==, 1);
  g_assert_true(uim_pref_gtk_value_changed);

  /* the value changes elsewhere */
  set_value("pref-test-bool", "#t");
  g_assert_true(gtk_check_button_get_active(GTK_CHECK_BUTTON(button)));
  /* syncing the widget is not a user change */
  g_assert_cmpint(mark_value_changed_count, ==, 1);
}

static void
test_int(Fixture *fixture, gconstpointer data)
{
  GtkWidget *spin = find_custom_widget(fixture, "pref-test-int");
  GtkAdjustment *adj;

  g_assert_true(GTK_IS_SPIN_BUTTON(spin));
  g_assert_cmpstr(row_label(spin), ==, "Test integer");
  g_assert_cmpint(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin)),
                  ==,
                  10);
  adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spin));
  g_assert_cmpfloat(gtk_adjustment_get_lower(adj), ==, 0);
  g_assert_cmpfloat(gtk_adjustment_get_upper(adj), ==, 100);

  gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), 55);
  pref_test_assert_value("pref-test-int", "55");
  g_assert_cmpint(mark_value_changed_count, ==, 1);

  /* the spin button clamps to the custom's range */
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), 1000);
  pref_test_assert_value("pref-test-int", "100");

  set_value("pref-test-int", "3");
  g_assert_cmpint(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin)),
                  ==,
                  3);
}

/* pref-test-int is active only while pref-test-bool is #t, and its row
 * is greyed out accordingly. */
static void
test_activity(Fixture *fixture, gconstpointer data)
{
  GtkWidget *button = find_custom_widget(fixture, "pref-test-bool");
  GtkWidget *spin = find_custom_widget(fixture, "pref-test-int");
  GtkWidget *entry = find_custom_widget(fixture, "pref-test-str");

  g_assert_true(gtk_widget_get_sensitive(gtk_widget_get_parent(spin)));

  gtk_check_button_set_active(GTK_CHECK_BUTTON(button), FALSE);
  g_assert_false(gtk_widget_get_sensitive(gtk_widget_get_parent(spin)));
  g_assert_false(gtk_widget_is_sensitive(spin));
  g_assert_true(gtk_widget_get_sensitive(gtk_widget_get_parent(entry)));

  gtk_check_button_set_active(GTK_CHECK_BUTTON(button), TRUE);
  g_assert_true(gtk_widget_get_sensitive(gtk_widget_get_parent(spin)));
}

static void
test_string(Fixture *fixture, gconstpointer data)
{
  GtkWidget *entry = find_custom_widget(fixture, "pref-test-str");
  struct uim_custom *custom;

  g_assert_true(GTK_IS_ENTRY(entry));
  g_assert_false(IS_UIMPREF_FILE_ENTRY(entry));
  g_assert_cmpstr(row_label(entry), ==, "Test string");
  assert_editable_text(entry, "hello");

  gtk_editable_set_text(GTK_EDITABLE(entry), "world");
  custom = uim_custom_get("pref-test-str");
  g_assert_cmpstr(custom->value->as_str, ==, "world");
  uim_custom_free(custom);
  g_assert_cmpint(mark_value_changed_count, >=, 1);

  set_value("pref-test-str", "\"from elsewhere\"");
  assert_editable_text(entry, "from elsewhere");
}

/* Typing must not move the cursor: the entry's own change comes back
 * through the update callback with an identical value, and
 * sync_value_string() must not reset the text then. */
static void
test_string_keeps_cursor(Fixture *fixture, gconstpointer data)
{
  GtkWidget *entry = find_custom_widget(fixture, "pref-test-str");
  int pos = 5;

  gtk_editable_set_position(GTK_EDITABLE(entry), 2);
  gtk_editable_insert_text(GTK_EDITABLE(entry), "!", -1, &pos);
  assert_editable_text(entry, "hello!");
  gtk_editable_set_position(GTK_EDITABLE(entry), 3);

  set_value("pref-test-str", "\"hello!\"");
  g_assert_cmpint(gtk_editable_get_position(GTK_EDITABLE(entry)), ==, 3);
}

static void
test_pathname(Fixture *fixture, gconstpointer data)
{
  GtkWidget *file = find_custom_widget(fixture, "pref-test-file");
  GtkWidget *dir = find_custom_widget(fixture, "pref-test-dir");
  GtkWidget *button;
  struct uim_custom *custom;

  g_assert_true(IS_UIMPREF_FILE_ENTRY(file));
  g_assert_cmpint(UIMPREF_FILE_ENTRY(file)->type,
                  ==,
                  UCustomPathnameType_RegularFile);
  assert_editable_text(file, "/tmp/pref-test-file");

  g_assert_true(IS_UIMPREF_FILE_ENTRY(dir));
  g_assert_cmpint(UIMPREF_FILE_ENTRY(dir)->type,
                  ==,
                  UCustomPathnameType_Directory);
  assert_editable_text(dir, "/tmp");

  /* the label sits above the entry and its "Select..." button */
  button = sibling_button(file);
  g_assert_cmpstr(gtk_button_get_label(GTK_BUTTON(button)), ==, "Select...");
  g_assert_cmpstr(gtk_label_get_text(GTK_LABEL(
                    gtk_widget_get_prev_sibling(gtk_widget_get_parent(file)))),
                  ==,
                  "Test file");

  gtk_editable_set_text(GTK_EDITABLE(file), "/usr/share/dict/words");
  custom = uim_custom_get("pref-test-file");
  g_assert_cmpstr(custom->value->as_pathname->str, ==, "/usr/share/dict/words");
  g_assert_cmpint(custom->value->as_pathname->type,
                  ==,
                  UCustomPathnameType_RegularFile);
  uim_custom_free(custom);
  g_assert_cmpint(mark_value_changed_count, >=, 1);

  set_value("pref-test-dir", "\"/var\"");
  assert_editable_text(dir, "/var");
}

static void
test_choice(Fixture *fixture, gconstpointer data)
{
  GtkWidget *combo = find_custom_widget(fixture, "pref-test-choice");
  GtkTreeModel *model;
  GtkTreeIter iter;
  char *text;

  g_assert_true(GTK_IS_COMBO_BOX_TEXT(combo));
  g_assert_cmpstr(row_label(combo), ==, "Test choice");

  model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));
  g_assert_cmpint(gtk_tree_model_iter_n_children(model, NULL), ==, 3);
  g_assert_true(gtk_tree_model_iter_nth_child(model, &iter, NULL, 2));
  gtk_tree_model_get(model, &iter, 0, &text, -1);
  g_assert_cmpstr(text, ==, "Gamma");
  g_free(text);

  g_assert_cmpint(gtk_combo_box_get_active(GTK_COMBO_BOX(combo)), ==, 1);
  text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
  g_assert_cmpstr(text, ==, "Beta");
  g_free(text);

  gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 2);
  pref_test_assert_value("pref-test-choice", "'gamma");
  g_assert_cmpint(mark_value_changed_count, ==, 1);

  set_value("pref-test-choice", "'alpha");
  g_assert_cmpint(gtk_combo_box_get_active(GTK_COMBO_BOX(combo)), ==, 0);
  /* the list is rebuilt, not appended to */
  g_assert_cmpint(gtk_tree_model_iter_n_children(model, NULL), ==, 3);
}

static void
assert_olist_buttons(gboolean up, gboolean down, gboolean left, gboolean right)
{
  g_assert_cmpint(gtk_widget_get_sensitive(olist_pref_win.up_button), ==, up);
  g_assert_cmpint(gtk_widget_get_sensitive(olist_pref_win.down_button),
                  ==,
                  down);
  g_assert_cmpint(gtk_widget_get_sensitive(olist_pref_win.left_button),
                  ==,
                  left);
  g_assert_cmpint(gtk_widget_get_sensitive(olist_pref_win.right_button),
                  ==,
                  right);
}

static void
test_olist(Fixture *fixture, gconstpointer data)
{
  GtkWidget *entry = find_custom_widget(fixture, "pref-test-olist");
  GtkTreeView *enabled, *disabled;

  g_assert_true(GTK_IS_ENTRY(entry));
  g_assert_false(gtk_editable_get_editable(GTK_EDITABLE(entry)));
  g_assert_cmpstr(row_label(entry), ==, "Test ordered list");
  assert_editable_text(entry, "\"One\", \"Two\"");

  /* "Edit..." opens the two-pane editor */
  click(sibling_button(entry));
  g_assert_true(GTK_IS_DIALOG(olist_pref_win.window));
  g_assert_cmpstr(gtk_window_get_title(GTK_WINDOW(olist_pref_win.window)),
                  ==,
                  "Test ordered list");
  enabled = GTK_TREE_VIEW(olist_pref_win.tree_view[0]);
  disabled = GTK_TREE_VIEW(olist_pref_win.tree_view[1]);
  g_assert_cmpint(model_n_rows(enabled), ==, 2);
  assert_model_string(enabled, 0, 0, "One");
  assert_model_string(enabled, 0, 1, "one");
  assert_model_string(enabled, 1, 0, "Two");
  g_assert_cmpint(model_n_rows(disabled), ==, 1);
  assert_model_string(disabled, 0, 0, "Three");
  assert_olist_buttons(FALSE, FALSE, FALSE, FALSE);

  /* The last enabled item can move up but not down. Down should be
   * insensitive here, but set_olist_buttons_sensitive() calls
   * gtk_tree_path_prev() on the path before checking for the last row
   * (inherited from the GTK 3 version), so only check that pressing it
   * changes nothing. */
  select_row(enabled, 1);
  g_assert_true(gtk_widget_get_sensitive(olist_pref_win.up_button));
  g_assert_false(gtk_widget_get_sensitive(olist_pref_win.left_button));
  g_assert_true(gtk_widget_get_sensitive(olist_pref_win.right_button));
  click(olist_pref_win.down_button);
  pref_test_assert_value("pref-test-olist", "'(one two)");
  g_assert_cmpint(mark_value_changed_count, ==, 0);
  click(olist_pref_win.up_button);
  pref_test_assert_value("pref-test-olist", "'(two one)");
  assert_editable_text(entry, "\"Two\", \"One\"");
  assert_model_string(enabled, 0, 1, "two");
  g_assert_cmpint(mark_value_changed_count, ==, 1);

  /* the selection follows the item, now first */
  assert_olist_buttons(FALSE, TRUE, FALSE, TRUE);
  click(olist_pref_win.down_button);
  pref_test_assert_value("pref-test-olist", "'(one two)");

  /* disable "One" */
  select_row(enabled, 0);
  click(olist_pref_win.right_button);
  pref_test_assert_value("pref-test-olist", "'(two)");
  assert_editable_text(entry, "\"Two\"");
  g_assert_cmpint(model_n_rows(enabled), ==, 1);
  g_assert_cmpint(model_n_rows(disabled), ==, 2);

  /* enable "Three" and "One" again; they are appended in list order */
  gtk_tree_selection_select_all(gtk_tree_view_get_selection(disabled));
  assert_olist_buttons(FALSE, FALSE, TRUE, FALSE);
  click(olist_pref_win.left_button);
  pref_test_assert_value("pref-test-olist", "'(two one three)");
  g_assert_cmpint(model_n_rows(enabled), ==, 3);
  g_assert_cmpint(model_n_rows(disabled), ==, 0);

  /* several selected items move to the disabled side at once */
  gtk_tree_selection_select_all(gtk_tree_view_get_selection(enabled));
  assert_olist_buttons(FALSE, FALSE, FALSE, TRUE);
  click(olist_pref_win.right_button);
  pref_test_assert_value("pref-test-olist", "'()");
  assert_editable_text(entry, "");

  gtk_dialog_response(GTK_DIALOG(olist_pref_win.window), GTK_RESPONSE_CLOSE);
}

/* "-" is usable exactly while a key is selected */
static void
assert_key_remove_button(GtkTreeView *view)
{
  g_assert_cmpint(
    gtk_widget_get_sensitive(key_pref_win.remove_button),
    ==,
    gtk_tree_selection_get_selected(gtk_tree_view_get_selection(view),
                                    NULL,
                                    NULL));
}

static void
test_key(Fixture *fixture, gconstpointer data)
{
  GtkWidget *entry = find_custom_widget(fixture, "pref-test-key");
  GtkTreeView *view;
  struct uim_custom *custom;

  g_assert_true(GTK_IS_ENTRY(entry));
  g_assert_false(gtk_editable_get_editable(GTK_EDITABLE(entry)));
  g_assert_cmpstr(row_label(entry), ==, "Test key");
  assert_editable_text(entry, "\"<Control>j\", \"escape\"");

  click(sibling_button(entry));
  g_assert_true(GTK_IS_DIALOG(key_pref_win.window));
  g_assert_cmpstr(gtk_window_get_title(GTK_WINDOW(key_pref_win.window)),
                  ==,
                  "Test key - key configuration");
  view = GTK_TREE_VIEW(key_pref_win.tree_view);
  g_assert_cmpint(model_n_rows(view), ==, 2);
  assert_model_string(view, 0, 0, "<Control>j");
  assert_model_string(view, 1, 0, "escape");
  g_assert_false(gtk_widget_get_sensitive(key_pref_win.add_button));
  /* GtkTreeView may select its cursor row when it gets the focus */
  assert_key_remove_button(view);
  gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(view));
  g_assert_false(gtk_widget_get_sensitive(key_pref_win.remove_button));

  /* a grabbed key fills the "Key:" entry and enables "+" */
  key_pref_set_value('a', UMod_Control | UMod_Alt);
  assert_editable_text(key_pref_win.keycode_entry, "<Control><Alt>a");
  g_assert_true(gtk_widget_get_sensitive(key_pref_win.add_button));

  click(key_pref_win.add_button);
  g_assert_cmpint(model_n_rows(view), ==, 3);
  assert_model_string(view, 2, 0, "<Control><Alt>a");
  assert_editable_text(entry,
                       "\"<Control>j\", \"escape\", \"<Control><Alt>a\"");
  custom = uim_custom_get("pref-test-key");
  g_assert_cmpstr(custom->value->as_key[2]->literal, ==, "<Control><Alt>a");
  g_assert_null(custom->value->as_key[3]);
  uim_custom_free(custom);
  g_assert_cmpint(mark_value_changed_count, ==, 1);

  /* remove the first key */
  select_row(view, 0);
  g_assert_true(gtk_widget_get_sensitive(key_pref_win.remove_button));
  click(key_pref_win.remove_button);
  g_assert_cmpint(model_n_rows(view), ==, 2);
  assert_model_string(view, 0, 0, "escape");
  assert_editable_text(entry, "\"escape\", \"<Control><Alt>a\"");
  assert_key_remove_button(view);
  custom = uim_custom_get("pref-test-key");
  g_assert_cmpstr(custom->value->as_key[0]->literal, ==, "escape");
  g_assert_null(custom->value->as_key[2]);
  uim_custom_free(custom);

  /* an empty "Key:" entry adds nothing */
  gtk_editable_set_text(GTK_EDITABLE(key_pref_win.keycode_entry), "");
  g_assert_false(gtk_widget_get_sensitive(key_pref_win.add_button));
  key_pref_add_button_clicked_cb(key_pref_win.add_button, GTK_ENTRY(entry));
  g_assert_cmpint(model_n_rows(view), ==, 2);

  gtk_dialog_response(GTK_DIALOG(key_pref_win.window), GTK_RESPONSE_CLOSE);
}

/* key_pref_set_value() turns a (UKey, UMod) pair from
 * im_uim_convert_keyevent() into a key literal. */
static void
test_key_literal(Fixture *fixture, gconstpointer data)
{
  static const struct
  {
    int ukey;
    int umod;
    const char *literal;
  } cases[] = {
    {'a', 0, "a"},
    {'a', UMod_Control, "<Control>a"},
    /* Shift is dropped for graphic characters; the character itself
     * already says it */
    {'A', UMod_Shift, "A"},
    {'!', UMod_Shift | UMod_Control, "<Control>!"},
    {' ', 0, "space"},
    {' ', UMod_Shift, "<Shift>space"},
    {UKey_Escape, 0, "escape"},
    {UKey_Return, UMod_Shift, "<Shift>return"},
    {UKey_F1,
     UMod_Shift | UMod_Control | UMod_Alt | UMod_Meta | UMod_Super | UMod_Hyper,
     "<Shift><Control><Alt><Meta><Super><Hyper>F1"},
    {UKey_Zenkaku_Hankaku, 0, "zenkaku-hankaku"},
    {UKey_Other, UMod_Control, "<Control>"},
  };
  GtkWidget *entry = find_custom_widget(fixture, "pref-test-key");
  guint i;

  click(sibling_button(entry));

  for (i = 0; i < G_N_ELEMENTS(cases); i++) {
    key_pref_win.grabbed_key_val = 1;
    key_pref_win.grabbed_key_state = 1;
    key_pref_set_value(cases[i].ukey, cases[i].umod);
    assert_editable_text(key_pref_win.keycode_entry, cases[i].literal);
    /* the grab state is cleared for the next key */
    g_assert_cmpint(key_pref_win.grabbed_key_val, ==, 0);
    g_assert_cmpint(key_pref_win.grabbed_key_state, ==, 0);
  }

  /* each complete literal is accepted by uim when added */
  for (i = 0; i < G_N_ELEMENTS(cases) - 1; i++) {
    struct uim_custom *custom;
    int n;

    key_pref_set_value(cases[i].ukey, cases[i].umod);
    click(key_pref_win.add_button);
    custom = uim_custom_get("pref-test-key");
    for (n = 0; custom->value->as_key[n]; n++)
      ;
    g_assert_cmpint(n, ==, 2 + i + 1);
    g_assert_cmpstr(custom->value->as_key[n - 1]->literal,
                    ==,
                    cases[i].literal);
    uim_custom_free(custom);
  }
}

static GtkWidget *
find_dialog(void)
{
  GListModel *toplevels = gtk_window_get_toplevels();
  guint i;

  for (i = 0; i < g_list_model_get_n_items(toplevels); i++) {
    GtkWidget *window = g_list_model_get_item(toplevels, i);

    g_object_unref(window);
    if (GTK_IS_DIALOG(window))
      return window;
  }
  return NULL;
}

static void
edit_table_cell(GtkTreeView *view, int row, int column, const char *text)
{
  GtkTreeViewColumn *view_column = gtk_tree_view_get_column(view, column);
  GList *cells = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(view_column));
  char *path = g_strdup_printf("%d", row);

  g_assert_nonnull(cells);
  g_signal_emit_by_name(cells->data, "edited", path, text);
  g_free(path);
  g_list_free(cells);
}

static void
test_table(Fixture *fixture, gconstpointer data)
{
  GtkWidget *label = find_custom_widget(fixture, "pref-test-table");
  GtkWidget *dialog;
  GtkTreeView *view;

  g_assert_true(GTK_IS_LABEL(label));
  g_assert_cmpstr(gtk_label_get_text(GTK_LABEL(label)), ==, "Test table");

  click(sibling_button(label));
  dialog = find_dialog();
  g_assert_nonnull(dialog);
  g_assert_cmpstr(gtk_window_get_title(GTK_WINDOW(dialog)), ==, "Test table");
  view = GTK_TREE_VIEW(find_widget_by_type(dialog, GTK_TYPE_TREE_VIEW));
  g_assert_nonnull(view);

  /* one column per header item, one row per table row */
  g_assert_cmpuint(gtk_tree_view_get_n_columns(view), ==, 2);
  g_assert_cmpstr(
    gtk_tree_view_column_get_title(gtk_tree_view_get_column(view, 0)),
    ==,
    "Input");
  g_assert_cmpstr(
    gtk_tree_view_column_get_title(gtk_tree_view_get_column(view, 1)),
    ==,
    "Output");
  g_assert_cmpint(model_n_rows(view), ==, 2);
  assert_model_string(view, 0, 0, "a");
  assert_model_string(view, 0, 1, "x");
  assert_model_string(view, 1, 0, "b");
  assert_model_string(view, 1, 1, "y");

  /* "+" appends an empty row */
  table_pref_add_button_clicked_cb(NULL, view);
  g_assert_cmpint(model_n_rows(view), ==, 3);
  assert_model_string(view, 2, 0, "");
  g_assert_cmpint(mark_value_changed_count, ==, 1);

  edit_table_cell(view, 2, 0, "c");
  edit_table_cell(view, 2, 1, "z");
  assert_model_string(view, 2, 0, "c");
  assert_model_string(view, 2, 1, "z");

  /* move the new row to the top */
  select_row(view, 2);
  table_pref_up_button_clicked_cb(NULL, view);
  table_pref_up_button_clicked_cb(NULL, view);
  assert_model_string(view, 0, 0, "c");
  /* the first row cannot move further up */
  table_pref_up_button_clicked_cb(NULL, view);
  assert_model_string(view, 0, 0, "c");
  table_pref_down_button_clicked_cb(NULL, view);
  assert_model_string(view, 1, 0, "c");

  /* remove "a x" */
  select_row(view, 0);
  table_pref_remove_button_clicked_cb(NULL, view);
  g_assert_cmpint(model_n_rows(view), ==, 2);

  /* edits stay in the dialog until it is closed */
  pref_test_assert_value("pref-test-table", "'((\"a\" \"x\") (\"b\" \"y\"))");
  gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);
  pref_test_assert_value("pref-test-table", "'((\"c\" \"z\") (\"b\" \"y\"))");
}

static void
test_set_default_value(Fixture *fixture, gconstpointer data)
{
  static const char *syms[] = {
    "pref-test-bool",
    "pref-test-int",
    "pref-test-str",
    "pref-test-file",
    "pref-test-choice",
    "pref-test-olist",
    "pref-test-key",
    "pref-test-table",
  };
  guint i;

  set_value("pref-test-bool", "#f");
  set_value("pref-test-int", "99");
  set_value("pref-test-str", "\"changed\"");
  set_value("pref-test-file", "\"/changed\"");
  set_value("pref-test-choice", "'gamma");
  set_value("pref-test-olist", "'(three)");
  set_value("pref-test-key", "'(\"<Alt>q\")");
  set_value("pref-test-table", "'((\"q\"))");
  assert_editable_text(find_custom_widget(fixture, "pref-test-key"),
                       "\"<Alt>q\"");

  for (i = 0; i < G_N_ELEMENTS(syms); i++)
    uim_pref_gtk_set_default_value(find_custom_widget(fixture, syms[i]));

  pref_test_assert_value("pref-test-bool", "#t");
  pref_test_assert_value("pref-test-int", "10");
  pref_test_assert_value("pref-test-str", "\"hello\"");
  pref_test_assert_value("pref-test-file", "\"/tmp/pref-test-file\"");
  pref_test_assert_value("pref-test-choice", "'beta");
  pref_test_assert_value("pref-test-olist", "'(one two)");
  /* pref-test-table is not checked: libuim gives a table custom its
   * current value as default_value (see /pref/custom/table-default). */

  g_assert_true(gtk_check_button_get_active(
    GTK_CHECK_BUTTON(find_custom_widget(fixture, "pref-test-bool"))));
  g_assert_cmpint(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(
                    find_custom_widget(fixture, "pref-test-int"))),
                  ==,
                  10);
  assert_editable_text(find_custom_widget(fixture, "pref-test-str"), "hello");
  assert_editable_text(find_custom_widget(fixture, "pref-test-file"),
                       "/tmp/pref-test-file");
  g_assert_cmpint(gtk_combo_box_get_active(GTK_COMBO_BOX(
                    find_custom_widget(fixture, "pref-test-choice"))),
                  ==,
                  1);
  assert_editable_text(find_custom_widget(fixture, "pref-test-olist"),
                       "\"One\", \"Two\"");
  assert_editable_text(find_custom_widget(fixture, "pref-test-key"),
                       "\"<Control>j\", \"escape\"");

  /* widgets without a custom symbol are ignored */
  uim_pref_gtk_set_default_value(fixture->vbox);
}

#define ADD_TEST(path, func)                                                   \
  g_test_add("/pref/widgets/" path,                                            \
             Fixture,                                                          \
             NULL,                                                             \
             fixture_set_up,                                                   \
             func,                                                             \
             fixture_tear_down)

int
main(int argc, char *argv[])
{
  int status;

  g_test_init(&argc, &argv, NULL);
  pref_test_setup_environment();

  if (!gtk_init_check())
    return pref_test_skip_all("no display is available");

  pref_test_init_uim();

  ADD_TEST("bool", test_bool);
  ADD_TEST("int", test_int);
  ADD_TEST("activity", test_activity);
  ADD_TEST("string", test_string);
  ADD_TEST("string-keeps-cursor", test_string_keeps_cursor);
  ADD_TEST("pathname", test_pathname);
  ADD_TEST("choice", test_choice);
  ADD_TEST("olist", test_olist);
  ADD_TEST("key", test_key);
  ADD_TEST("key-literal", test_key_literal);
  ADD_TEST("table", test_table);
  ADD_TEST("set-default-value", test_set_default_value);

  status = g_test_run();

  pref_test_quit_uim();
  return status;
}
