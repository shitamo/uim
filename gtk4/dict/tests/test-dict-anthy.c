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
 * Tests for the Anthy backend of uim-dict-gtk4 (anthy.c through dict.c).
 *
 * Every test works on a private dictionary file in a temporary home
 * directory (see test_dict_anthy_setup()); the real user dictionary is
 * never opened.
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "canna-cclass.h"
#include "dict.h"
#include "util.h"
#include "word.h"

#include "test-dict-common.h"

#define ANTHY_IDENTIFIER "Anthy private dictionary"

#define FIXTURE                                                                \
  "あいう #JN*50 愛宇\n"                                                  \
  "てすと #T35*100 手素人\n"

/* Convert a UTF-8 literal to the dictionary's charset. */
static gchar *
to_dict(uim_dict *dict, const gchar *utf8)
{
  return charset_convert(utf8, "UTF-8", dict->charset);
}

static uim_word *
find_word(uim_dict *dict, const gchar *utf8_phon, const gchar *utf8_desc)
{
  gchar *phon = to_dict(dict, utf8_phon);
  gchar *desc = to_dict(dict, utf8_desc);
  uim_word *w = test_dict_word_list_find(dict->word_list, phon, desc);

  g_free(phon);
  g_free(desc);
  return w;
}

static uim_word *
make_word(uim_dict *dict,
          const gchar *utf8_phon,
          const gchar *utf8_desc,
          const gchar *cclass_native,
          int freq)
{
  uim_word *word = NULL;
  gchar *phon = to_dict(dict, utf8_phon);
  gchar *desc = to_dict(dict, utf8_desc);

  word_append(&word,
              WORD_TYPE_ANTHY,
              dict->charset,
              phon,
              desc,
              NULL,
              cclass_native,
              freq,
              0,
              NULL);
  g_free(phon);
  g_free(desc);
  return word;
}

static uim_dict *
open_dict(void)
{
  uim_dict *dict = uim_dict_open(ANTHY_IDENTIFIER);

  g_assert_nonnull(dict);
  return uim_dict_ref(dict);
}

/* the part-of-speech description for a code, in the dictionary charset */
static gchar *
cclass_desc(uim_dict *dict, const char *code)
{
  const char *eucjp = find_desc_from_code(code);

  g_assert_nonnull(eucjp);
  return charset_convert(eucjp, "EUC-JP", dict->charset);
}

static void
test_anthy_open_unknown(void)
{
  if (!test_dict_anthy_setup(FIXTURE))
    return;

  g_assert_null(uim_dict_open(NULL));
  g_assert_null(uim_dict_open("No such dictionary"));
  g_assert_null(uim_dict_open("Canna private dictionary"));
}

static void
test_anthy_read_private_dic(void)
{
  uim_dict *dict;
  uim_word *w;
  gchar *desc;

  if (!test_dict_anthy_setup(FIXTURE))
    return;

  dict = open_dict();
  g_assert_cmpstr(dict->identifier, ==, ANTHY_IDENTIFIER);
  g_assert_null(dict->filename);
#if LIBANTHY_UTF8_CAPABLE
  g_assert_cmpstr(dict->charset, ==, "UTF-8");
#else
  g_assert_cmpstr(dict->charset, ==, "EUC-JP");
#endif
  g_assert_cmpuint(test_dict_word_list_length(dict->word_list), ==, 2);

  w = find_word(dict, "あいう", "愛宇");
  g_assert_nonnull(w);
  g_assert_cmpint(w->type, ==, WORD_TYPE_ANTHY);
  g_assert_cmpstr(w->charset, ==, dict->charset);
  g_assert_cmpstr(w->cclass_native, ==, "#JN");
  g_assert_cmpint(w->freq, ==, 50);
  g_assert_cmpint(w->okuri, ==, 0);
  desc = cclass_desc(dict, "#JN");
  g_assert_cmpstr(w->cclass_code, ==, desc);
  g_free(desc);

  w = find_word(dict, "てすと", "手素人");
  g_assert_nonnull(w);
  g_assert_cmpstr(w->cclass_native, ==, "#T35");
  g_assert_cmpint(w->freq, ==, 100);
  desc = cclass_desc(dict, "#T35");
  g_assert_cmpstr(w->cclass_code, ==, desc);
#if LIBANTHY_UTF8_CAPABLE
  g_assert_cmpstr(w->cclass_code, ==, "名詞(語幹,格助接続)");
#endif
  g_free(desc);

  uim_dict_unref(dict);
}

static void
test_anthy_empty_dic(void)
{
  uim_dict *dict;

  if (!test_dict_anthy_setup(""))
    return;

  dict = open_dict();
  g_assert_null(dict->word_list);
  uim_dict_unref(dict);
}

