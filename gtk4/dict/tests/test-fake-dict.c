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

#include <stdlib.h>
#include <string.h>

#include "test-fake-dict.h"

typedef struct _fake_dict
{
  uim_dict dict; /* must be first */
  uim_word *stored;
  int fail;
} fake_dict;

fake_dict_stats fake_dict_stats_data;

static uim_dict *
fake_dict_open(const char *identifier);
static void
fake_dict_close(uim_dict *dict);
static int
fake_dict_add_word(uim_dict *dict, uim_word *word);
static int
fake_dict_change_word(uim_dict *dict, uim_word *word);
static int
fake_dict_remove_word(uim_dict *dict, uim_word *word);
static void
fake_dict_refresh(uim_dict *dict);

uim_dict_class fake_dict_class = {
  "Fake", /* type */

  NULL, /* init */
  NULL, /* exit */

  NULL, /* list_known_dict */
  NULL, /* default_priv_dict */

  fake_dict_open, /* open */

  NULL, /* is_supported */
  NULL, /* load_file */
  NULL, /* save_file */

  fake_dict_close, /* close */

  fake_dict_add_word,    /* add_word */
  fake_dict_change_word, /* change_word */
  fake_dict_remove_word, /* remove_word */
  fake_dict_refresh,     /* refresh */
};

static uim_dict *
fake_dict_open(const char *identifier)
{
  return fake_dict_new(identifier, "UTF-8");
}

uim_dict *
fake_dict_new(const char *identifier, const char *charset)
{
  fake_dict *fd = calloc(1, sizeof(fake_dict));

  fd->dict.funcs = &fake_dict_class;
  fd->dict.identifier = strdup(identifier);
  fd->dict.filename = NULL;
  fd->dict.charset = strdup(charset);
  fd->dict.ref_count = 0;
  fd->dict.word_list = NULL;

  return &fd->dict;
}

void
fake_dict_store(uim_dict *dict,
                const char *phon,
                const char *desc,
                const char *cclass_code,
                const char *cclass_native,
                int freq)
{
  fake_dict *fd = (fake_dict *)dict;

  word_append(&fd->stored,
              WORD_TYPE_ANTHY,
              dict->charset,
              (char *)phon,
              (char *)desc,
              cclass_code,
              cclass_native,
              freq,
              0,
              NULL);
}

unsigned int
fake_dict_n_stored(uim_dict *dict)
{
  fake_dict *fd = (fake_dict *)dict;
  unsigned int n = 0;
  uim_word *w;

  for (w = fd->stored; w; w = w->next)
    n++;
  return n;
}

uim_word *
fake_dict_find_stored(uim_dict *dict, const char *phon, const char *desc)
{
  fake_dict *fd = (fake_dict *)dict;
  uim_word *w;

  for (w = fd->stored; w; w = w->next) {
    if (!strcmp(w->phon, phon) && !strcmp(w->desc, desc))
      return w;
  }
  return NULL;
}

void
fake_dict_set_fail(uim_dict *dict, int fail)
{
  ((fake_dict *)dict)->fail = fail;
}

void
fake_dict_reset_stats(void)
{
  word_free_list(fake_dict_stats_data.last_added);
  memset(&fake_dict_stats_data, 0, sizeof(fake_dict_stats_data));
}

static void
fake_dict_close(uim_dict *dict)
{
  fake_dict *fd = (fake_dict *)dict;

  fake_dict_stats_data.n_close++;

  word_free_list(dict->word_list);
  word_free_list(fd->stored);
  free(dict->identifier);
  free(dict->filename);
  free(dict->charset);
  free(fd);
}

static int
fake_dict_add_word(uim_dict *dict, uim_word *word)
{
  fake_dict *fd = (fake_dict *)dict;
  uim_word *found;

  fake_dict_stats_data.n_add++;

  word_free_list(fake_dict_stats_data.last_added);
  fake_dict_stats_data.last_added = NULL;
  word_append(&fake_dict_stats_data.last_added,
              word->type,
              word->charset,
              word->phon,
              word->desc,
              word->cclass_code,
              word->cclass_native,
              word->freq,
              word->okuri,
              word->annotation);

  if (fd->fail)
    return 0;
  if (!word->phon || !*word->phon || !word->desc || !*word->desc)
    return 0;

  /* like Anthy, a duplicate only updates the frequency */
  found = fake_dict_find_stored(dict, word->phon, word->desc);
  if (found) {
    found->freq = word->freq;
    return 1;
  }

  fake_dict_store(dict,
                  word->phon,
                  word->desc,
                  word->cclass_code,
                  word->cclass_native,
                  word->freq);
  return 1;
}

static int
fake_dict_change_word(uim_dict *dict, uim_word *word)
{
  fake_dict_stats_data.n_change++;
  return ((fake_dict *)dict)->fail ? 0 : 1;
}

static int
fake_dict_remove_word(uim_dict *dict, uim_word *word)
{
  fake_dict *fd = (fake_dict *)dict;
  uim_word **pos;

  fake_dict_stats_data.n_remove++;

  if (fd->fail)
    return 0;

  for (pos = &fd->stored; *pos; pos = &(*pos)->next) {
    uim_word *w = *pos;

    if (!strcmp(w->phon, word->phon) && !strcmp(w->desc, word->desc)) {
      *pos = w->next;
      w->next = NULL;
      word_free_list(w);
      return 1;
    }
  }
  return 0;
}

static void
fake_dict_refresh(uim_dict *dict)
{
  fake_dict *fd = (fake_dict *)dict;
  uim_word *w;

  fake_dict_stats_data.n_refresh++;

  word_free_list(dict->word_list);
  dict->word_list = NULL;
  for (w = fd->stored; w; w = w->next) {
    word_append(&dict->word_list,
                w->type,
                w->charset,
                w->phon,
                w->desc,
                w->cclass_code,
                w->cclass_native,
                w->freq,
                w->okuri,
                w->annotation);
  }
}
