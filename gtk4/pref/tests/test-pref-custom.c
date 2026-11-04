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
 * Unit tests for the non-GUI side of uim-pref-gtk4: how it reads the
 * group/custom structure it lays out, and how it reads, converts,
 * validates and stores values through libuim-custom. Each test edits a
 * struct uim_custom the same way the widget callbacks in
 * gtk-custom-widgets-gtk4.c do. No display is needed.
 */

#include <config.h>

#include "pref-test-common.h"

static gboolean
strv_contains(char **strv, const char *str)
{
  for (; strv && *strv; strv++)
    if (!strcmp(*strv, str))
      return TRUE;
  return FALSE;
}

static guint
strv_length(char **strv)
{
  guint n = 0;

  for (; strv && *strv; strv++)
    n++;
  return n;
}

/* Restore every fixture custom to its default between tests. */
static void
reset_fixture(void)
{
  uim_scm_eval_c_string("(for-each (lambda (sym) (custom-set-value! sym "
                        "(custom-default-value sym)))"
                        " (custom-collect-by-group 'pref-test))");
}

static void
test_groups(void)
{
  char **groups, **subgroups, **syms;
  struct uim_custom_group *group;

  /* create_pref_treeview() lists the primary groups */
  groups = uim_custom_primary_groups();
  g_assert_true(strv_contains(groups, "pref-test"));
  g_assert_false(strv_contains(groups, "pref-test-sub"));
  uim_custom_symbol_list_free(groups);

  group = uim_custom_group_get("pref-test");
  g_assert_nonnull(group);
  g_assert_cmpstr(group->symbol, ==, "pref-test");
  g_assert_cmpstr(group->label, ==, "Pref test");
  uim_custom_group_free(group);

  /* create_sub_group_widgets() makes one box/frame per subgroup */
  subgroups = uim_custom_group_subgroups("pref-test");
  g_assert_true(strv_contains(subgroups, "main"));
  g_assert_true(strv_contains(subgroups, "pref-test-sub"));
  uim_custom_symbol_list_free(subgroups);

  /* and collects the customs of each with the "AND expression" hack */
  syms = uim_custom_collect_by_group("pref-test 'main");
  g_assert_cmpuint(strv_length(syms), ==, 3);
  g_assert_true(strv_contains(syms, "pref-test-bool"));
  g_assert_true(strv_contains(syms, "pref-test-int"));
  g_assert_true(strv_contains(syms, "pref-test-str"));
  uim_custom_symbol_list_free(syms);

  syms = uim_custom_collect_by_group("pref-test 'pref-test-sub");
  g_assert_cmpuint(strv_length(syms), ==, 6);
  g_assert_false(strv_contains(syms, "pref-test-bool"));
  g_assert_true(strv_contains(syms, "pref-test-table"));
  uim_custom_symbol_list_free(syms);

  syms = uim_custom_collect_by_group("pref-test");
  g_assert_cmpuint(strv_length(syms), ==, 9);
  uim_custom_symbol_list_free(syms);
}

static void
test_types(void)
{
  static const struct
  {
    const char *sym;
    int type;
    const char *label;
  } expected[] = {
    {"pref-test-bool", UCustom_Bool, "Test boolean"},
    {"pref-test-int", UCustom_Int, "Test integer"},
    {"pref-test-str", UCustom_Str, "Test string"},
    {"pref-test-file", UCustom_Pathname, "Test file"},
    {"pref-test-dir", UCustom_Pathname, "Test directory"},
    {"pref-test-choice", UCustom_Choice, "Test choice"},
    {"pref-test-olist", UCustom_OrderedList, "Test ordered list"},
    {"pref-test-key", UCustom_Key, "Test key"},
    {"pref-test-table", UCustom_Table, "Test table"},
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS(expected); i++) {
    struct uim_custom *custom = uim_custom_get(expected[i].sym);

    g_assert_nonnull(custom);
    g_assert_cmpstr(custom->symbol, ==, expected[i].sym);
    g_assert_cmpint(custom->type, ==, expected[i].type);
    g_assert_cmpstr(custom->label, ==, expected[i].label);
    g_assert_nonnull(custom->value);
    g_assert_nonnull(custom->default_value);
    uim_custom_free(custom);
  }
}

