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
 * GTK tests for gtk-gtk4.c, the main window of uim-pref-gtk4: the group
 * list, switching between group pages, and the Defaults/Apply/OK
 * buttons and quit confirmation.
 *
 * gtk-gtk4.c is included, with its main() renamed, so that the static
 * window construction functions and state can be reached. The program
 * needs a display; meson runs it under xvfb-run when that is
 * available, and it reports itself as skipped when there is none.
 */

#include <config.h>

#include "pref-test-common.h"

#define main uim_pref_gtk4_main
#include "../gtk-gtk4.c"
#undef main

static void
reset_fixture_values(void)
{
  uim_scm_eval_c_string("(for-each (lambda (sym) (custom-set-value! sym "
                        "(custom-default-value sym)))"
                        " (custom-collect-by-group 'pref-test))");
}

static void
iterate_main_loop(void)
{
  while (g_main_context_iteration(NULL, FALSE))
    ;
}

static guint
n_toplevels(void)
{
  return g_list_model_get_n_items(gtk_window_get_toplevels());
}

static void
fixture_set_up(gpointer fixture, gconstpointer data)
{
  reset_fixture_values();
  create_pref_window();
  gtk_window_present(GTK_WINDOW(pref_window));
  iterate_main_loop();
  g_assert_false(uim_pref_gtk_value_changed);
}

static void
fixture_tear_down(gpointer fixture, gconstpointer data)
{
  GListModel *toplevels = gtk_window_get_toplevels();
  char *path;

  uim_custom_cb_remove(NULL);
  while (g_list_model_get_n_items(toplevels) > 0) {
    GtkWindow *window = g_list_model_get_item(toplevels, 0);

    gtk_window_destroy(window);
    g_object_unref(window);
  }
  iterate_main_loop();

  pref_window = pref_tree_view = pref_hbox = current_group_widget = NULL;
  pref_apply_button = pref_ok_button = NULL;
  uim_pref_gtk_value_changed = FALSE;

  path = pref_test_custom_file("pref-test");
  g_unlink(path);
  g_free(path);
  g_free(pref_test_read_helper_messages());
}

/* The number of reload requests Apply/OK have broadcast since the last
 * call. */
static int
n_reload_requests(void)
{
  char *messages = pref_test_read_helper_messages();
  char **lines = g_strsplit(messages, "\n", -1);
  char **line;
  int n = 0;

  for (line = lines; *line; line++) {
    if (!strcmp(*line, "custom_reload_notify"))
      n++;
    else
      g_assert_cmpstr(*line, ==, "");
  }
  g_strfreev(lines);
  g_free(messages);
  return n;
}

/* Select the row of group_sym in the group list and return its iter. */
static void
select_group(const char *group_sym, GtkTreeIter *iter_ret)
{
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(pref_tree_view));
  GtkTreeIter iter;
  gboolean valid;

  for (valid = gtk_tree_model_get_iter_first(model, &iter); valid;
       valid = gtk_tree_model_iter_next(model, &iter)) {
    char *sym;

    gtk_tree_model_get(model, &iter, GROUP_SYM, &sym, -1);
    if (!strcmp(sym, group_sym)) {
      g_free(sym);
      gtk_tree_selection_select_iter(
        gtk_tree_view_get_selection(GTK_TREE_VIEW(pref_tree_view)),
        &iter);
      if (iter_ret)
        *iter_ret = iter;
      return;
    }
    g_free(sym);
  }
  g_assert_not_reached();
}

static GtkWidget *
row_widget(GtkTreeIter *iter)
{
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(pref_tree_view));
  GtkWidget *widget;

  gtk_tree_model_get(model, iter, GROUP_WIDGET, &widget, -1);
  if (widget)
    g_object_unref(widget);
  return widget;
}

