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

/* Helpers shared by the uim-dict-gtk4 tests. */

#ifndef UIM_DICT_TEST_COMMON_H
#define UIM_DICT_TEST_COMMON_H

#include <glib.h>

#include "word.h"

G_BEGIN_DECLS

/*
 * Create a fresh temporary directory, point $HOME and the XDG base
 * directories at it so that nothing the code under test writes can reach
 * the real user's files, and return its path.  The directory is removed
 * by test_dict_cleanup_home().
 */
const gchar *
test_dict_setup_home(void);
void
test_dict_cleanup_home(void);

/* Convert a UTF-8 string literal to EUC-JP (newly allocated). */
gchar *
test_dict_eucjp(const gchar *utf8);

/* Number of entries in a word list. */
guint
test_dict_word_list_length(uim_word *list);

/* First entry whose phon and desc equal the given strings, or NULL. */
uim_word *
test_dict_word_list_find(uim_word *list, const gchar *phon, const gchar *desc);

#ifdef USE_ANTHY
/*
 * Make Anthy use the temporary home returned by test_dict_setup_home()
 * and (re)write its private dictionary file with @contents, which is in
 * Anthy's text format ("yomi #WTYPE*freq word" per line, UTF-8).
 *
 * Anthy does not look at $HOME; it takes the home directory from the
 * password database.  test_dict_anthy_setup() therefore overrides Anthy's
 * "HOME" configuration value and verifies that the override took effect,
 * so the tests can never read or modify the real private dictionary.
 * Returns FALSE (after calling g_test_skip()) if that is not possible.
 */
gboolean
test_dict_anthy_setup(const gchar *contents);

/* Contents of the private dictionary file, or NULL if it does not exist. */
gchar *
test_dict_anthy_read_private_dic(void);
#endif

G_END_DECLS

#endif /* UIM_DICT_TEST_COMMON_H */