static void
test_anthy_add_word(void)
{
  uim_dict *dict;
  uim_word *word, *w;
  gchar *contents, *desc;

  if (!test_dict_anthy_setup(FIXTURE))
    return;

  dict = open_dict();
  word = make_word(dict, "おく", "置く", "#K5", 300);
  g_assert_cmpint(uim_dict_add_word(dict, word), ==, 1);
  word_free_list(word);

  /* written to the private dictionary file ... */
  contents = test_dict_anthy_read_private_dic();
  g_assert_nonnull(contents);
  g_assert_nonnull(strstr(contents, "おく #K5*300 置く"));
  /* ... without touching the other entries */
  g_assert_nonnull(strstr(contents, "あいう #JN*50 愛宇"));
  g_assert_nonnull(strstr(contents, "てすと #T35*100 手素人"));
  g_free(contents);

  /* and visible after a refresh */
  g_assert_null(find_word(dict, "おく", "置く"));
  uim_dict_refresh(dict);
  g_assert_cmpuint(test_dict_word_list_length(dict->word_list), ==, 3);
  w = find_word(dict, "おく", "置く");
  g_assert_nonnull(w);
  g_assert_cmpstr(w->cclass_native, ==, "#K5");
  g_assert_cmpint(w->freq, ==, 300);
  desc = cclass_desc(dict, "#K5");
  g_assert_cmpstr(w->cclass_code, ==, desc);
  g_free(desc);

  uim_dict_unref(dict);

  /* a newly opened dictionary sees the word too */
  dict = open_dict();
  g_assert_nonnull(find_word(dict, "おく", "置く"));
  uim_dict_unref(dict);
}

static void
test_anthy_add_duplicate(void)
{
  uim_dict *dict;
  uim_word *word, *w;

  if (!test_dict_anthy_setup(FIXTURE))
    return;

  dict = open_dict();
  word = make_word(dict, "てすと", "手素人", "#T35", 500);
  /* a duplicate counts as success and updates the frequency */
  g_assert_cmpint(uim_dict_add_word(dict, word), ==, 1);
  word_free_list(word);

  uim_dict_refresh(dict);
  g_assert_cmpuint(test_dict_word_list_length(dict->word_list), ==, 2);
  w = find_word(dict, "てすと", "手素人");
  g_assert_nonnull(w);
  g_assert_cmpint(w->freq, ==, 500);

  uim_dict_unref(dict);
}

static void
test_anthy_add_invalid(void)
{
  uim_dict *dict;
  uim_word *word;
  gchar *before, *after;

  if (!test_dict_anthy_setup(FIXTURE))
    return;

  before = test_dict_anthy_read_private_dic();
  dict = open_dict();

  word = make_word(dict, "", "空", "#T35", 1);
  g_assert_cmpint(uim_dict_add_word(dict, word), ==, 0);
  word_free_list(word);

  word = make_word(dict, "から", "", "#T35", 1);
  g_assert_cmpint(uim_dict_add_word(dict, word), ==, 0);
  word_free_list(word);

  word = make_word(dict, "から", "空", "", 1);
  g_assert_cmpint(uim_dict_add_word(dict, word), ==, 0);
  word_free_list(word);

  g_assert_cmpint(uim_dict_add_word(dict, NULL), ==, 0);

  after = test_dict_anthy_read_private_dic();
  g_assert_cmpstr(after, ==, before);

  uim_dict_refresh(dict);
  g_assert_cmpuint(test_dict_word_list_length(dict->word_list), ==, 2);

  uim_dict_unref(dict);
  g_free(before);
  g_free(after);
}

static void
test_anthy_remove_word(void)
{
  uim_dict *dict;
  uim_word *w;
  gchar *contents;

  if (!test_dict_anthy_setup(FIXTURE))
    return;

  dict = open_dict();
  w = find_word(dict, "てすと", "手素人");
  g_assert_nonnull(w);
  g_assert_cmpint(uim_dict_remove_word(dict, w), ==, 1);

  contents = test_dict_anthy_read_private_dic();
  g_assert_null(strstr(contents, "手素人"));
  g_assert_nonnull(strstr(contents, "あいう #JN*50 愛宇"));
  g_free(contents);

  uim_dict_refresh(dict);
  g_assert_cmpuint(test_dict_word_list_length(dict->word_list), ==, 1);
  g_assert_null(find_word(dict, "てすと", "手素人"));
  g_assert_nonnull(find_word(dict, "あいう", "愛宇"));

  /* remove the last one as well */
  g_assert_cmpint(uim_dict_remove_word(dict, dict->word_list), ==, 1);
  uim_dict_refresh(dict);
  g_assert_null(dict->word_list);

  uim_dict_unref(dict);
}

static void
test_anthy_change_word_unsupported(void)
{
  uim_dict *dict;

  if (!test_dict_anthy_setup(FIXTURE))
    return;

  dict = open_dict();
  g_assert_cmpint(uim_dict_change_word(dict, dict->word_list), ==, 0);
  uim_dict_unref(dict);
}

int
main(int argc, char *argv[])
{
  int ret;

  g_test_init(&argc, &argv, NULL);
  test_dict_setup_home();

  g_test_add_func("/dict/anthy/open-unknown", test_anthy_open_unknown);
  g_test_add_func("/dict/anthy/read-private-dic", test_anthy_read_private_dic);
  g_test_add_func("/dict/anthy/empty-dic", test_anthy_empty_dic);
  g_test_add_func("/dict/anthy/add-word", test_anthy_add_word);
  g_test_add_func("/dict/anthy/add-duplicate", test_anthy_add_duplicate);
  g_test_add_func("/dict/anthy/add-invalid", test_anthy_add_invalid);
  g_test_add_func("/dict/anthy/remove-word", test_anthy_remove_word);
  g_test_add_func("/dict/anthy/change-word-unsupported",
                  test_anthy_change_word_unsupported);

  ret = g_test_run();
  test_dict_cleanup_home();
  return ret;
}