static void
test_bool(void)
{
  struct uim_custom *custom;

  reset_fixture();
  custom = uim_custom_get("pref-test-bool");
  g_assert_true(custom->value->as_bool);
  g_assert_true(custom->default_value->as_bool);

  custom->value->as_bool = UIM_FALSE;
  g_assert_true(uim_custom_set(custom));
  uim_custom_free(custom);

  pref_test_assert_value("pref-test-bool", "#f");
  custom = uim_custom_get("pref-test-bool");
  g_assert_false(custom->value->as_bool);
  g_assert_true(custom->default_value->as_bool);
  uim_custom_free(custom);
}

static void
test_int(void)
{
  struct uim_custom *custom;

  reset_fixture();
  custom = uim_custom_get("pref-test-int");
  g_assert_cmpint(custom->value->as_int, ==, 10);
  g_assert_cmpint(custom->range->as_int.min, ==, 0);
  g_assert_cmpint(custom->range->as_int.max, ==, 100);

  custom->value->as_int = 42;
  g_assert_true(uim_custom_set(custom));
  pref_test_assert_value("pref-test-int", "42");

  /* bounds are inclusive */
  custom->value->as_int = 0;
  g_assert_true(uim_custom_set(custom));
  custom->value->as_int = 100;
  g_assert_true(uim_custom_set(custom));
  pref_test_assert_value("pref-test-int", "100");

  /* out of range values are rejected and the old value is kept */
  custom->value->as_int = 101;
  g_assert_false(uim_custom_set(custom));
  custom->value->as_int = -1;
  g_assert_false(uim_custom_set(custom));
  pref_test_assert_value("pref-test-int", "100");
  uim_custom_free(custom);
}

static void
test_str(void)
{
  struct uim_custom *custom;

  reset_fixture();
  custom = uim_custom_get("pref-test-str");
  g_assert_cmpstr(custom->value->as_str, ==, "hello");

  /* custom_entry_changed_cb() replaces as_str like this */
  free(custom->value->as_str);
  custom->value->as_str = strdup("a \"quoted\" \\ string");
  g_assert_true(uim_custom_set(custom));
  uim_custom_free(custom);

  /* special characters survive the round trip through Scheme */
  custom = uim_custom_get("pref-test-str");
  g_assert_cmpstr(custom->value->as_str, ==, "a \"quoted\" \\ string");
  g_assert_cmpstr(custom->default_value->as_str, ==, "hello");

  free(custom->value->as_str);
  custom->value->as_str = strdup("");
  g_assert_true(uim_custom_set(custom));
  uim_custom_free(custom);
  pref_test_assert_value("pref-test-str", "\"\"");
}

static void
test_pathname(void)
{
  struct uim_custom *custom;

  reset_fixture();
  custom = uim_custom_get("pref-test-file");
  g_assert_cmpstr(custom->value->as_pathname->str, ==, "/tmp/pref-test-file");
  g_assert_cmpint(custom->value->as_pathname->type,
                  ==,
                  UCustomPathnameType_RegularFile);
  free(custom->value->as_pathname->str);
  custom->value->as_pathname->str = strdup("/nonexistent/is/fine");
  g_assert_true(uim_custom_set(custom));
  uim_custom_free(custom);
  pref_test_assert_value("pref-test-file", "\"/nonexistent/is/fine\"");

  custom = uim_custom_get("pref-test-dir");
  g_assert_cmpstr(custom->value->as_pathname->str, ==, "/tmp");
  g_assert_cmpint(custom->value->as_pathname->type,
                  ==,
                  UCustomPathnameType_Directory);
  uim_custom_free(custom);
}

