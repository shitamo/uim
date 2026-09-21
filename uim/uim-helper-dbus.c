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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uim.h"
#include "uim-internal.h"
#include "uim-helper.h"
#include "uim-helper-dbus.h"

#if HAVE_DBUS

#include <dbus/dbus.h>

#define UIM_DBUS_BUS_NAME    "org.uim.HelperBus"
#define UIM_DBUS_OBJECT_PATH "/org/uim/HelperBus"
#define UIM_DBUS_INTERFACE   "org.uim.HelperBus1"
#define UIM_DBUS_METHOD_SEND "Send"
#define UIM_DBUS_SIGNAL_MSG  "Message"

/*
 * ---- client side -------------------------------------------------
 *
 * Exactly one connection per process, mirroring the single static
 * uim_fd already assumed by uim-helper-client.c. A private (not shared)
 * connection is used deliberately: a process embedding libuim (a GTK
 * immodule, say) may already hold its own shared session-bus connection
 * via GDBus, and we don't want to fight it over dispatch or fd
 * ownership.
 */

static DBusConnection *client_conn = NULL;
static void (*client_disconnect_cb)(void) = NULL;
static char *client_unique_name = NULL;
static int client_pseudo_fd = -1;
static char *client_incoming_buf = NULL;

static DBusHandlerResult
client_signal_filter(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
  DBusError err;
  const char *sender_id = NULL;
  const char *body = NULL;

  (void)conn;
  (void)user_data;

  if (!dbus_message_is_signal(msg, UIM_DBUS_INTERFACE, UIM_DBUS_SIGNAL_MSG))
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  dbus_error_init(&err);
  if (!dbus_message_get_args(msg, &err,
			      DBUS_TYPE_STRING, &sender_id,
			      DBUS_TYPE_STRING, &body,
			      DBUS_TYPE_INVALID)) {
    dbus_error_free(&err);
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  /* Don't echo our own broadcast back to ourselves -- the old
   * distribute_message() in uim-helper-server.c skipped the
   * originating fd for the same reason. */
  if (client_unique_name && sender_id && strcmp(sender_id, client_unique_name) == 0)
    return DBUS_HANDLER_RESULT_HANDLED;

  if (body) {
    if (!client_incoming_buf)
      client_incoming_buf = uim_strdup("");
    client_incoming_buf = uim_helper_buffer_append(client_incoming_buf, body, strlen(body));
  }

  return DBUS_HANDLER_RESULT_HANDLED;
}

char *
uim_helper_dbus_get_message(void)
{
  if (!client_incoming_buf)
    client_incoming_buf = uim_strdup("");

  return uim_helper_buffer_get_message(client_incoming_buf);
}

int
uim_helper_dbus_init_client_fd(void (*disconnect_cb)(void))
{
  DBusError err;
  char match_rule[256];
  int fd = -1;

  dbus_error_init(&err);

  client_conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
  if (!client_conn) {
    if (dbus_error_is_set(&err)) {
      fprintf(stderr, "uim-helper: failed to connect to session bus: %s\n",
	      err.message);
      dbus_error_free(&err);
    }
    return -1;
  }
  dbus_connection_set_exit_on_disconnect(client_conn, 0);

  /* This is the actual point of moving to D-Bus: if uim-helper-server
   * isn't running yet, dbus-daemon (optionally via systemd --user, see
   * data/dbus-1/services/org.uim.HelperBus.service) activates it with
   * an environment *it* manages, rather than whatever the first process
   * that happened to need the helper bus had inherited -- which is
   * exactly the class of problem that made e.g. xdg-desktop-portal-gtk's
   * file picker unable to see GTK_IM_MODULE=uim. */
  dbus_bus_start_service_by_name(client_conn, UIM_DBUS_BUS_NAME, 0, NULL, &err);
  if (dbus_error_is_set(&err))
    dbus_error_free(&err);

  if (!dbus_connection_get_unix_fd(client_conn, &fd) || fd < 0) {
    dbus_connection_close(client_conn);
    dbus_connection_unref(client_conn);
    client_conn = NULL;
    return -1;
  }

  snprintf(match_rule, sizeof(match_rule),
	   "type='signal',interface='%s',member='%s',path='%s'",
	   UIM_DBUS_INTERFACE, UIM_DBUS_SIGNAL_MSG, UIM_DBUS_OBJECT_PATH);
  dbus_bus_add_match(client_conn, match_rule, &err);
  if (dbus_error_is_set(&err)) {
    fprintf(stderr, "uim-helper: dbus_bus_add_match failed: %s\n", err.message);
    dbus_error_free(&err);
  }

  dbus_connection_add_filter(client_conn, client_signal_filter, NULL, NULL);

  free(client_unique_name);
  client_unique_name = uim_strdup(dbus_bus_get_unique_name(client_conn));

  client_disconnect_cb = disconnect_cb;
  client_pseudo_fd = fd;

  return fd;
}

void
uim_helper_dbus_close_client_fd(int fd)
{
  void (*cb)(void) = client_disconnect_cb;

  (void)fd;

  if (client_conn) {
    dbus_connection_close(client_conn);
    dbus_connection_unref(client_conn);
    client_conn = NULL;
  }

  free(client_unique_name);
  client_unique_name = NULL;
  client_disconnect_cb = NULL;
  client_pseudo_fd = -1;

  if (cb)
    cb();
}

void
uim_helper_dbus_send_message(int fd, const char *message)
{
  DBusMessage *m;
  char *terminated;
  const char *sender;

  (void)fd;

  if (!client_conn || !message)
    return;

  /* Preserve the historical wire format: every message is terminated by
   * a blank line, which is how uim_helper_buffer_get_message() finds
   * message boundaries. We reuse that same framing (and that same
   * parser, via uim_helper_client_queue_incoming_message()) on the
   * receiving end instead of inventing a new one. */
  uim_asprintf(&terminated, "%s\n", message);

  m = dbus_message_new_method_call(UIM_DBUS_BUS_NAME, UIM_DBUS_OBJECT_PATH,
				    UIM_DBUS_INTERFACE, UIM_DBUS_METHOD_SEND);
  if (m) {
    sender = client_unique_name ? client_unique_name : "";
    dbus_message_append_args(m,
			      DBUS_TYPE_STRING, &sender,
			      DBUS_TYPE_STRING, &terminated,
			      DBUS_TYPE_INVALID);
    dbus_message_set_no_reply(m, 1);
    dbus_connection_send(client_conn, m, NULL);
    dbus_message_unref(m);
  }

  free(terminated);
}

void
uim_helper_dbus_read_proc(int fd)
{
  if (!client_conn)
    return;

  /* Non-blocking pump: read anything newly arrived on the wire and hand
   * it to client_signal_filter() above, which queues it for
   * uim_helper_get_message() exactly like the socket transport did. */
  dbus_connection_read_write_dispatch(client_conn, 0);
  while (dbus_connection_get_dispatch_status(client_conn) == DBUS_DISPATCH_DATA_REMAINS)
    dbus_connection_dispatch(client_conn);

  if (!dbus_connection_get_is_connected(client_conn))
    uim_helper_dbus_close_client_fd(fd);
}

uim_bool
uim_helper_client_fd_is_dbus(int fd)
{
  return (client_pseudo_fd != -1 && fd == client_pseudo_fd) ? UIM_TRUE : UIM_FALSE;
}

uim_bool
uim_helper_dbus_enabled(void)
{
  const char *transport = getenv("UIM_HELPER_TRANSPORT");

  if (transport && strcmp(transport, "socket") == 0)
    return UIM_FALSE;

  return UIM_TRUE;
}


/*
 * ---- server side ---------------------------------------------------
 *
 * Replaces the accept()/select() loop in uim-helper-server.c's main().
 * Owns the org.uim.HelperBus name and re-broadcasts every "Send" method
 * call as a "Message" signal -- the D-Bus session bus itself fans that
 * signal out to every client that added the match rule above, which is
 * the same broadcast-to-everyone-but-the-sender behavior
 * distribute_message() implemented by hand over raw sockets.
 */

static DBusHandlerResult
server_message_handler(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
  DBusError err;
  const char *sender_id = NULL;
  const char *body = NULL;
  DBusMessage *sig;

  (void)user_data;

  if (!dbus_message_is_method_call(msg, UIM_DBUS_INTERFACE, UIM_DBUS_METHOD_SEND))
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  dbus_error_init(&err);
  if (dbus_message_get_args(msg, &err,
			     DBUS_TYPE_STRING, &sender_id,
			     DBUS_TYPE_STRING, &body,
			     DBUS_TYPE_INVALID)) {
    sig = dbus_message_new_signal(UIM_DBUS_OBJECT_PATH, UIM_DBUS_INTERFACE,
				   UIM_DBUS_SIGNAL_MSG);
    if (sig) {
      dbus_message_append_args(sig,
				DBUS_TYPE_STRING, &sender_id,
				DBUS_TYPE_STRING, &body,
				DBUS_TYPE_INVALID);
      /* No destination set: this broadcasts to every peer on the bus
       * that matched the signal rule, not just one recipient. */
      dbus_connection_send(conn, sig, NULL);
      dbus_message_unref(sig);
    }
  } else {
    dbus_error_free(&err);
  }

  return DBUS_HANDLER_RESULT_HANDLED;
}

uim_bool
uim_helper_dbus_server_run(void)
{
  DBusConnection *conn;
  DBusError err;
  int ret;

  dbus_error_init(&err);
  conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
  if (!conn) {
    if (dbus_error_is_set(&err)) {
      fprintf(stderr, "uim-helper-server: dbus_bus_get failed: %s\n", err.message);
      dbus_error_free(&err);
    }
    return UIM_FALSE;
  }

  ret = dbus_bus_request_name(conn, UIM_DBUS_BUS_NAME,
			       DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
  if (dbus_error_is_set(&err)) {
    fprintf(stderr, "uim-helper-server: dbus_bus_request_name failed: %s\n",
	    err.message);
    dbus_error_free(&err);
    dbus_connection_unref(conn);
    return UIM_TRUE;
  }

  if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
    /* Another uim-helper-server already owns the name for this
     * session -- exactly one instance per user session, the same
     * invariant the socket server enforced by binding a single
     * well-known socket path. Nothing left to do. */
    dbus_connection_unref(conn);
    return UIM_TRUE;
  }

  dbus_connection_add_filter(conn, server_message_handler, NULL, NULL);

  while (dbus_connection_read_write_dispatch(conn, -1))
    ; /* block until there's something to do; exits when disconnected */

  dbus_connection_unref(conn);

  return UIM_TRUE;
}

#else /* !HAVE_DBUS */

uim_bool
uim_helper_dbus_enabled(void)
{
  return UIM_FALSE;
}

int
uim_helper_dbus_init_client_fd(void (*disconnect_cb)(void))
{
  (void)disconnect_cb;
  return -1;
}

void
uim_helper_dbus_close_client_fd(int fd)
{
  (void)fd;
}

void
uim_helper_dbus_send_message(int fd, const char *message)
{
  (void)fd;
  (void)message;
}

void
uim_helper_dbus_read_proc(int fd)
{
  (void)fd;
}

char *
uim_helper_dbus_get_message(void)
{
  return NULL;
}

uim_bool
uim_helper_client_fd_is_dbus(int fd)
{
  (void)fd;
  return UIM_FALSE;
}

uim_bool
uim_helper_dbus_server_run(void)
{
  return UIM_FALSE;
}

#endif /* HAVE_DBUS */
