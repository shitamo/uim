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

#include <config.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "test-dict-common.h"

#ifdef USE_ANTHY
/* Not declared in the Anthy headers, but exported by libanthy/libanthydic. */
void
anthy_conf_override(const char *var, const char *val);
const char *
anthy_conf_get_str(const char *var);
#endif

static gchar *test_home = NULL;

static void
remove_recursively(const gchar *path)
{
  if (g_file_test(path, G_FILE_TEST_IS_DIR) &&
      !g_file_test(path, G_FILE_TEST_IS_SYMLINK)) {
    GDir *dir = g_dir_open(path, 0, NULL);
    const gchar *name;

    if (dir) {
      while ((name = g_dir_read_name(dir)) != NULL) {
        gchar *child = g_build_filename(path, name, NULL);
        remove_recursively(child);
        g_free(child);
      }
      g_dir_close(dir);
    }
  }
  g_remove(path);
}

const gchar *
test_dict_setup_home(void)
{
  GError *error = NULL;
  gchar *dir;

  if (test_home)
    return test_home;

  test_home = g_dir_make_tmp("uim-dict-test-XXXXXX", &error);
  g_assert_no_error(error);
  g_assert_nonnull(test_home);

  g_setenv("HOME", test_home, TRUE);

  dir = g_build_filename(test_home, ".config", NULL);
  g_mkdir_with_parents(dir, 0700);
  g_setenv("XDG_CONFIG_HOME", dir, TRUE);
  g_free(dir);

  dir = g_build_filename(test_home, ".local", "share", NULL);
  g_mkdir_with_parents(dir, 0700);
  g_setenv("XDG_DATA_HOME", dir, TRUE);
  g_free(dir);

  dir = g_build_filename(test_home, ".cache", NULL);
  g_mkdir_with_parents(dir, 0700);
  g_setenv("XDG_CACHE_HOME", dir, TRUE);
  g_free(dir);

  dir = g_build_filename(test_home, "runtime", NULL);
  g_mkdir_with_parents(dir, 0700);
  g_setenv("XDG_RUNTIME_DIR", dir, TRUE);
  g_free(dir);

  return test_home;
}

void
test_dict_cleanup_home(void)
{
  if (!test_home)
    return;

  remove_recursively(test_home);
  g_free(test_home);
  test_home = NULL;
}

gchar *
test_dict_eucjp(const gchar *utf8)
{
  GError *error = NULL;
  gchar *eucjp = g_convert(utf8, -1, "EUC-JP", "UTF-8", NULL, NULL, &error);

  g_assert_no_error(error);
  return eucjp;
}

guint
test_dict_word_list_length(uim_word *list)
{
  guint n = 0;

  for (; list; list = list->next)
    n++;
  return n;
}

uim_word *
test_dict_word_list_find(uim_word *list, const gchar *phon, const gchar *desc)
{
  for (; list; list = list->next) {
    if (!g_strcmp0(list->phon, phon) && !g_strcmp0(list->desc, desc))
      return list;
  }
  return NULL;
}

#ifdef USE_ANTHY
static gchar *
anthy_private_dic_path(void)
{
  return g_build_filename(test_home, ".anthy", "private_words_default", NULL);
}

gboolean
test_dict_anthy_setup(const gchar *contents)
{
  static gboolean overridden = FALSE;
  GError *error = NULL;
  gchar *anthy_dir, *path;
  const char *anthy_home;

  test_dict_setup_home();

  if (!overridden) {
    anthy_conf_override("HOME", test_home);
    overridden = TRUE;
  }
  anthy_home = anthy_conf_get_str("HOME");
  if (g_strcmp0(anthy_home, test_home) != 0) {
    g_test_skip("cannot redirect Anthy's private dictionary to a "
                "temporary directory");
    return FALSE;
  }

  anthy_dir = g_build_filename(test_home, ".anthy", NULL);
  g_assert_cmpint(g_mkdir_with_parents(anthy_dir, 0700), ==, 0);
  g_free(anthy_dir);

  /* Anthy only adds words to an existing private dictionary file, so
   * always create it (possibly empty). */
  path = anthy_private_dic_path();
  g_file_set_contents(path, contents ? contents : "", -1, &error);
  g_assert_no_error(error);
  g_free(path);

  return TRUE;
}

gchar *
test_dict_anthy_read_private_dic(void)
{
  gchar *path = anthy_private_dic_path();
  gchar *contents = NULL;

  if (!g_file_get_contents(path, &contents, NULL, NULL))
    contents = NULL;
  g_free(path);

  return contents;
}
#endif
