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
 * GTK tests for key handling in uim-pref-gtk4 with real key events:
 * im_uim_convert_keyevent() in key-util-gtk4.c, and the "Key:" entry and
 * "Grab..." window of the key editor in gtk-custom-widgets-gtk4.c.
 *
 * GTK 4 has no public API to make a GdkEvent, so the key events are
 * injected into the X server with the XTest extension. The program
 * therefore needs an X11 display with XTest, such as the Xvfb that
 * meson starts with xvfb-run; it reports itself as skipped otherwise.
 */

#include <config.h>

#include "pref-test-common.h"

#include "../gtk-custom-widgets-gtk4.c"

#include <gdk/x11/gdkx.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

/* Normally defined in gtk-gtk4.c, which owns the Apply button. */
gboolean uim_pref_gtk_value_changed = FALSE;

void
uim_pref_gtk_mark_value_changed(void)
{
  uim_pref_gtk_value_changed = TRUE;
}

static Display *xdisplay;

static gboolean
timeout_cb(gpointer data)
{
  *(gboolean *)data = TRUE;
  return G_SOURCE_REMOVE;
}

/* Run the main loop until *flag is set, for at most two seconds. */
static gboolean
wait_for(const gboolean *flag)
{
  gboolean timed_out = FALSE;
  guint id = g_timeout_add(2000, timeout_cb, &timed_out);

  while (!*flag && !timed_out)
    g_main_context_iteration(NULL, TRUE);
  if (!timed_out)
    g_source_remove(id);
  return *flag;
}

static void
notify_flag_cb(GObject *object, GParamSpec *pspec, gpointer data)
{
  *(gboolean *)data = TRUE;
}

/* Give window the X input focus (there is no window manager) and wait
 * until GTK has noticed. */
static void
focus_window(GtkWidget *window)
{
  GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(window));
  gboolean active = gtk_window_is_active(GTK_WINDOW(window));
  gulong id;

  g_assert_nonnull(surface);
  id = g_signal_connect(window,
                        "notify::is-active",
                        G_CALLBACK(notify_flag_cb),
                        &active);
  XSetInputFocus(xdisplay,
                 gdk_x11_surface_get_xid(surface),
                 RevertToPointerRoot,
                 CurrentTime);
  XSync(xdisplay, False);
  g_assert_true(wait_for(&active));
  g_signal_handler_disconnect(window, id);
}

static void
fake_key(KeySym keysym, gboolean press)
{
  KeyCode keycode = XKeysymToKeycode(xdisplay, keysym);

  g_assert_cmpuint(keycode, !=, 0);
  XTestFakeKeyEvent(xdisplay, keycode, press, CurrentTime);
  XSync(xdisplay, False);
}

/*
 * im_uim_convert_keyevent()
 */

typedef struct
{
  GtkWidget *window;
  int n_events;
  int ukey;
  int umod;
} ConvertFixture;

static gboolean
convert_key_cb(GtkEventControllerKey *controller,
               guint keyval,
               guint keycode,
               GdkModifierType state,
               ConvertFixture *fixture)
{
  GdkEvent *event =
    gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));

  im_uim_convert_keyevent(event, &fixture->ukey, &fixture->umod);
  fixture->n_events++;
  return TRUE;
}

static void
convert_set_up(ConvertFixture *fixture, gconstpointer data)
{
  GtkEventController *controller = gtk_event_controller_key_new();

  fixture->window = gtk_window_new();
  gtk_window_set_default_size(GTK_WINDOW(fixture->window), 200, 100);
  g_signal_connect(controller,
                   "key-pressed",
                   G_CALLBACK(convert_key_cb),
                   fixture);
  g_signal_connect(controller,
                   "key-released",
                   G_CALLBACK(convert_key_cb),
                   fixture);
  gtk_widget_add_controller(fixture->window, controller);
  gtk_window_present(GTK_WINDOW(fixture->window));
  focus_window(fixture->window);
}

static void
convert_tear_down(ConvertFixture *fixture, gconstpointer data)
{
  gtk_window_destroy(GTK_WINDOW(fixture->window));
  while (g_main_context_iteration(NULL, FALSE))
    ;
}

