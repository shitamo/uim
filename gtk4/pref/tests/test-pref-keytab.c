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
 * Unit tests for gtk-keytab.h, the UKey -> key name table that the key
 * editor of uim-pref-gtk4 uses to turn a grabbed key into a literal
 * such as "<Control>escape". No display is needed.
 */

#include <config.h>

#include "pref-test-common.h"

#include "../gtk-keytab.h"

static void
test_known_keys(void)
{
  g_assert_cmpstr(uim_pref_get_keysym(UKey_Escape), ==, "escape");
  g_assert_cmpstr(uim_pref_get_keysym(UKey_Return), ==, "return");
  g_assert_cmpstr(uim_pref_get_keysym(UKey_Backspace), ==, "backspace");
  g_assert_cmpstr(uim_pref_get_keysym(UKey_Tab), ==, "tab");
  g_assert_cmpstr(uim_pref_get_keysym(UKey_Left), ==, "left");
  g_assert_cmpstr(uim_pref_get_keysym(UKey_Prior), ==, "prior");
  g_assert_cmpstr(uim_pref_get_keysym(UKey_F1), ==, "F1");
  g_assert_cmpstr(uim_pref_get_keysym(UKey_F35), ==, "F35");
  g_assert_cmpstr(uim_pref_get_keysym(UKey_Muhenkan), ==, "Muhenkan");
  g_assert_cmpstr(uim_pref_get_keysym(UKey_Zenkaku_Hankaku),
                  ==,
                  "zenkaku-hankaku");
  g_assert_cmpstr(uim_pref_get_keysym(UKey_Hangul), ==, "hangul");
  g_assert_cmpstr(uim_pref_get_keysym(UKey_Dead_Grave), ==, "dead-grave");
  g_assert_cmpstr(uim_pref_get_keysym(UKey_Kana_A), ==, "kana-A");
  g_assert_cmpstr(uim_pref_get_keysym(UKey_Shift_key), ==, "Shift_key");
  g_assert_cmpstr(uim_pref_get_keysym(UKey_Caps_Lock), ==, "caps-lock");
}

static void
test_unknown_keys(void)
{
  /* Printable ASCII is handled by key_pref_set_value() itself and is
   * not in the table. */
  g_assert_null(uim_pref_get_keysym('a'));
  g_assert_null(uim_pref_get_keysym(' '));
  g_assert_null(uim_pref_get_keysym(0));
  g_assert_null(uim_pref_get_keysym(UKey_Other));
  g_assert_null(uim_pref_get_keysym(-1));
}

/* F1..F35, the dead keys and the kana keys are contiguous ranges in
 * uim.h, which im_uim_convert_keyevent() relies on. */
static void
test_ranges_are_contiguous(void)
{
  int i;

  for (i = 0; i <= UKey_F35 - UKey_F1; i++) {
    char *expected = g_strdup_printf("F%d", i + 1);

    g_assert_cmpstr(uim_pref_get_keysym(UKey_F1 + i), ==, expected);
    g_free(expected);
  }
  for (i = UKey_Dead_Grave; i <= UKey_Dead_Horn; i++)
    g_assert_true(g_str_has_prefix(uim_pref_get_keysym(i), "dead-"));
  for (i = UKey_Kana_Fullstop; i <= UKey_Kana_SemivoicedSound; i++)
    g_assert_true(g_str_has_prefix(uim_pref_get_keysym(i), "kana-"));
}

static void
test_table_is_consistent(void)
{
  int i, j;

  for (i = 0; key_tab[i].key; i++) {
    g_assert_nonnull(key_tab[i].str);
    /* every code above the ASCII range ... */
    g_assert_cmpint(key_tab[i].key, >=, 128);
    /* ... maps to exactly one name, so lookups are unambiguous */
    for (j = i + 1; key_tab[j].key; j++) {
      g_assert_cmpint(key_tab[i].key, !=, key_tab[j].key);
      g_assert_cmpstr(key_tab[i].str, !=, key_tab[j].str);
    }
  }
  g_assert_cmpint(i, >, 100);
}

/* Every name the editor can produce must be a key string that uim's
 * Scheme side accepts; otherwise uim_custom_set() rejects the key
 * that the user has just grabbed. */
static void
test_names_are_valid_uim_keys(void)
{
  int i;

  for (i = 0; key_tab[i].key; i++) {
    const char *modifiers[] = {"", "<Control>", "<Shift><Alt>", NULL};
    const char **mod;

    for (mod = modifiers; *mod; mod++) {
      char *expr =
        g_strdup_printf("(valid-key-str? \"%s%s\")", *mod, key_tab[i].str);
      uim_lisp result = uim_scm_eval_c_string(expr);

      if (!uim_scm_c_bool(result))
        g_error("uim rejects the key string \"%s%s\"", *mod, key_tab[i].str);
      g_free(expr);
    }
  }
}

int
main(int argc, char *argv[])
{
  int status;

  g_test_init(&argc, &argv, NULL);

  pref_test_setup_environment();
  pref_test_init_uim();

  g_test_add_func("/pref/keytab/known-keys", test_known_keys);
  g_test_add_func("/pref/keytab/unknown-keys", test_unknown_keys);
  g_test_add_func("/pref/keytab/ranges", test_ranges_are_contiguous);
  g_test_add_func("/pref/keytab/consistent", test_table_is_consistent);
  g_test_add_func("/pref/keytab/valid-uim-keys", test_names_are_valid_uim_keys);

  status = g_test_run();

  pref_test_quit_uim();
  return status;
}
