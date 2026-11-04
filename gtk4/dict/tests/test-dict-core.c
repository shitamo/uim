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
 * Unit tests for the GUI-independent parts of uim-dict-gtk4:
 * word.c (word lists), util.c (charset conversion), canna-cclass.c
 * (part-of-speech tables) and dict.c (the uim_dict dispatch layer, driven
 * by an in-memory fake backend).
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
#include "test-fake-dict.h"

/* ------------------------------------------------------------------ */
/* word.c                                                              */
/* ------------------------------------------------------------------ */

static void
test_word_append_null_fields(void)
{
  uim_word *list = NULL;

  word_append(&list, WORD_TYPE_CANNA, NULL, NULL, NULL, NULL, NULL, 7, 1, NULL);

  g_assert_nonnull(list);
  g_assert_null(list->next);
  g_assert_cmpint(list->type, ==, WORD_TYPE_CANNA);
  /* every NULL string becomes an empty string, never NULL */
  g_assert_cmpstr(list->charset, ==, "");
  g_assert_cmpstr(list->phon, ==, "");
  g_assert_cmpstr(list->desc, ==, "");
  g_assert_cmpstr(list->cclass_code, ==, "");
  g_assert_cmpstr(list->cclass_native, ==, "");
  g_assert_cmpstr(list->annotation, ==, "");
  g_assert_cmpint(list->freq, ==, 7);
  g_assert_cmpint(list->okuri, ==, 1);

  word_free_list(list);
}

static void
test_word_append_copies_and_orders(void)
{
  uim_word *list = NULL, *last;
  char phon[] = "かんじ";
  char desc[] = "漢字";

  word_append(&list,
              WORD_TYPE_ANTHY,
              "UTF-8",
              phon,
              desc,
              "名詞",
              "#T35",
              100,
              0,
              "note");
  word_append(&list,
              WORD_TYPE_ANTHY,
              "UTF-8",
              "にほん",
              "日本",
              "地名",
              "#CN",
              200,
              0,
              NULL);
  word_append(&list,
              WORD_TYPE_ANTHY,
              "UTF-8",
              "とうきょう",
              "東京",
              "地名",
              "#CN",
              300,
              0,
              NULL);

  /* the strings are copied, not referenced */
  phon[0] = 'X';
  desc[0] = 'X';

  g_assert_cmpuint(test_dict_word_list_length(list), ==, 3);
  g_assert_cmpstr(list->charset, ==, "UTF-8");
  g_assert_cmpstr(list->phon, ==, "かんじ");
  g_assert_cmpstr(list->desc, ==, "漢字");
  g_assert_cmpstr(list->cclass_code, ==, "名詞");
  g_assert_cmpstr(list->cclass_native, ==, "#T35");
  g_assert_cmpstr(list->annotation, ==, "note");
  g_assert_cmpint(list->freq, ==, 100);

  /* appended at the tail, in call order */
  g_assert_cmpstr(list->next->phon, ==, "にほん");
  g_assert_cmpstr(list->next->next->phon, ==, "とうきょう");

  last = word_last(list);
  g_assert_true(last == list->next->next);
  g_assert_null(last->next);
  g_assert_cmpint(last->freq, ==, 300);

  word_free_list(list);
}

static void
test_word_last_and_free_empty(void)
{
  g_assert_null(word_last(NULL));
  word_free_list(NULL); /* must not crash */
}

static void
test_word_identifier_to_type(void)
{
  g_assert_cmpint(dict_identifier_to_word_type("Anthy private dictionary"),
                  ==,
                  WORD_TYPE_ANTHY);
  g_assert_cmpint(dict_identifier_to_word_type("Canna private dictionary"),
                  ==,
                  WORD_TYPE_CANNA);
  /* unknown identifiers fall back to Anthy */
  g_assert_cmpint(dict_identifier_to_word_type("Unknown dictionary"),
                  ==,
                  WORD_TYPE_ANTHY);

  g_assert_cmpint(dict_identifier_to_support_type("Anthy private dictionary"),
                  ==,
                  SUPPORT_ANTHY);
  g_assert_cmpint(dict_identifier_to_support_type("Canna private dictionary"),
                  ==,
                  SUPPORT_CANNA);
  g_assert_cmpint(dict_identifier_to_support_type("Unknown dictionary"),
                  ==,
                  SUPPORT_ANTHY);
}

/* ------------------------------------------------------------------ */
/* util.c                                                              */
/* ------------------------------------------------------------------ */