/* Send one key press or release and return its conversion. */
static void
convert(
  ConvertFixture *fixture, KeySym keysym, gboolean press, int *ukey, int *umod)
{
  int n = fixture->n_events;
  gboolean done = FALSE;
  guint id;

  fake_key(keysym, press);
  id = g_timeout_add(2000, timeout_cb, &done);
  while (fixture->n_events == n && !done)
    g_main_context_iteration(NULL, TRUE);
  if (!done)
    g_source_remove(id);
  if (fixture->n_events == n)
    g_error("no key event for keysym 0x%lx arrived", (unsigned long)keysym);
  *ukey = fixture->ukey;
  *umod = fixture->umod;
}

/* Press and release keysym while the given modifier keys are held,
 * and return the conversion of the press. */
static void
convert_with(ConvertFixture *fixture,
             const KeySym *modifiers,
             KeySym keysym,
             int *ukey,
             int *umod)
{
  const KeySym *mod;
  int ignored_key, ignored_mod;

  for (mod = modifiers; mod && *mod; mod++)
    convert(fixture, *mod, TRUE, &ignored_key, &ignored_mod);
  convert(fixture, keysym, TRUE, ukey, umod);
  convert(fixture, keysym, FALSE, &ignored_key, &ignored_mod);
  for (mod = modifiers; mod && *mod; mod++)
    convert(fixture, *mod, FALSE, &ignored_key, &ignored_mod);
}

static void
test_convert_plain_keys(ConvertFixture *fixture, gconstpointer data)
{
  static const struct
  {
    KeySym keysym;
    int ukey;
  } cases[] = {
    {XK_a, 'a'},
    {XK_1, '1'},
    {XK_space, ' '},
    {XK_slash, '/'},
    {XK_Escape, UKey_Escape},
    {XK_Return, UKey_Return},
    {XK_BackSpace, UKey_Backspace},
    {XK_Delete, UKey_Delete},
    {XK_Tab, UKey_Tab},
    {XK_Left, UKey_Left},
    {XK_Down, UKey_Down},
    {XK_Prior, UKey_Prior},
    {XK_Home, UKey_Home},
    {XK_F1, UKey_F1},
    {XK_F12, UKey_F12},
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS(cases); i++) {
    int ukey, umod;

    convert_with(fixture, NULL, cases[i].keysym, &ukey, &umod);
    g_assert_cmpint(ukey, ==, cases[i].ukey);
    g_assert_cmpint(umod, ==, 0);
  }
}

static void
test_convert_modifiers(ConvertFixture *fixture, gconstpointer data)
{
  static const KeySym shift[] = {XK_Shift_L, 0};
  static const KeySym control[] = {XK_Control_L, 0};
  static const KeySym alt[] = {XK_Alt_L, 0};
  static const KeySym control_shift[] = {XK_Control_L, XK_Shift_L, 0};
  int ukey, umod;

  /* the modifier keys themselves */
  convert(fixture, XK_Shift_L, TRUE, &ukey, &umod);
  g_assert_cmpint(ukey, ==, UKey_Shift_key);
  convert(fixture, XK_Shift_L, FALSE, &ukey, &umod);
  g_assert_cmpint(ukey, ==, UKey_Shift_key);
  g_assert_cmpint(umod, ==, UMod_Shift);
  convert(fixture, XK_Control_L, TRUE, &ukey, &umod);
  g_assert_cmpint(ukey, ==, UKey_Control_key);
  convert(fixture, XK_Control_L, FALSE, &ukey, &umod);
  convert(fixture, XK_Alt_L, TRUE, &ukey, &umod);
  g_assert_cmpint(ukey, ==, UKey_Alt_key);
  convert(fixture, XK_Alt_L, FALSE, &ukey, &umod);

  /* Shift gives the shifted keysym and UMod_Shift */
  convert_with(fixture, shift, XK_a, &ukey, &umod);
  g_assert_cmpint(ukey, ==, 'A');
  g_assert_cmpint(umod, ==, UMod_Shift);
  convert_with(fixture, shift, XK_1, &ukey, &umod);
  g_assert_cmpint(ukey, ==, '!');
  g_assert_cmpint(umod, ==, UMod_Shift);

  convert_with(fixture, control, XK_j, &ukey, &umod);
  g_assert_cmpint(ukey, ==, 'j');
  g_assert_cmpint(umod, ==, UMod_Control);

  convert_with(fixture, control_shift, XK_F1, &ukey, &umod);
  g_assert_cmpint(ukey, ==, UKey_F1);
  g_assert_cmpint(umod, ==, UMod_Control | UMod_Shift);

  /* On X11 Mod1..Mod5 are mapped through the modifier keys that are
   * actually held, so Alt_L down + Mod1 gives UMod_Alt. */
  convert_with(fixture, alt, XK_x, &ukey, &umod);
  g_assert_cmpint(ukey, ==, 'x');
  g_assert_cmpint(umod, ==, UMod_Alt);

  /* nothing is left over once every key is up */
  convert_with(fixture, NULL, XK_x, &ukey, &umod);
  g_assert_cmpint(umod, ==, 0);
}

