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
 * An in-memory uim_dict backend for tests.
 *
 * The backend keeps its own "stored" word list, which plays the role of
 * the dictionary file.  uim_dict::word_list is a snapshot of it that is
 * only rebuilt by refresh(), just like the Anthy backend re-reads its
 * private dictionary on refresh.
 */

#ifndef UIM_DICT_TEST_FAKE_DICT_H
#define UIM_DICT_TEST_FAKE_DICT_H

#include "dict.h"

typedef struct _fake_dict_stats
{
  int n_add;
  int n_change;
  int n_remove;
  int n_refresh;
  int n_close;
  /* the last word passed to add_word (copied) */
  uim_word *last_added;
} fake_dict_stats;

extern uim_dict_class fake_dict_class;
extern fake_dict_stats fake_dict_stats_data;

/*
 * Create a dictionary named @identifier holding words in @charset.
 * Both the stored words and the word list start out empty.
 */
uim_dict *
fake_dict_new(const char *identifier, const char *charset);

/* Store a word (as if it already were in the dictionary file); call
 * uim_dict_refresh() to make it visible in dict->word_list. */
void
fake_dict_store(uim_dict *dict,
                const char *phon,
                const char *desc,
                const char *cclass_code,
                const char *cclass_native,
                int freq);

/* Number of stored words, and lookup of a stored word. */
unsigned int
fake_dict_n_stored(uim_dict *dict);
uim_word *
fake_dict_find_stored(uim_dict *dict, const char *phon, const char *desc);

/* Make add/change/remove fail (return 0) from now on. */
void
fake_dict_set_fail(uim_dict *dict, int fail);

void
fake_dict_reset_stats(void);

#endif /* UIM_DICT_TEST_FAKE_DICT_H */