static void
test_util_eucjp_roundtrip(void)
{
  const gchar *utf8 = "漢字かなカナ ABC";
  gchar *eucjp, *back, *expected;

  eucjp = utf8_to_eucjp(utf8);
  g_assert_nonnull(eucjp);
  expected = test_dict_eucjp(utf8);
  g_assert_cmpstr(eucjp, ==, expected);
  /* EUC-JP is not valid UTF-8 here, so it really was converted */
  g_assert_false(g_utf8_validate(eucjp, -1, NULL));

  back = eucjp_to_utf8(eucjp);
  g_assert_cmpstr(back, ==, utf8);

  g_free(eucjp);
  g_free(back);
  g_free(expected);
}

static void
test_util_charset_convert(void)
{
  gchar *out;

  out = charset_convert("ascii only", "UTF-8", "EUC-JP");
  g_assert_cmpstr(out, ==, "ascii only");
  g_free(out);

  out = charset_convert("にほんご", "UTF-8", "UTF-8");
  g_assert_cmpstr(out, ==, "にほんご");
  g_free(out);

  out = charset_convert("", "UTF-8", "EUC-JP");
  g_assert_cmpstr(out, ==, "");
  g_free(out);
}

static void
test_util_charset_convert_unconvertible(void)
{
  if (g_test_subprocess()) {
    gchar *out;

    /* U+1F600 has no EUC-JP representation */
    out = utf8_to_eucjp("\xf0\x9f\x98\x80");
    g_assert_null(out);

    /* not valid UTF-8 */
    out = utf8_to_eucjp("\xa4\xa2");
    g_assert_null(out);
    return;
  }

  /* charset_convert() reports conversion errors on stderr */
  g_test_trap_subprocess(NULL, 0, G_TEST_SUBPROCESS_DEFAULT);
  g_test_trap_assert_passed();
  g_test_trap_assert_stderr("*g_convert failed*");
}

static void
test_util_charset_convert_bad_args(void)
{
  gchar *out;

  g_test_expect_message(NULL, G_LOG_LEVEL_CRITICAL, "*assertion*");
  out = charset_convert(NULL, "UTF-8", "EUC-JP");
  g_test_assert_expected_messages();
  g_assert_null(out);

  /* an empty or missing charset returns a copy of the input */
  g_test_expect_message(NULL, G_LOG_LEVEL_CRITICAL, "*assertion*");
  out = charset_convert("abc", "", "EUC-JP");
  g_test_assert_expected_messages();
  g_assert_cmpstr(out, ==, "abc");
  g_free(out);

  g_test_expect_message(NULL, G_LOG_LEVEL_CRITICAL, "*assertion*");
  out = charset_convert("abc", "UTF-8", NULL);
  g_test_assert_expected_messages();
  g_assert_cmpstr(out, ==, "abc");
  g_free(out);
}

static void
test_util_utf8_convert(void)
{
  gchar *out;

  /* the tests run in a UTF-8 locale (C.UTF-8) */
  out = utf8_convert("日本語");
  g_assert_cmpstr(out, ==, "日本語");
  g_free(out);
}

/* ------------------------------------------------------------------ */
/* canna-cclass.c                                                      */
/* ------------------------------------------------------------------ */

typedef struct
{
  category_code *table;
  unsigned int *n;
  int type;
} cclass_table;

static void
get_cclass_tables(cclass_table tables[NR_POS])
{
  tables[POS_SUBSTANTIVE].table = substantive_code;
  tables[POS_SUBSTANTIVE].n = &nr_substantive_code;
  tables[POS_VERB].table = verb_code;
  tables[POS_VERB].n = &nr_verb_code;
  tables[POS_ADJECTIVE].table = adjective_code;
  tables[POS_ADJECTIVE].n = &nr_adjective_code;
  tables[POS_ADVERB].table = adverb_code;
  tables[POS_ADVERB].n = &nr_adverb_code;
  tables[POS_ETC].table = etc_code;
  tables[POS_ETC].n = &nr_etc_code;
}