/*
 * The key editor
 */

typedef struct
{
  GtkWidget *window;
  GtkWidget *key_entry;
} EditorFixture;

static void
editor_set_up(EditorFixture *fixture, gconstpointer data)
{
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  uim_scm_eval_c_string(
    "(custom-set-value! 'pref-test-key (custom-default-value 'pref-test-key))");

  fixture->window = gtk_window_new();
  gtk_window_set_child(GTK_WINDOW(fixture->window), vbox);
  uim_pref_gtk_add_custom(vbox, "pref-test-key");
  fixture->key_entry = gtk_widget_get_next_sibling(
    gtk_widget_get_first_child(gtk_widget_get_first_child(vbox)));
  g_assert_true(GTK_IS_ENTRY(fixture->key_entry));
  gtk_window_present(GTK_WINDOW(fixture->window));

  /* "Edit..." */
  g_signal_emit_by_name(gtk_widget_get_next_sibling(fixture->key_entry),
                        "clicked");
  g_assert_true(GTK_IS_DIALOG(key_pref_win.window));
}

static void
editor_tear_down(EditorFixture *fixture, gconstpointer data)
{
  GListModel *toplevels = gtk_window_get_toplevels();

  uim_custom_cb_remove(NULL);
  while (g_list_model_get_n_items(toplevels) > 0) {
    GtkWindow *window = g_list_model_get_item(toplevels, 0);

    gtk_window_destroy(window);
    g_object_unref(window);
  }
  while (g_main_context_iteration(NULL, FALSE))
    ;
  memset(&key_pref_win, 0, sizeof(key_pref_win));
}

static gboolean entry_changed;

static void
entry_changed_cb(GtkEditable *editable, gpointer data)
{
  entry_changed = TRUE;
}

/* Typing a key combination into the "Key:" entry fills it with the
 * matching key literal. */
static void
test_key_entry(EditorFixture *fixture, gconstpointer data)
{
  static const struct
  {
    KeySym modifier;
    KeySym keysym;
    const char *literal;
  } cases[] = {
    {XK_Control_L, XK_j, "<Control>j"},
    {0, XK_Escape, "escape"},
    {XK_Shift_L, XK_a, "A"},
    {XK_Shift_L, XK_Return, "<Shift>return"},
    {XK_Alt_L, XK_F5, "<Alt>F5"},
    {0, XK_space, "space"},
  };
  GtkWidget *entry = key_pref_win.keycode_entry;
  guint i;

  g_signal_connect(entry, "changed", G_CALLBACK(entry_changed_cb), NULL);
  focus_window(key_pref_win.window);
  gtk_widget_grab_focus(entry);

  for (i = 0; i < G_N_ELEMENTS(cases); i++) {
    gtk_editable_set_text(GTK_EDITABLE(entry), "");
    entry_changed = FALSE;
    if (cases[i].modifier)
      fake_key(cases[i].modifier, TRUE);
    fake_key(cases[i].keysym, TRUE);
    fake_key(cases[i].keysym, FALSE);
    if (cases[i].modifier)
      fake_key(cases[i].modifier, FALSE);
    g_assert_true(wait_for(&entry_changed));
    /* let the remaining events through */
    while (g_main_context_iteration(NULL, FALSE))
      ;
    g_assert_cmpstr(gtk_editable_get_text(GTK_EDITABLE(entry)),
                    ==,
                    cases[i].literal);
  }

  /* the grabbed key can be added as is; uim-custom stores "space" as
   * " " */
  g_signal_emit_by_name(key_pref_win.add_button, "clicked");
  pref_test_assert_value("pref-test-key",
                         "'(\"<IgnoreCase><Control>j\" \"escape\" \" \")");
}