static void
test_choice(void)
{
  struct uim_custom *custom;
  struct uim_custom_choice **items;

  reset_fixture();
  custom = uim_custom_get("pref-test-choice");
  g_assert_cmpstr(custom->value->as_choice->symbol, ==, "beta");
  g_assert_cmpstr(custom->value->as_choice->label, ==, "Beta");

  /* sync_value_choice() fills the combo box from valid_items in order */
  items = custom->range->as_choice.valid_items;
  g_assert_cmpstr(items[0]->symbol, ==, "alpha");
  g_assert_cmpstr(items[1]->symbol, ==, "beta");
  g_assert_cmpstr(items[2]->symbol, ==, "gamma");
  g_assert_cmpstr(items[2]->label, ==, "Gamma");
  g_assert_null(items[3]);

  free(custom->value->as_choice->symbol);
  custom->value->as_choice->symbol = strdup("gamma");
  g_assert_true(uim_custom_set(custom));
  pref_test_assert_value("pref-test-choice", "'gamma");

  /* a symbol outside the range is rejected */
  free(custom->value->as_choice->symbol);
  custom->value->as_choice->symbol = strdup("delta");
  g_assert_false(uim_custom_set(custom));
  pref_test_assert_value("pref-test-choice", "'gamma");
  uim_custom_free(custom);
}

static void
test_olist(void)
{
  struct uim_custom *custom;
  struct uim_custom_choice *tmp;

  reset_fixture();
  custom = uim_custom_get("pref-test-olist");
  g_assert_cmpstr(custom->value->as_olist[0]->symbol, ==, "one");
  g_assert_cmpstr(custom->value->as_olist[1]->symbol, ==, "two");
  g_assert_null(custom->value->as_olist[2]);
  g_assert_cmpstr(custom->range->as_olist.valid_items[2]->symbol, ==, "three");

  /* olist_pref_up_button_clicked_cb() swaps neighbours like this */
  tmp = custom->value->as_olist[0];
  custom->value->as_olist[0] = custom->value->as_olist[1];
  custom->value->as_olist[1] = tmp;
  g_assert_true(uim_custom_set(custom));
  uim_custom_free(custom);
  pref_test_assert_value("pref-test-olist", "'(two one)");

  /* an empty list is valid */
  uim_scm_eval_c_string("(custom-set-value! 'pref-test-olist '())");
  pref_test_assert_value("pref-test-olist", "'()");

  /* a symbol outside the range is rejected */
  custom = uim_custom_get("pref-test-olist");
  g_assert_null(custom->value->as_olist[0]);
  custom->value->as_olist =
    realloc(custom->value->as_olist, sizeof(struct uim_custom_choice *) * 2);
  custom->value->as_olist[0] = malloc(sizeof(struct uim_custom_choice));
  custom->value->as_olist[0]->symbol = strdup("four");
  custom->value->as_olist[0]->label = strdup("Four");
  custom->value->as_olist[0]->desc = strdup("");
  custom->value->as_olist[1] = NULL;
  g_assert_false(uim_custom_set(custom));
  uim_custom_free(custom);
  pref_test_assert_value("pref-test-olist", "'()");
}

static void
set_key_literal(struct uim_custom *custom, int idx, const char *literal)
{
  free(custom->value->as_key[idx]->literal);
  custom->value->as_key[idx]->literal = strdup(literal);
}

static void
assert_key_literals(const char *custom_sym,
                    const char *first,
                    const char *second)
{
  struct uim_custom *custom = uim_custom_get(custom_sym);

  g_assert_cmpstr(custom->value->as_key[0]->literal, ==, first);
  g_assert_cmpstr(custom->value->as_key[1]->literal, ==, second);
  g_assert_null(custom->value->as_key[2]);
  uim_custom_free(custom);
}

static void
test_key(void)
{
  struct uim_custom *custom;

  reset_fixture();
  custom = uim_custom_get("pref-test-key");
  g_assert_cmpstr(custom->value->as_key[0]->literal, ==, "<Control>j");
  g_assert_cmpstr(custom->value->as_key[1]->literal, ==, "escape");
  g_assert_null(custom->value->as_key[2]);

  set_key_literal(custom, 1, "<Shift><Alt>F1");
  g_assert_true(uim_custom_set(custom));
  assert_key_literals("pref-test-key", "<Control>j", "<Shift><Alt>F1");

  /* key_pref_set_value() writes "space" for 0x20; uim accepts it as a
   * key string and keeps it as typed */
  set_key_literal(custom, 1, "<Control>space");
  g_assert_true(uim_custom_set(custom));
  uim_custom_free(custom);
  custom = uim_custom_get("pref-test-key");
  g_assert_cmpstr(custom->value->as_key[1]->literal, ==, "<Control>space");

  /* Malformed key strings such as "<Foo>a" are not tested here:
   * uim_custom_set() passes them through gui-key-str->key-str, which
   * rewrites them before custom-key? sees them, so libuim accepts
   * them. The key editor only produces strings from key_tab and
   * key_pref_set_value(), which test-pref-keytab checks. */
  uim_custom_free(custom);
  assert_key_literals("pref-test-key", "<Control>j", "<Control>space");
}

