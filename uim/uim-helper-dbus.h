/*

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

*/

/*
 * uim-helper-dbus.[ch]: D-Bus transport for the uim "helper bus" -- the
 * message bus historically implemented by uim-helper-server.c over a
 * per-user Unix domain socket at $XDG_RUNTIME_DIR/uim/socket/uim-helper.
 *
 * This is phase 1 of moving uim toward an ibus-style, D-Bus-activated
 * architecture. It replaces only the *transport* of the existing helper
 * bus protocol (newline/blank-line-terminated text messages, broadcast to
 * every other connected peer): the wire-level text protocol, and every
 * caller of uim_helper_send_message()/uim_helper_get_message() elsewhere
 * in the tree (candidate windows, toolbars, uim-pref, im modules, ...),
 * is untouched. See doc/dbus-helper-bus.md for the full design and the
 * roadmap for later phases (per-InputContext D-Bus objects replacing
 * uim-agent's own IPC, im modules talking D-Bus directly, etc).
 *
 * When built without D-Bus support (HAVE_DBUS undefined), every function
 * here is a harmless stub and uim transparently keeps using the legacy
 * socket transport.
 */

#ifndef UIM_HELPER_DBUS_H
#define UIM_HELPER_DBUS_H

#include "uim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Returns UIM_TRUE if this build has the D-Bus helper-bus transport
 * compiled in and the user has not forced the legacy transport with
 * UIM_HELPER_TRANSPORT=socket in the environment. */
uim_bool uim_helper_dbus_enabled(void);

/*
 * Client side (candidate window, toolbar, uim-pref, im modules, ...).
 * These mirror uim_helper_init_client_fd()/uim_helper_close_client_fd()/
 * uim_helper_read_proc(), but are backed by a private connection to the
 * D-Bus session bus instead of a Unix domain socket.
 *
 * The int returned/taken by these functions is the D-Bus connection's
 * underlying socket fd (see dbus_connection_get_unix_fd()), suitable for
 * select()/poll() so it drops into uim's existing fd-based event loops
 * unchanged -- but it must never be read(2)/write(2) directly. Use
 * uim_helper_dbus_send_message() and uim_helper_dbus_read_proc() instead;
 * uim-helper.c/uim-helper-client.c dispatch to these automatically for
 * any fd that uim_helper_dbus_init_client_fd() handed out, via
 * uim_helper_client_fd_is_dbus() below.
 */
int  uim_helper_dbus_init_client_fd(void (*disconnect_cb)(void));
void uim_helper_dbus_close_client_fd(int fd);
void uim_helper_dbus_send_message(int fd, const char *message);
void uim_helper_dbus_read_proc(int fd);

/* Pops one already-framed message (terminated by a blank line, exactly
 * like uim_helper_buffer_get_message()'s contract) out of the D-Bus
 * client's own incoming-message queue, or NULL if none is pending yet.
 * uim_helper_get_message() in uim-helper-client.c calls this instead of
 * uim_helper_buffer_get_message(uim_read_buf) whenever the D-Bus
 * transport is the active one. Kept entirely self-contained in this
 * file (its own static buffer, no dependency on uim-helper-client.c) so
 * that uim-helper-server -- which links this file but not
 * uim-helper-client.c -- has nothing left unresolved at link time. */
char *uim_helper_dbus_get_message(void);

/* UIM_TRUE if fd is the D-Bus pseudo-fd currently handed out by
 * uim_helper_dbus_init_client_fd(); used by uim-helper.c/
 * uim-helper-client.c to route uim_helper_send_message()/
 * uim_helper_close_client_fd()/uim_helper_read_proc()/
 * uim_helper_get_message() calls to the right transport without every
 * caller having to know which one is active. */
uim_bool uim_helper_client_fd_is_dbus(int fd);

/*
 * Server side (uim-helper-server). Owns the org.uim.HelperBus D-Bus
 * name on the session bus and re-broadcasts every message sent to it via
 * the "Send" method as a "Message" signal, mirroring the old
 * distribute_message()/accept loop in uim-helper-server.c. Blocks until
 * the bus connection is lost.
 *
 * Returns UIM_TRUE if the D-Bus path was fully handled (whether that
 * meant running the dispatch loop until disconnect, or discovering that
 * another instance already owns the bus name and there's nothing to do)
 * -- the caller should just exit(0) in that case. Returns UIM_FALSE only
 * when the session bus itself could not be reached at all, in which case
 * the caller should fall back to the legacy socket server.
 */
uim_bool uim_helper_dbus_server_run(void);

#ifdef __cplusplus
}
#endif
#endif /* UIM_HELPER_DBUS_H */