static void
test_cclass_tables_consistent(void)
{
  cclass_table tables[NR_POS];
  int pos;

  get_cclass_tables(tables);

  for (pos = 0; pos < NR_POS; pos++) {
    unsigned int i;

    g_assert_cmpuint(*tables[pos].n, >, 0);

    for (i = 0; i < *tables[pos].n; i++) {
      const category_code *c = &tables[pos].table[i];
      gchar *utf8;

      g_assert_nonnull(c->code);
      g_assert_true(c->code[0] == '#');
      g_assert_nonnull(c->desc);
      g_assert_nonnull(c->example);

      /* descriptions are EUC-JP and must convert cleanly */
      utf8 = eucjp_to_utf8(c->desc);
      g_assert_nonnull(utf8);
      g_assert_true(g_utf8_validate(utf8, -1, NULL));
      g_free(utf8);

      /* code -> description -> code round trip within the category.
       * Descriptions are not unique (see test_cclass_duplicate_desc()), so
       * only require a code with the same description. */
      g_assert_cmpstr(find_desc_from_code_with_type(c->code, pos), ==, c->desc);
      g_assert_nonnull(find_code_from_desc(c->desc, pos));
      g_assert_cmpstr(
        find_desc_from_code_with_type(find_code_from_desc(c->desc, pos), pos),
        ==,
        c->desc);
      g_assert_cmpint(find_cclass_type_from_code(c->code), ==, pos);
      g_assert_cmpint(find_cclass_type_from_desc(c->desc), ==, pos);
      g_assert_cmpstr(find_desc_from_code(c->code), ==, c->desc);
    }
  }
}

static void
test_cclass_duplicate_desc(void)
{
  cclass_table tables[NR_POS];
  GString *duplicates = g_string_new(NULL);
  int pos;

  get_cclass_tables(tables);

  /* The GUI shows descriptions and maps them back to codes with
   * find_code_from_desc(), so two codes sharing a description cannot both
   * be chosen.  Keep track of the known case and catch new ones. */
  for (pos = 0; pos < NR_POS; pos++) {
    unsigned int i, j;

    for (i = 0; i < *tables[pos].n; i++) {
      for (j = i + 1; j < *tables[pos].n; j++) {
        if (!strcmp(tables[pos].table[i].desc, tables[pos].table[j].desc))
          g_string_append_printf(duplicates,
                                 "%s=%s ",
                                 tables[pos].table[i].code,
                                 tables[pos].table[j].code);
      }
    }
  }

  /* "#T37" and "#F14" are both described as "adverb (stem)"; the later
   * entry wins, so "#T37" cannot be selected from its description. */
  g_assert_cmpstr(duplicates->str, ==, "#T37=#F14 ");
  g_assert_cmpstr(find_code_from_desc(adverb_code[5].desc, POS_ADVERB),
                  ==,
                  "#F14");
  g_assert_cmpstr(adverb_code[5].code, ==, "#T37");

  g_string_free(duplicates, TRUE);
}

static void
check_code(const char *code, int type, const char *utf8_desc)
{
  gchar *desc = test_dict_eucjp(utf8_desc);

  g_assert_cmpstr(find_desc_from_code(code), ==, desc);
  g_assert_cmpstr(find_desc_from_code_with_type(code, type), ==, desc);
  g_assert_cmpstr(find_code_from_desc(desc, type), ==, code);
  g_assert_cmpint(find_cclass_type_from_code(code), ==, type);
  g_assert_cmpint(find_cclass_type_from_desc(desc), ==, type);

  g_free(desc);
}

static void
test_cclass_known_codes(void)
{
  check_code("#T00", POS_SUBSTANTIVE, "名詞(な,さ,する,語幹,格助接続)");
  check_code("#T35", POS_SUBSTANTIVE, "名詞(語幹,格助接続)");
  check_code("#JN", POS_SUBSTANTIVE, "人名");
  check_code("#CN", POS_SUBSTANTIVE, "地名");
  check_code("#K5", POS_VERB, "か行5段");
  check_code("#SX", POS_VERB, "サ変活用動詞");
  check_code("#KY", POS_ADJECTIVE, "形容詞");
  check_code("#F06", POS_ADVERB, "副詞(と,語幹)");
  check_code("#T07", POS_ADVERB, "副詞(な,さ,語幹)");
  check_code("#KJ", POS_ETC, "一文字漢字");
  check_code("#NN", POS_ETC, "数詞:一般");
}

static void
test_cclass_first_entries(void)
{
  /* WordWindow uses the first entry of each table as the default part of
   * speech for the corresponding broad category */
  g_assert_cmpstr(substantive_code[0].code, ==, "#T00");
  g_assert_cmpstr(verb_code[0].code, ==, "#K5");
  g_assert_cmpstr(adjective_code[0].code, ==, "#KY");
  g_assert_cmpstr(adverb_code[0].code, ==, "#T07");
  g_assert_cmpstr(etc_code[0].code, ==, "#KJ");
}

