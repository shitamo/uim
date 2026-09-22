/*

  standalone-gtk4.c: standalone floating shell for the GTK4 uim toolbar

  Copyright (c) 2003-2026 uim Project https://github.com/uim/uim

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

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.


  Positioning note
  -----------------
  GTK 4 dropped gtk_window_move()/gtk_window_get_position(): a portable
  toolkit-level API to place a plain toplevel at an exact screen
  position no longer exists, because Wayland compositors generally do
  not let a client place itself. This shell therefore:

    * on Wayland, anchors itself to a screen corner with the
      gtk4-layer-shell protocol when that optional library is
      available (HAVE_GTK4_LAYER_SHELL) -- this is the same mechanism
      panels/docks use, and it is what lets the toolbar float above
      normal windows and survive workspace switches;
    * on X11, falls back to moving its own top-level surface directly
      via Xlib (GDK_WINDOWING_X11), which still works because X11
      itself (unlike the Wayland protocol) has always allowed a
      client to reposition its own window;
    * otherwise, simply lets the window manager place a normal,
      decorated toplevel, which is the safe and portable degradation.

  Dragging the bar with the primary mouse button uses the same
  X11-only path; on a pure-Wayland session without layer-shell the
  window is still fully usable, just not draggable/anchored.
*/

#include <config.h>

#include <locale.h>

#include <gtk/gtk.h>
#include <uim/gettext.h>
#include <uim/uim.h>

#include "uim-toolbar-widget.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

#ifdef HAVE_GTK4_LAYER_SHELL
#include <gtk4-layer-shell.h>
#endif

typedef struct {
  GtkWidget *window;
  gboolean dragging;
  double drag_start_win_x, drag_start_win_y;
  double drag_start_ptr_x, drag_start_ptr_y;
} StandaloneShell;

#if defined(GDK_WINDOWING_X11)
static gboolean
get_x11_window_origin(GtkWidget *window, int *x, int *y)
{
  GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(window));
  Display *xdisplay;
  Window xwindow, root, child;
  int rx, ry, wx, wy;
  unsigned int mask;

  if (!surface || !GDK_IS_X11_SURFACE(surface))
    return FALSE;

  xdisplay = GDK_SURFACE_XDISPLAY(surface);
  xwindow = GDK_SURFACE_XID(surface);
  if (!XQueryPointer(xdisplay, xwindow, &root, &child,
                     &rx, &ry, &wx, &wy, &mask))
    return FALSE;

  *x = rx - wx;
  *y = ry - wy;
  return TRUE;
}

static void
move_x11_window(GtkWidget *window, int x, int y)
{
  GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(window));

  if (!surface || !GDK_IS_X11_SURFACE(surface))
    return;

  XMoveWindow(GDK_SURFACE_XDISPLAY(surface), GDK_SURFACE_XID(surface), x, y);
}
#endif

static void
drag_begin_cb(GtkGestureDrag *gesture, double start_x, double start_y,
             gpointer data)
{
#if defined(GDK_WINDOWING_X11)
  StandaloneShell *shell = data;
  int wx = 0, wy = 0;

  get_x11_window_origin(shell->window, &wx, &wy);
  shell->drag_start_win_x = wx;
  shell->drag_start_win_y = wy;
  shell->drag_start_ptr_x = start_x;
  shell->drag_start_ptr_y = start_y;
  shell->dragging = TRUE;
#endif
}

static void
drag_update_cb(GtkGestureDrag *gesture, double offset_x, double offset_y,
              gpointer data)
{
#if defined(GDK_WINDOWING_X11)
  StandaloneShell *shell = data;

  if (!shell->dragging)
    return;

  move_x11_window(shell->window,
                  (int)(shell->drag_start_win_x + offset_x),
                  (int)(shell->drag_start_win_y + offset_y));
#endif
}

static void
drag_end_cb(GtkGestureDrag *gesture, double offset_x, double offset_y,
           gpointer data)
{
  StandaloneShell *shell = data;

  shell->dragging = FALSE;
}

static void
activate_cb(GtkApplication *app, gpointer user_data)
{
  StandaloneShell *shell = g_new0(StandaloneShell, 1);
  GtkWidget *window;
  GtkWidget *frame;
  GtkWidget *toolbar;
  GtkGesture *drag;

  window = gtk_application_window_new(app);
  shell->window = window;
  gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
  gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

#ifdef HAVE_GTK4_LAYER_SHELL
  if (gtk_layer_is_supported()) {
    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_auto_exclusive_zone_enable(GTK_WINDOW(window));
  }
#endif

  frame = gtk_frame_new(NULL);
  gtk_window_set_child(GTK_WINDOW(window), frame);

  toolbar = uim_toolbar_new(UIM_TOOLBAR_KIND_STANDALONE);
  uim_toolbar_set_host_window(UIM_TOOLBAR(toolbar), GTK_WINDOW(window));
  gtk_frame_set_child(GTK_FRAME(frame), toolbar);

  drag = gtk_gesture_drag_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_PRIMARY);
  g_signal_connect(drag, "drag-begin", G_CALLBACK(drag_begin_cb), shell);
  g_signal_connect(drag, "drag-update", G_CALLBACK(drag_update_cb), shell);
  g_signal_connect(drag, "drag-end", G_CALLBACK(drag_end_cb), shell);
  gtk_widget_add_controller(frame, GTK_EVENT_CONTROLLER(drag));

  g_object_set_data_full(G_OBJECT(window), "uim-shell", shell, g_free);

  gtk_window_present(GTK_WINDOW(window));
}

int
main(int argc, char *argv[])
{
  GtkApplication *app;
  int status;

  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
  bind_textdomain_codeset(PACKAGE, "UTF-8");

  uim_init();

  app = gtk_application_new("org.uim.ToolbarGtk4",
                            G_APPLICATION_NON_UNIQUE);
  g_signal_connect(app, "activate", G_CALLBACK(activate_cb), NULL);
  status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);

  uim_quit();

  return status;
}