static void
test_table(void)
{
  struct uim_custom *custom;
  char ***table;

  reset_fixture();
  custom = uim_custom_get("pref-test-table");
  table = custom->value->as_table;
  g_assert_cmpstr(table[0][0], ==, "a");
  g_assert_cmpstr(table[0][1], ==, "x");
  g_assert_null(table[0][2]);
  g_assert_cmpstr(table[1][0], ==, "b");
  g_assert_null(table[2]);

  /* create_table_tree_view() names its columns after the header */
  g_assert_cmpstr(custom->range->as_table_header.valid_items[0]->label,
                  ==,
                  "Input");
  g_assert_cmpstr(custom->range->as_table_header.valid_items[1]->label,
                  ==,
                  "Output");
  g_assert_null(custom->range->as_table_header.valid_items[2]);

  free(table[1][1]);
  table[1][1] = strdup("z");
  g_assert_true(uim_custom_set(custom));
  uim_custom_free(custom);
  pref_test_assert_value("pref-test-table", "'((\"a\" \"x\") (\"b\" \"z\"))");
}

/* uim_pref_gtk_set_default_value() ("Defaults") copies default_value
 * into value. For tables libuim's uim_custom_table_get() ignores its
 * getter argument and returns the current value for default_value as
 * well, so a table cannot be reset. Reported as TODO until libuim is
 * fixed. */
static void
test_table_default(void)
{
  struct uim_custom *custom;

  reset_fixture();
  uim_scm_eval_c_string("(custom-set-value! 'pref-test-table '((\"q\")))");
  custom = uim_custom_get("pref-test-table");
  g_assert_cmpstr(custom->value->as_table[0][0], ==, "q");
  if (g_strcmp0(custom->default_value->as_table[0][0], "a") != 0) {
    g_test_incomplete("libuim returns the current value as the default "
                      "value of a table custom");
  } else {
    g_assert_cmpstr(custom->default_value->as_table[1][1], ==, "y");
  }
  uim_custom_free(custom);
}

static void
test_activity(void)
{
  struct uim_custom *custom;

  reset_fixture();
  custom = uim_custom_get("pref-test-int");
  g_assert_true(custom->is_active);
  uim_custom_free(custom);

  uim_scm_eval_c_string("(custom-set-value! 'pref-test-bool #f)");
  custom = uim_custom_get("pref-test-int");
  g_assert_false(custom->is_active);
  uim_custom_free(custom);

  custom = uim_custom_get("pref-test-str");
  g_assert_true(custom->is_active);
  uim_custom_free(custom);
}

static int update_cb_count;
static void *update_cb_ptr;
static char *update_cb_sym;

static void
update_cb(void *ptr, const char *custom_sym)
{
  update_cb_count++;
  update_cb_ptr = ptr;
  g_free(update_cb_sym);
  update_cb_sym = g_strdup(custom_sym);
}

/* The widgets register with uim_custom_cb_add() to follow changes made
 * elsewhere (other widgets, "Defaults", activity hooks). */
