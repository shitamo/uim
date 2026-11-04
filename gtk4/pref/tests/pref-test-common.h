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
 * Shared fixture for the uim-pref-gtk4 tests.
 *
 * Each test program starts libuim with the build tree's Scheme files
 * (LIBUIM_SCM_FILES etc. come from meson's uim_build_tree_env and
 * LIBUIM_VANILLA keeps ~/.uim and the user's customs out), enables the
 * custom API and loads fixture.scm, which defines one custom variable
 * of every type in the 'pref-test group.
 *
 * libuim derives ~/.uim.d from getpwuid(), not $HOME, so uim_custom_save()
 * would write into the real home directory. To keep the tests
 * hermetic the Scheme procedure home-directory, which uim-custom.c
 * uses to build the path, is redefined to return a temporary
 * directory.
 */

#ifndef UIM_PREF_TEST_COMMON_H
#define UIM_PREF_TEST_COMMON_H

#include <glib.h>
#include <glib/gstdio.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "uim/uim.h"
#include "uim/uim-custom.h"
#include "uim/uim-scm.h"

#ifndef PREF_TEST_FIXTURE
#  error "PREF_TEST_FIXTURE must be defined to the path of fixture.scm"
#endif

/* The exit status meson and automake treat as "skipped". */
#define PREF_TEST_EXIT_SKIP 77

static char *pref_test_home = NULL;
/* listening socket of pref_test_start_fake_helper() */
static int pref_test_helper_fd = -1;

static void
pref_test_remove_tree(const char *path)
{
  if (g_file_test(path, G_FILE_TEST_IS_DIR) &&
      !g_file_test(path, G_FILE_TEST_IS_SYMLINK)) {
    GDir *dir = g_dir_open(path, 0, NULL);
    const char *name;

    if (dir) {
      while ((name = g_dir_read_name(dir))) {
        char *child = g_build_filename(path, name, NULL);
        pref_test_remove_tree(child);
        g_free(child);
      }
      g_dir_close(dir);
    }
    g_rmdir(path);
  } else {
    g_unlink(path);
  }
}

/* Must be called before gtk_init() so that GTK picks up the same
 * temporary $HOME and $XDG_RUNTIME_DIR. */
static void
pref_test_setup_environment(void)
{
  GError *error = NULL;

  pref_test_home = g_dir_make_tmp("uim-pref-test-XXXXXX", &error);
  g_assert_no_error(error);

  g_setenv("HOME", pref_test_home, TRUE);
  /* uim-helper-client looks for the helper socket here, so a running
   * uim-helper-server of the user is never reached; see also
   * pref_test_start_fake_helper(). */
  g_setenv("XDG_RUNTIME_DIR", pref_test_home, TRUE);
  g_setenv("XDG_CONFIG_HOME", pref_test_home, TRUE);
  g_setenv("XDG_DATA_HOME", pref_test_home, TRUE);
  g_setenv("XDG_CACHE_HOME", pref_test_home, TRUE);
  g_setenv("LIBUIM_VANILLA", "1", TRUE);
  g_setenv("UIM_DISABLE_NOTIFY", "1", TRUE);
}

static void
pref_test_init_uim(void)
{
  char *expr;

  g_assert_cmpint(uim_init(), ==, 0);
  g_assert_true(uim_custom_enable());

  expr = g_strdup_printf("(define home-directory (lambda (user) \"%s\"))",
                         pref_test_home);
  uim_scm_eval_c_string(expr);
  g_free(expr);

  g_assert_true(uim_scm_load_file(PREF_TEST_FIXTURE));
}

static void
pref_test_quit_uim(void)
{
  uim_quit();
  if (pref_test_helper_fd >= 0) {
    close(pref_test_helper_fd);
    pref_test_helper_fd = -1;
  }
  if (pref_test_home) {
    pref_test_remove_tree(pref_test_home);
    g_free(pref_test_home);
    pref_test_home = NULL;
  }
}

/* Report the whole program as skipped and clean up. Returns the exit
 * status for main(). The TAP plan is written with printf() because
 * g_print() output becomes a TAP comment under --tap. */
G_GNUC_UNUSED static int
pref_test_skip_all(const char *reason)
{
  printf("1..0 # SKIP %s\n", reason);
  fflush(stdout);
  pref_test_quit_uim();
  return 0;
}

/* The literal uim-custom would write for custom_sym's current value,
 * e.g. "#t", "10" or "'(\"<Control>j\" \"escape\")". */
G_GNUC_UNUSED static char *
pref_test_value_literal(const char *custom_sym)
{
  char *literal = uim_custom_value_as_literal(custom_sym);

  g_assert_nonnull(literal);
  return literal;
}

#define pref_test_assert_value(custom_sym, expected)                           \
  do {                                                                         \
    char *literal_ = pref_test_value_literal(custom_sym);                      \
    g_assert_cmpstr(literal_, ==, (expected));                                 \
    free(literal_);                                                            \
  } while (0)

/* Path of the file uim_custom_save() writes for group. */
G_GNUC_UNUSED static char *
pref_test_custom_file(const char *group)
{
  char *base = g_strdup_printf("custom-%s.scm", group);
  char *path =
    g_build_filename(pref_test_home, ".uim.d", "customs", base, NULL);

  g_free(base);
  return path;
}

/*
 * A stand-in for uim-helper-server. uim_custom_broadcast_reload_request()
 * (called by Apply and OK) connects to $XDG_RUNTIME_DIR/uim/socket/uim-helper
 * and, when nothing listens there, tries to start uim-helper-server and
 * retries for two seconds. Listening on that path makes the call return
 * at once and lets the tests see what was sent.
 */
G_GNUC_UNUSED static void
pref_test_start_fake_helper(void)
{
  struct sockaddr_un addr;
  char *dir, *path;

  dir = g_build_filename(pref_test_home, "uim", "socket", NULL);
  g_assert_cmpint(g_mkdir_with_parents(dir, 0700), ==, 0);
  path = g_build_filename(dir, "uim-helper", NULL);
  g_assert_cmpuint(strlen(path), <, sizeof(addr.sun_path));

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, path);

  pref_test_helper_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  g_assert_cmpint(pref_test_helper_fd, >=, 0);
  g_assert_cmpint(
    bind(pref_test_helper_fd, (struct sockaddr *)&addr, sizeof(addr)),
    ==,
    0);
  g_assert_cmpint(listen(pref_test_helper_fd, 64), ==, 0);
  g_assert_cmpint(fcntl(pref_test_helper_fd, F_SETFL, O_NONBLOCK), ==, 0);

  g_free(path);
  g_free(dir);
}

/* Everything clients have sent to the fake helper since the last call.
 * Clients write their message and close, so the data is already
 * buffered in each pending connection. */
G_GNUC_UNUSED static char *
pref_test_read_helper_messages(void)
{
  GString *messages = g_string_new("");
  int fd;

  g_assert_cmpint(pref_test_helper_fd, >=, 0);
  while ((fd = accept(pref_test_helper_fd, NULL, NULL)) >= 0) {
    char buf[256];
    ssize_t n;

    while ((n = read(fd, buf, sizeof(buf))) > 0)
      g_string_append_len(messages, buf, n);
    close(fd);
  }
  g_assert_true(errno == EAGAIN || errno == EWOULDBLOCK);

  return g_string_free(messages, FALSE);
}

#endif /* UIM_PREF_TEST_COMMON_H */