static GtkWidget *
find_custom_widget_in(GtkWidget *widget, const char *custom_sym)
{
  const char *sym;
  GtkWidget *child;

  sym = g_object_get_data(G_OBJECT(widget), "uim-pref-gtk::uim-custom-sym");
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
find_custom_widget(const char *custom_sym)
{
  GtkWidget *widget = find_custom_widget_in(current_group_widget, custom_sym);

  g_assert_nonnull(widget);
  return widget;
}

static GtkWidget *
find_button(GtkWidget *widget, const char *label)
{
  GtkWidget *child;

  if (GTK_IS_BUTTON(widget)) {
    const char *button_label = gtk_button_get_label(GTK_BUTTON(widget));
    if (button_label && !strcmp(button_label, label))
      return widget;
  }
  for (child = gtk_widget_get_first_child(widget); child;
       child = gtk_widget_get_next_sibling(child)) {
    GtkWidget *found = find_button(child, label);
    if (found)
      return found;
  }
  return NULL;
}

static void
click(const char *label)
{
  GtkWidget *button = find_button(pref_window, label);

  g_assert_nonnull(button);
  g_signal_emit_by_name(button, "clicked");
}

static GtkWidget *
frame_ancestor(GtkWidget *widget)
{
  return gtk_widget_get_ancestor(widget, GTK_TYPE_FRAME);
}

static void
test_group_list(gpointer fixture, gconstpointer data)
{
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(pref_tree_view));
  char **groups, **grp;
  GtkTreeIter iter;
  gboolean valid;
  int width, height;

  /* one row per primary group, in order, labelled with the group label */
  groups = uim_custom_primary_groups();
  valid = gtk_tree_model_get_iter_first(model, &iter);
  for (grp = groups; *grp; grp++) {
    struct uim_custom_group *group = uim_custom_group_get(*grp);
    char *label, *sym;

    g_assert_true(valid);
    gtk_tree_model_get(model, &iter, GROUP_COLUMN, &label, GROUP_SYM, &sym, -1);
    g_assert_cmpstr(sym, ==, *grp);
    g_assert_cmpstr(label, ==, group->label);
    /* group pages are built lazily; only the first one exists */
    if (grp == groups)
      g_assert_nonnull(row_widget(&iter));
    else
      g_assert_null(row_widget(&iter));
    g_free(label);
    g_free(sym);
    uim_custom_group_free(group);
    valid = gtk_tree_model_iter_next(model, &iter);
  }
  g_assert_false(valid);
  uim_custom_symbol_list_free(groups);

  /* the first group is shown */
  g_assert_true(gtk_tree_model_get_iter_first(model, &iter));
  g_assert_true(current_group_widget == row_widget(&iter));
  g_assert_true(gtk_widget_get_parent(current_group_widget) == pref_hbox);

  /* nothing to apply yet */
  g_assert_false(gtk_widget_get_sensitive(pref_apply_button));
  g_assert_true(gtk_widget_get_sensitive(pref_ok_button));
  g_assert_nonnull(find_button(pref_window, "_Defaults"));
  g_assert_nonnull(find_button(pref_window, "_Cancel"));

  /* 95% of the 1024x768 Xvfb screen, capped at 800x600 */
  gtk_window_get_default_size(GTK_WINDOW(pref_window), &width, &height);
  g_assert_cmpint(width, <=, DEFAULT_WINDOW_WIDTH_MAX);
  g_assert_cmpint(height, <=, DEFAULT_WINDOW_HEIGHT_MAX);
  g_assert_cmpint(width, >, 0);
  g_assert_cmpint(height, >, 0);
}