static void
test_update_callback(void)
{
  struct uim_custom *custom;
  int dummy;

  reset_fixture();
  update_cb_count = 0;
  g_assert_true(uim_custom_cb_add("pref-test-str", &dummy, update_cb));

  custom = uim_custom_get("pref-test-str");
  free(custom->value->as_str);
  custom->value->as_str = strdup("changed");
  g_assert_true(uim_custom_set(custom));
  uim_custom_free(custom);

  g_assert_cmpint(update_cb_count, >=, 1);
  g_assert_true(update_cb_ptr == &dummy);
  g_assert_cmpstr(update_cb_sym, ==, "pref-test-str");

  g_assert_true(uim_custom_cb_remove("pref-test-str"));
  update_cb_count = 0;
  uim_scm_eval_c_string("(custom-set-value! 'pref-test-str \"again\")");
  g_assert_cmpint(update_cb_count, ==, 0);

  g_clear_pointer(&update_cb_sym, g_free);
}

static void
test_save_and_load(void)
{
  char *path, *contents = NULL;
  struct uim_custom *custom;

  reset_fixture();
  uim_scm_eval_c_string("(custom-set-value! 'pref-test-int 42)");
  uim_scm_eval_c_string("(custom-set-value! 'pref-test-str \"saved\")");
  uim_scm_eval_c_string("(custom-set-value! 'pref-test-choice 'alpha)");
  uim_scm_eval_c_string("(custom-set-value! 'pref-test-olist '(three))");
  uim_scm_eval_c_string("(custom-set-value! 'pref-test-key '(\"<Alt>x\"))");

  /* ok_button_clicked() and apply_button_clicked() call this */
  g_assert_true(uim_custom_save());

  path = pref_test_custom_file("pref-test");
  g_assert_true(g_file_get_contents(path, &contents, NULL, NULL));
  g_assert_nonnull(strstr(contents, "(define pref-test-bool #t)"));
  g_assert_nonnull(strstr(contents, "(define pref-test-int 42)"));
  g_assert_nonnull(strstr(contents, "(define pref-test-str \"saved\")"));
  g_assert_nonnull(strstr(contents, "(define pref-test-choice 'alpha)"));
  g_assert_nonnull(strstr(contents, "(define pref-test-olist '(three))"));
  g_assert_nonnull(strstr(contents, "(define pref-test-key '(\"<Alt>x\"))"));
  g_assert_nonnull(
    strstr(contents,
           "(define pref-test-table '((\"a\" \"x\") (\"b\" \"y\")))"));
  g_free(contents);
  g_free(path);

  /* nothing is written outside the temporary home */
  path = g_build_filename(pref_test_home, ".uim.d", "customs", NULL);
  g_assert_true(g_file_test(path, G_FILE_TEST_IS_DIR));
  g_free(path);

  /* loading brings the saved values back */
  reset_fixture();
  pref_test_assert_value("pref-test-int", "10");
  g_assert_true(uim_custom_load());
  pref_test_assert_value("pref-test-int", "42");
  pref_test_assert_value("pref-test-choice", "'alpha");
  custom = uim_custom_get("pref-test-str");
  g_assert_cmpstr(custom->value->as_str, ==, "saved");
  uim_custom_free(custom);
  custom = uim_custom_get("pref-test-key");
  g_assert_cmpstr(custom->value->as_key[0]->literal, ==, "<Alt>x");
  g_assert_null(custom->value->as_key[1]);
  uim_custom_free(custom);
}

int
main(int argc, char *argv[])
{
  int status;

  g_test_init(&argc, &argv, NULL);

  pref_test_setup_environment();
  pref_test_init_uim();

  g_test_add_func("/pref/custom/groups", test_groups);
  g_test_add_func("/pref/custom/types", test_types);
  g_test_add_func("/pref/custom/bool", test_bool);
  g_test_add_func("/pref/custom/int", test_int);
  g_test_add_func("/pref/custom/str", test_str);
  g_test_add_func("/pref/custom/pathname", test_pathname);
  g_test_add_func("/pref/custom/choice", test_choice);
  g_test_add_func("/pref/custom/olist", test_olist);
  g_test_add_func("/pref/custom/key", test_key);
  g_test_add_func("/pref/custom/table", test_table);
  g_test_add_func("/pref/custom/table-default", test_table_default);
  g_test_add_func("/pref/custom/activity", test_activity);
  g_test_add_func("/pref/custom/update-callback", test_update_callback);
  g_test_add_func("/pref/custom/save-and-load", test_save_and_load);

  status = g_test_run();

  pref_test_quit_uim();
  return status;
}