static void
test_cclass_unknown(void)
{
  gchar *desc;
  int pos;

  g_assert_null(find_desc_from_code("#NOSUCH"));
  g_assert_cmpint(find_cclass_type_from_code("#NOSUCH"), ==, -1);
  g_assert_null(find_code_from_desc("no such description", POS_SUBSTANTIVE));
  g_assert_cmpint(find_cclass_type_from_desc("no such description"), ==, -1);
  g_assert_null(find_desc_from_code(""));

  /* a code only matches within its own category */
  g_assert_null(find_desc_from_code_with_type("#T35", POS_VERB));
  g_assert_null(find_desc_from_code_with_type("#K5", POS_SUBSTANTIVE));
  /* the place/person codes moved from "etc" to "substantive" */
  g_assert_null(find_desc_from_code_with_type("#CN", POS_ETC));

  desc = test_dict_eucjp("人名");
  g_assert_null(find_code_from_desc(desc, POS_ETC));
  g_free(desc);

  /* out-of-range categories never match */
  for (pos = NR_POS; pos < NR_POS + 2; pos++)
    g_assert_null(find_desc_from_code_with_type("#T35", pos));
}

/* ------------------------------------------------------------------ */
/* dict.c                                                              */
/* ------------------------------------------------------------------ */

static void
test_dict_open_without_backend(void)
{
  /* this binary has no backend compiled in */
  g_assert_null(uim_dict_open("Anthy private dictionary"));
  g_assert_null(uim_dict_open("Canna private dictionary"));
}

static void
test_dict_null_safety(void)
{
  uim_word *word = NULL;

  word_append(&word,
              WORD_TYPE_ANTHY,
              "UTF-8",
              "a",
              "b",
              NULL,
              NULL,
              1,
              0,
              NULL);

  g_assert_cmpint(uim_dict_add_word(NULL, word), ==, 0);
  g_assert_cmpint(uim_dict_change_word(NULL, word), ==, 0);
  g_assert_cmpint(uim_dict_remove_word(NULL, word), ==, 0);
  uim_dict_refresh(NULL);
  uim_dict_close(NULL);
  g_assert_null(uim_dict_ref(NULL));
  uim_dict_unref(NULL);

  word_free_list(word);
}

static void
test_dict_dispatch(void)
{
  uim_dict *dict;
  uim_word *word = NULL;

  fake_dict_reset_stats();
  dict = uim_dict_ref(fake_dict_new("Test dictionary", "UTF-8"));
  g_assert_nonnull(dict);
  g_assert_cmpuint(dict->ref_count, ==, 1);
  g_assert_null(dict->word_list);

  word_append(&word,
              WORD_TYPE_ANTHY,
              "UTF-8",
              "てすと",
              "テスト",
              "名詞",
              "#T35",
              10,
              0,
              NULL);

  g_assert_cmpint(uim_dict_add_word(dict, word), ==, 1);
  g_assert_cmpint(fake_dict_stats_data.n_add, ==, 1);
  g_assert_cmpstr(fake_dict_stats_data.last_added->cclass_native, ==, "#T35");
  g_assert_cmpuint(fake_dict_n_stored(dict), ==, 1);

  /* the word list only changes on refresh */
  g_assert_null(dict->word_list);
  uim_dict_refresh(dict);
  g_assert_cmpint(fake_dict_stats_data.n_refresh, ==, 1);
  g_assert_cmpuint(test_dict_word_list_length(dict->word_list), ==, 1);
  g_assert_nonnull(
    test_dict_word_list_find(dict->word_list, "てすと", "テスト"));

  g_assert_cmpint(uim_dict_change_word(dict, word), ==, 1);
  g_assert_cmpint(fake_dict_stats_data.n_change, ==, 1);

  g_assert_cmpint(uim_dict_remove_word(dict, word), ==, 1);
  g_assert_cmpint(fake_dict_stats_data.n_remove, ==, 1);
  g_assert_cmpuint(fake_dict_n_stored(dict), ==, 0);
  uim_dict_refresh(dict);
  g_assert_null(dict->word_list);

  /* backend failures are passed through */
  fake_dict_set_fail(dict, 1);
  g_assert_cmpint(uim_dict_add_word(dict, word), ==, 0);
  g_assert_cmpint(uim_dict_change_word(dict, word), ==, 0);
  g_assert_cmpint(uim_dict_remove_word(dict, word), ==, 0);

  g_assert_cmpint(fake_dict_stats_data.n_close, ==, 0);
  uim_dict_unref(dict);
  g_assert_cmpint(fake_dict_stats_data.n_close, ==, 1);

  word_free_list(word);
  fake_dict_reset_stats();
}