static void
test_switch_group(gpointer fixture, gconstpointer data)
{
  GtkWidget *first_page = current_group_widget, *page, *child;
  GtkTreeIter iter, first_iter;
  char *first_sym;

  select_group("pref-test", &iter);
  page = row_widget(&iter);
  g_assert_nonnull(page);
  g_assert_true(GTK_IS_SCROLLED_WINDOW(page));
  g_assert_true(current_group_widget == page);
  g_assert_true(gtk_widget_get_parent(page) == pref_hbox);
  g_assert_true(gtk_widget_get_visible(page));
  g_assert_false(gtk_widget_get_visible(first_page));

  /* heading, then the 'main' subgroup unframed, then one frame per
   * other subgroup */
  child = find_custom_widget("pref-test-bool");
  g_assert_null(frame_ancestor(child));
  child = frame_ancestor(find_custom_widget("pref-test-choice"));
  g_assert_nonnull(child);
  g_assert_cmpstr(gtk_frame_get_label(GTK_FRAME(child)), ==, "Pref test sub");
  g_assert_true(frame_ancestor(find_custom_widget("pref-test-table")) == child);
  child = gtk_widget_get_first_child(
    gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(page)));
  if (GTK_IS_VIEWPORT(child))
    child = gtk_widget_get_first_child(child);
  child = gtk_widget_get_first_child(child);
  g_assert_true(GTK_IS_LABEL(child));
  g_assert_cmpstr(gtk_label_get_text(GTK_LABEL(child)), ==, "Pref test");

  /* switching back shows the first page again, and the page of
   * pref-test is reused rather than rebuilt */
  {
    GtkTreeModel *model =
      gtk_tree_view_get_model(GTK_TREE_VIEW(pref_tree_view));
    g_assert_true(gtk_tree_model_get_iter_first(model, &first_iter));
    gtk_tree_model_get(model, &first_iter, GROUP_SYM, &first_sym, -1);
  }
  select_group(first_sym, NULL);
  g_free(first_sym);
  g_assert_true(current_group_widget == first_page);
  g_assert_true(gtk_widget_get_visible(first_page));
  g_assert_false(gtk_widget_get_visible(page));

  select_group("pref-test", &iter);
  g_assert_true(row_widget(&iter) == page);
  g_assert_true(gtk_widget_get_visible(page));
  g_assert_true(gtk_widget_get_parent(page) == pref_hbox);
}

static void
assert_saved(const char *expected)
{
  char *path = pref_test_custom_file("pref-test");
  char *contents = NULL;

  g_assert_true(g_file_get_contents(path, &contents, NULL, NULL));
  if (!strstr(contents, expected))
    g_error("\"%s\" is not in %s:\n%s", expected, path, contents);
  g_free(contents);
  g_free(path);
}

static gboolean
saved_file_exists(void)
{
  char *path = pref_test_custom_file("pref-test");
  gboolean exists = g_file_test(path, G_FILE_TEST_EXISTS);

  g_free(path);
  return exists;
}

static void
test_apply(gpointer fixture, gconstpointer data)
{
  GtkWidget *button;

  select_group("pref-test", NULL);
  button = find_custom_widget("pref-test-bool");

  /* Apply with nothing changed writes nothing */
  click("_Apply");
  g_assert_false(saved_file_exists());
  g_assert_cmpint(n_reload_requests(), ==, 0);

  gtk_check_button_set_active(GTK_CHECK_BUTTON(button), FALSE);
  g_assert_true(uim_pref_gtk_value_changed);
  g_assert_true(gtk_widget_get_sensitive(pref_apply_button));
  /* nothing is written before Apply */
  g_assert_false(saved_file_exists());

  click("_Apply");
  assert_saved("(define pref-test-bool #f)");
  /* running uim clients are told to reload */
  g_assert_cmpint(n_reload_requests(), ==, 1);
  g_assert_false(uim_pref_gtk_value_changed);
  g_assert_false(gtk_widget_get_sensitive(pref_apply_button));

  /* a later change is written by the next Apply */
  gtk_spin_button_set_value(
    GTK_SPIN_BUTTON(find_custom_widget("pref-test-int")),
    77);
  g_assert_true(gtk_widget_get_sensitive(pref_apply_button));
  click("_Apply");
  assert_saved("(define pref-test-int 77)");
}

static void
test_ok(gpointer fixture, gconstpointer data)
{
  select_group("pref-test", NULL);
  gtk_editable_set_text(GTK_EDITABLE(find_custom_widget("pref-test-str")),
                        "saved by OK");
  g_assert_true(uim_pref_gtk_value_changed);

  /* OK saves, then quits (there is no running application here) */
  click("_OK");
  assert_saved("(define pref-test-str \"saved by OK\")");
  g_assert_cmpint(n_reload_requests(), ==, 1);
  g_assert_false(uim_pref_gtk_value_changed);
}