static GtkWidget *
find_toplevel_with_title(const char *title)
{
  GListModel *toplevels = gtk_window_get_toplevels();
  guint i;

  for (i = 0; i < g_list_model_get_n_items(toplevels); i++) {
    GtkWidget *window = g_list_model_get_item(toplevels, i);

    g_object_unref(window);
    if (!g_strcmp0(gtk_window_get_title(GTK_WINDOW(window)), title))
      return window;
  }
  return NULL;
}

static GtkWidget *
find_button(GtkWidget *widget, const char *label)
{
  GtkWidget *child;

  if (GTK_IS_BUTTON(widget) &&
      !g_strcmp0(gtk_button_get_label(GTK_BUTTON(widget)), label))
    return widget;
  for (child = gtk_widget_get_first_child(widget); child;
       child = gtk_widget_get_next_sibling(child)) {
    GtkWidget *found = find_button(child, label);
    if (found)
      return found;
  }
  return NULL;
}

static void
destroyed_cb(GtkWidget *widget, gpointer data)
{
  *(gboolean *)data = TRUE;
}

/* "Grab..." opens a window that takes the key combination pressed
 * while it is focused, and closes when the key is released. */
static void
test_grab_window(EditorFixture *fixture, gconstpointer data)
{
  GtkWidget *grab_window;
  gboolean destroyed = FALSE;

  g_signal_emit_by_name(find_button(key_pref_win.window, "Grab..."), "clicked");
  grab_window = find_toplevel_with_title("Grabbing keys");
  g_assert_nonnull(grab_window);
  g_assert_true(gtk_window_get_modal(GTK_WINDOW(grab_window)));
  g_assert_true(gtk_window_get_transient_for(GTK_WINDOW(grab_window)) ==
                GTK_WINDOW(key_pref_win.window));
  g_signal_connect(grab_window,
                   "destroy",
                   G_CALLBACK(destroyed_cb),
                   &destroyed);
  focus_window(grab_window);

  fake_key(XK_Control_L, TRUE);
  fake_key(XK_Shift_L, TRUE);
  fake_key(XK_Tab, TRUE);
  fake_key(XK_Tab, FALSE);
  g_assert_true(wait_for(&destroyed));
  fake_key(XK_Shift_L, FALSE);
  fake_key(XK_Control_L, FALSE);
  while (g_main_context_iteration(NULL, FALSE))
    ;

  g_assert_cmpstr(
    gtk_editable_get_text(GTK_EDITABLE(key_pref_win.keycode_entry)),
    ==,
    "<Shift><Control>tab");
  g_assert_null(find_toplevel_with_title("Grabbing keys"));
}

int
main(int argc, char *argv[])
{
  GdkDisplay *display;
  int event_base, error_base, major, minor;
  int status;

  g_test_init(&argc, &argv, NULL);
  pref_test_setup_environment();

  if (!gtk_init_check())
    return pref_test_skip_all("no display is available");
  display = gdk_display_get_default();
  if (!GDK_IS_X11_DISPLAY(display))
    return pref_test_skip_all("the display is not X11");
  xdisplay = gdk_x11_display_get_xdisplay(display);
  if (!XTestQueryExtension(xdisplay, &event_base, &error_base, &major, &minor))
    return pref_test_skip_all("the X server has no XTest extension");

  pref_test_init_uim();
  /* as activate_cb() in gtk-gtk4.c does */
  im_uim_init_modifier_keys();

  g_test_add("/pref/keyevent/convert-plain-keys",
             ConvertFixture,
             NULL,
             convert_set_up,
             test_convert_plain_keys,
             convert_tear_down);
  g_test_add("/pref/keyevent/convert-modifiers",
             ConvertFixture,
             NULL,
             convert_set_up,
             test_convert_modifiers,
             convert_tear_down);
  g_test_add("/pref/keyevent/key-entry",
             EditorFixture,
             NULL,
             editor_set_up,
             test_key_entry,
             editor_tear_down);
  g_test_add("/pref/keyevent/grab-window",
             EditorFixture,
             NULL,
             editor_set_up,
             test_grab_window,
             editor_tear_down);

  status = g_test_run();

  pref_test_quit_uim();
  return status;
}