static void
test_dict_refcount(void)
{
  uim_dict *dict;

  fake_dict_reset_stats();
  dict = fake_dict_new("Test dictionary", "UTF-8");
  g_assert_cmpuint(dict->ref_count, ==, 0);

  g_assert_true(uim_dict_ref(dict) == dict);
  g_assert_true(uim_dict_ref(dict) == dict);
  g_assert_cmpuint(dict->ref_count, ==, 2);

  uim_dict_unref(dict);
  g_assert_cmpint(fake_dict_stats_data.n_close, ==, 0);
  g_assert_cmpuint(dict->ref_count, ==, 1);

  uim_dict_unref(dict); /* last reference: closes the dictionary */
  g_assert_cmpint(fake_dict_stats_data.n_close, ==, 1);
  fake_dict_reset_stats();
}

static void
test_dict_missing_methods(void)
{
  static uim_dict_class empty_class = {"Empty"};
  uim_dict dict;
  uim_word *word = NULL;

  memset(&dict, 0, sizeof(dict));
  dict.funcs = &empty_class;
  dict.identifier = "Empty dictionary";
  dict.charset = "UTF-8";

  word_append(&word,
              WORD_TYPE_ANTHY,
              "UTF-8",
              "a",
              "b",
              NULL,
              NULL,
              1,
              0,
              NULL);

  /* a backend without the methods reports failure instead of crashing */
  g_assert_cmpint(uim_dict_add_word(&dict, word), ==, 0);
  g_assert_cmpint(uim_dict_change_word(&dict, word), ==, 0);
  g_assert_cmpint(uim_dict_remove_word(&dict, word), ==, 0);
  uim_dict_refresh(&dict);
  uim_dict_close(&dict);

  /* and so does a dictionary without a class */
  dict.funcs = NULL;
  g_assert_cmpint(uim_dict_add_word(&dict, word), ==, 0);
  g_assert_cmpint(uim_dict_change_word(&dict, word), ==, 0);
  g_assert_cmpint(uim_dict_remove_word(&dict, word), ==, 0);
  uim_dict_refresh(&dict);
  uim_dict_close(&dict);

  word_free_list(word);
}

int
main(int argc, char *argv[])
{
  g_test_init(&argc, &argv, NULL);

  g_test_add_func("/dict/word/append-null-fields",
                  test_word_append_null_fields);
  g_test_add_func("/dict/word/append-copies-and-orders",
                  test_word_append_copies_and_orders);
  g_test_add_func("/dict/word/last-and-free-empty",
                  test_word_last_and_free_empty);
  g_test_add_func("/dict/word/identifier-to-type",
                  test_word_identifier_to_type);

  g_test_add_func("/dict/util/eucjp-roundtrip", test_util_eucjp_roundtrip);
  g_test_add_func("/dict/util/charset-convert", test_util_charset_convert);
  g_test_add_func("/dict/util/charset-convert-unconvertible",
                  test_util_charset_convert_unconvertible);
  g_test_add_func("/dict/util/charset-convert-bad-args",
                  test_util_charset_convert_bad_args);
  g_test_add_func("/dict/util/utf8-convert", test_util_utf8_convert);

  g_test_add_func("/dict/cclass/tables-consistent",
                  test_cclass_tables_consistent);
  g_test_add_func("/dict/cclass/duplicate-desc", test_cclass_duplicate_desc);
  g_test_add_func("/dict/cclass/known-codes", test_cclass_known_codes);
  g_test_add_func("/dict/cclass/first-entries", test_cclass_first_entries);
  g_test_add_func("/dict/cclass/unknown", test_cclass_unknown);

  g_test_add_func("/dict/dict/open-without-backend",
                  test_dict_open_without_backend);
  g_test_add_func("/dict/dict/null-safety", test_dict_null_safety);
  g_test_add_func("/dict/dict/dispatch", test_dict_dispatch);
  g_test_add_func("/dict/dict/refcount", test_dict_refcount);
  g_test_add_func("/dict/dict/missing-methods", test_dict_missing_methods);

  return g_test_run();
}