static void
test_defaults(gpointer fixture, gconstpointer data)
{
  GtkWidget *spin, *entry, *combo;

  select_group("pref-test", NULL);
  spin = find_custom_widget("pref-test-int");
  entry = find_custom_widget("pref-test-str");
  combo = find_custom_widget("pref-test-choice");

  gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), 50);
  gtk_editable_set_text(GTK_EDITABLE(entry), "changed");
  gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
  click("_Apply");
  g_assert_false(gtk_widget_get_sensitive(pref_apply_button));

  /* Defaults resets every custom on the current page ... */
  click("_Defaults");
  pref_test_assert_value("pref-test-int", "10");
  pref_test_assert_value("pref-test-str", "\"hello\"");
  pref_test_assert_value("pref-test-choice", "'beta");
  g_assert_cmpint(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin)),
                  ==,
                  10);
  g_assert_cmpstr(gtk_editable_get_text(GTK_EDITABLE(entry)), ==, "hello");
  g_assert_cmpint(gtk_combo_box_get_active(GTK_COMBO_BOX(combo)), ==, 1);

  /* ... as a change that still has to be applied */
  g_assert_true(uim_pref_gtk_value_changed);
  g_assert_true(gtk_widget_get_sensitive(pref_apply_button));
  assert_saved("(define pref-test-int 50)");
  click("_Apply");
  assert_saved("(define pref-test-int 10)");
}

/* Escape, Cancel and closing the window quit at once when nothing has
 * changed, and ask first otherwise. */
static void
test_quit_confirmation(gpointer fixture, gconstpointer data)
{
  guint n = n_toplevels();

  g_assert_true(key_pressed_cb(NULL, GDK_KEY_Escape, 0, 0, NULL));
  g_assert_true(close_request_cb(GTK_WINDOW(pref_window), NULL));
  click("_Cancel");
  iterate_main_loop();
  g_assert_cmpuint(n_toplevels(), ==, n);

  /* other keys are left to the widgets */
  g_assert_false(key_pressed_cb(NULL, GDK_KEY_a, 0, 0, NULL));

  select_group("pref-test", NULL);
  gtk_check_button_set_active(
    GTK_CHECK_BUTTON(find_custom_widget("pref-test-bool")),
    FALSE);

  g_assert_true(key_pressed_cb(NULL, GDK_KEY_Escape, 0, 0, NULL));
  iterate_main_loop();
  /* the confirmation dialog is open and the window is still there */
  g_assert_cmpuint(n_toplevels(), ==, n + 1);
  g_assert_true(gtk_widget_get_visible(pref_window));
  /* closing the window must not bypass the confirmation either */
  g_assert_true(close_request_cb(GTK_WINDOW(pref_window), NULL));
}

static void
test_dot_uim_warning(gpointer fixture, gconstpointer data)
{
  char *dot_uim = g_build_filename(g_get_home_dir(), ".uim", NULL);
  guint n = n_toplevels();

  /* the test home has no ~/.uim */
  g_assert_cmpstr(g_get_home_dir(), ==, pref_test_home);
  g_assert_false(check_dot_uim_file(NULL));
  iterate_main_loop();
  g_assert_cmpuint(n_toplevels(), ==, n);

  g_assert_true(g_file_set_contents(dot_uim, "", -1, NULL));

  uim_scm_eval_c_string(
    "(define uim-pref-suppress-dot-uim-warning-dialog? #t)");
  g_assert_false(check_dot_uim_file(NULL));
  iterate_main_loop();
  g_assert_cmpuint(n_toplevels(), ==, n);

  uim_scm_eval_c_string(
    "(define uim-pref-suppress-dot-uim-warning-dialog? #f)");
  g_assert_false(check_dot_uim_file(NULL));
  iterate_main_loop();
  g_assert_cmpuint(n_toplevels(), ==, n + 1);

  g_unlink(dot_uim);
  g_free(dot_uim);
}

#define ADD_TEST(path, func)                                                   \
  g_test_add("/pref/window/" path,                                             \
             void,                                                             \
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
  pref_test_start_fake_helper();

  ADD_TEST("group-list", test_group_list);
  ADD_TEST("switch-group", test_switch_group);
  ADD_TEST("apply", test_apply);
  ADD_TEST("ok", test_ok);
  ADD_TEST("defaults", test_defaults);
  ADD_TEST("quit-confirmation", test_quit_confirmation);
  ADD_TEST("dot-uim-warning", test_dot_uim_warning);

  status = g_test_run();

  pref_test_quit_uim();
  return status;
}
