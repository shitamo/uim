/*
 *  Copyright (c) 2003,2004 Masahito Omote <omote@utyuuzin.net>
 *                2005-2026 uim Project https://github.com/uim/uim
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of authors nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>

#include <gtk/gtk.h>

#include "gettext.h"

#include "word-list-view-gtk4.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

/*
 * WordListItem: one row of the list -- a GObject wrapper around a
 * uim_word (GListModel items must be GObjects). The displayed values
 * are copies converted to UTF-8 and exposed as properties, so the
 * column sorters can reach them through GtkPropertyExpression.
 */
#define WORD_LIST_ITEM_TYPE (word_list_item_get_type())
G_DECLARE_FINAL_TYPE(WordListItem, word_list_item, WORD_LIST, ITEM, GObject)

struct _WordListItem
{
  GObject parent_instance;

  gint word_type;
  gchar *phon;
  gchar *desc;
  gchar *cclass_code;
  gint freq;
  gboolean okuri;

  uim_word *word; /* not owned; it belongs to the uim_dict */
};

enum {
  ITEM_PROP_0,
  ITEM_PROP_WORD_TYPE,
  ITEM_PROP_PHON,
  ITEM_PROP_DESC,
  ITEM_PROP_CCLASS_CODE,
  ITEM_PROP_FREQ,
  ITEM_PROP_OKURI,
  ITEM_N_PROPS
};

static GParamSpec *item_props[ITEM_N_PROPS];

G_DEFINE_FINAL_TYPE(WordListItem, word_list_item, G_TYPE_OBJECT)

/* GtkListStore accepted NULL strings (shown as empty cells), but the
 * labels the strings are bound to don't, so keep "" instead */
static gchar *
dup_nonnull(const gchar *str)
{
  return g_strdup(str ? str : "");
}

static gchar *
take_nonnull(gchar *str)
{
  return str ? str : g_strdup("");
}

static void
word_list_item_finalize(GObject *object)
{
  WordListItem *item = WORD_LIST_ITEM(object);

  g_free(item->phon);
  g_free(item->desc);
  g_free(item->cclass_code);

  G_OBJECT_CLASS(word_list_item_parent_class)->finalize(object);
}

static void
word_list_item_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
  WordListItem *item = WORD_LIST_ITEM(object);

  switch (prop_id) {
  case ITEM_PROP_WORD_TYPE:
    item->word_type = g_value_get_int(value);
    break;
  case ITEM_PROP_PHON:
    g_free(item->phon);
    item->phon = dup_nonnull(g_value_get_string(value));
    break;
  case ITEM_PROP_DESC:
    g_free(item->desc);
    item->desc = dup_nonnull(g_value_get_string(value));
    break;
  case ITEM_PROP_CCLASS_CODE:
    g_free(item->cclass_code);
    item->cclass_code = dup_nonnull(g_value_get_string(value));
    break;
  case ITEM_PROP_FREQ:
    item->freq = g_value_get_int(value);
    break;
  case ITEM_PROP_OKURI:
    item->okuri = g_value_get_boolean(value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static void
word_list_item_get_property(GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
  WordListItem *item = WORD_LIST_ITEM(object);

  switch (prop_id) {
  case ITEM_PROP_WORD_TYPE:
    g_value_set_int(value, item->word_type);
    break;
  case ITEM_PROP_PHON:
    g_value_set_string(value, item->phon);
    break;
  case ITEM_PROP_DESC:
    g_value_set_string(value, item->desc);
    break;
  case ITEM_PROP_CCLASS_CODE:
    g_value_set_string(value, item->cclass_code);
    break;
  case ITEM_PROP_FREQ:
    g_value_set_int(value, item->freq);
    break;
  case ITEM_PROP_OKURI:
    g_value_set_boolean(value, item->okuri);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static void
word_list_item_class_init(WordListItemClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

  gobject_class->finalize = word_list_item_finalize;
  gobject_class->set_property = word_list_item_set_property;
  gobject_class->get_property = word_list_item_get_property;

  item_props[ITEM_PROP_WORD_TYPE] = g_param_spec_int("word-type",
                                                     NULL,
                                                     NULL,
                                                     G_MININT,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READWRITE);
  item_props[ITEM_PROP_PHON] =
    g_param_spec_string("phon", NULL, NULL, NULL, G_PARAM_READWRITE);
  item_props[ITEM_PROP_DESC] =
    g_param_spec_string("desc", NULL, NULL, NULL, G_PARAM_READWRITE);
  item_props[ITEM_PROP_CCLASS_CODE] =
    g_param_spec_string("cclass-code", NULL, NULL, NULL, G_PARAM_READWRITE);
  item_props[ITEM_PROP_FREQ] = g_param_spec_int("freq",
                                                NULL,
                                                NULL,
                                                G_MININT,
                                                G_MAXINT,
                                                0,
                                                G_PARAM_READWRITE);
  item_props[ITEM_PROP_OKURI] =
    g_param_spec_boolean("okuri", NULL, NULL, FALSE, G_PARAM_READWRITE);

  g_object_class_install_properties(gobject_class, ITEM_N_PROPS, item_props);
}

static void
word_list_item_init(WordListItem *item)
{
  item->phon = g_strdup("");
  item->desc = g_strdup("");
  item->cclass_code = g_strdup("");
}

static WordListItem *
word_list_item_new(uim_word *w)
{
  WordListItem *item = g_object_new(WORD_LIST_ITEM_TYPE, NULL);

  item->word = w;
  item->word_type = w->type;
  item->phon = take_nonnull(charset_convert(w->phon, w->charset, "UTF-8"));
  item->desc = take_nonnull(charset_convert(w->desc, w->charset, "UTF-8"));
  item->cclass_code =
    take_nonnull(charset_convert(w->cclass_code, w->charset, "UTF-8"));
  item->freq = w->freq;
  item->okuri = w->okuri;

  return item;
}

/*
 * WordListView
 */
static void
word_list_view_class_init(WordListViewClass *klass);
static void
word_list_view_init(WordListView *view);
static void
word_list_view_finalize(GObject *object);
static void
word_list_view_dispose(GObject *object);

static void
word_list_view_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec);
static void
word_list_view_get_property(GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec);

static GtkWidgetClass *parent_class = NULL;

enum {
  PROP_0,
  PROP_CCLASS_CODE_SHOW,
  PROP_FREQ_SHOW,
  PROP_OKURI_SHOW,
  PROP_EDITABLE,
  PROP_SELECTION_MODE
};

enum {
  SELECTION_CHANGED_SIGNAL,
  LAST_SIGNAL
};

static guint word_list_view_signals[LAST_SIGNAL] = {0};

GType
word_list_view_get_type(void)
{
  static GType type = 0;

  if (type == 0) {
    static const GTypeInfo info = {
      sizeof(WordListViewClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      (GClassInitFunc)word_list_view_class_init,
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof(WordListView),
      0,                                     /* n_preallocs */
      (GInstanceInitFunc)word_list_view_init /* instance_init */
    };
    type = g_type_register_static(GTK_TYPE_WIDGET, "WordListView", &info, 0);
  }
  return type;
}

static void
word_list_view_class_init(WordListViewClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  GtkWidgetClass *object_class = GTK_WIDGET_CLASS(klass);

  parent_class = g_type_class_peek_parent(klass);

  gobject_class->get_property = word_list_view_get_property;
  gobject_class->set_property = word_list_view_set_property;
  gobject_class->finalize = word_list_view_finalize;
  gobject_class->dispose = word_list_view_dispose;

  /* WordListView wraps a single GtkScrolledWindow child (see the
   * header comment) instead of being one; BinLayout gives that one
   * child the widget's whole allocation, matching how
   * GtkScrolledWindow itself lays out when it *was* the base class. */
  gtk_widget_class_set_layout_manager_type(object_class, GTK_TYPE_BIN_LAYOUT);

  word_list_view_signals[SELECTION_CHANGED_SIGNAL] =
    g_signal_new("selection-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(WordListViewClass, selection_changed),
                 NULL,
                 NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);

  g_object_class_install_property(
    gobject_class,
    PROP_CCLASS_CODE_SHOW,
    g_param_spec_boolean("cclass_code_show",
                         _("Show part of speech column"),
                         _("to be written"),
                         FALSE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(
    gobject_class,
    PROP_FREQ_SHOW,
    g_param_spec_boolean("freq_show",
                         _("Show freq column"),
                         _("to be written"),
                         FALSE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(
    gobject_class,
    PROP_OKURI_SHOW,
    g_param_spec_boolean("okuri_show",
                         _("Show okuri column"),
                         _("to be written"),
                         FALSE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(
    gobject_class,
    PROP_EDITABLE,
    g_param_spec_boolean("editable",
                         _("to be written"),
                         _("to be written"),
                         FALSE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(
    gobject_class,
    PROP_SELECTION_MODE,
    g_param_spec_enum("selection_mode",
                      _("to be written"),
                      _("to be written"),
                      GTK_TYPE_SELECTION_MODE,
                      GTK_SELECTION_SINGLE,
                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

/*
 * cell factories
 *
 * Each column has its own GtkSignalListItemFactory carrying, as
 * object data, the WordListItem property it shows ("property") and
 * whether its cells honor the view's "editable" property
 * ("editable"). Editable cells are GtkEditableLabels whose committed
 * text is written back to the row object, like the "edited" handler
 * of the old GtkCellRendererText did with the GtkListStore.
 */
static void
cell_editing_changed_cb(GtkEditableLabel *label,
                        GParamSpec *pspec,
                        GtkListItemFactory *factory)
{
  WordListItem *item;
  const gchar *property, *text;
  GParamSpec *item_pspec;

  if (gtk_editable_label_get_editing(label))
    return;

  item = g_object_get_data(G_OBJECT(label), "item");
  if (!item)
    return;

  property = g_object_get_data(G_OBJECT(factory), "property");
  text = gtk_editable_get_text(GTK_EDITABLE(label));
  item_pspec = g_object_class_find_property(G_OBJECT_GET_CLASS(item), property);

  if (G_PARAM_SPEC_VALUE_TYPE(item_pspec) == G_TYPE_INT)
    g_object_set(item, property, atoi(text), NULL);
  else
    g_object_set(item, property, text, NULL);
}

static void
cell_setup_cb(GtkSignalListItemFactory *factory,
              GObject *object,
              WordListView *view)
{
  GtkWidget *child;

  if (g_object_get_data(G_OBJECT(factory), "editable")) {
    child = gtk_editable_label_new("");
    /* a non-editable view must not let a click or a key press start
     * editing the cell; it then just selects the row */
    g_object_bind_property(view,
                           "editable",
                           child,
                           "editable",
                           G_BINDING_SYNC_CREATE);
    g_object_bind_property(view,
                           "editable",
                           child,
                           "can-target",
                           G_BINDING_SYNC_CREATE);
    g_object_bind_property(view,
                           "editable",
                           child,
                           "focusable",
                           G_BINDING_SYNC_CREATE);
    g_signal_connect(child,
                     "notify::editing",
                     G_CALLBACK(cell_editing_changed_cb),
                     factory);
  } else {
    child = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(child), 0.0);
  }
  gtk_widget_set_halign(child, GTK_ALIGN_FILL);
  gtk_column_view_cell_set_child(GTK_COLUMN_VIEW_CELL(object), child);
}

static void
cell_bind_cb(GtkSignalListItemFactory *factory,
             GObject *object,
             WordListView *view)
{
  GtkColumnViewCell *cell = GTK_COLUMN_VIEW_CELL(object);
  GtkWidget *child = gtk_column_view_cell_get_child(cell);
  GObject *item = gtk_column_view_cell_get_item(cell);
  const gchar *property = g_object_get_data(G_OBJECT(factory), "property");
  GBinding *binding;

  /* int and boolean properties are shown through GValue's default
   * transformations to strings, as GtkCellRendererText did */
  binding = g_object_bind_property(item,
                                   property,
                                   child,
                                   GTK_IS_LABEL(child) ? "label" : "text",
                                   G_BINDING_SYNC_CREATE);
  g_object_set_data(G_OBJECT(child), "binding", binding);
  g_object_set_data(G_OBJECT(child), "item", item);
}

static void
cell_unbind_cb(GtkSignalListItemFactory *factory,
               GObject *object,
               WordListView *view)
{
  GtkWidget *child =
    gtk_column_view_cell_get_child(GTK_COLUMN_VIEW_CELL(object));
  GBinding *binding = g_object_get_data(G_OBJECT(child), "binding");

  if (binding)
    g_binding_unbind(binding);
  g_object_set_data(G_OBJECT(child), "binding", NULL);
  g_object_set_data(G_OBJECT(child), "item", NULL);
}

static GtkColumnViewColumn *
word_list_view_append_column(WordListView *view,
                             const gchar *title,
                             const gchar *property,
                             gboolean editable,
                             GtkSorter *sorter)
{
  GtkListItemFactory *factory;
  GtkColumnViewColumn *column;

  factory = gtk_signal_list_item_factory_new();
  g_object_set_data(G_OBJECT(factory), "property", (gpointer)property);
  g_object_set_data(G_OBJECT(factory), "editable", GINT_TO_POINTER(editable));
  g_signal_connect(factory, "setup", G_CALLBACK(cell_setup_cb), view);
  g_signal_connect(factory, "bind", G_CALLBACK(cell_bind_cb), view);
  g_signal_connect(factory, "unbind", G_CALLBACK(cell_unbind_cb), view);

  column = gtk_column_view_column_new(title, factory);
  if (sorter) {
    gtk_column_view_column_set_sorter(column, sorter);
    g_object_unref(sorter);
  }
  gtk_column_view_append_column(view->view, column);
  /* the view holds its own reference; the WordListView keeps a
   * borrowed pointer, valid as long as the view */
  g_object_unref(column);

  return column;
}

static GtkSorter *
string_sorter_new(const gchar *property)
{
  GtkStringSorter *sorter = gtk_string_sorter_new(
    gtk_property_expression_new(WORD_LIST_ITEM_TYPE, NULL, property));

  /* GtkTreeSortable's default sort is case sensitive */
  gtk_string_sorter_set_ignore_case(sorter, FALSE);

  return GTK_SORTER(sorter);
}

static GtkSorter *
numeric_sorter_new(const gchar *property)
{
  return GTK_SORTER(gtk_numeric_sorter_new(
    gtk_property_expression_new(WORD_LIST_ITEM_TYPE, NULL, property)));
}

static void
selection_model_changed_cb(GtkSelectionModel *model,
                           guint position,
                           guint n_items,
                           WordListView *view)
{
  g_signal_emit(view, word_list_view_signals[SELECTION_CHANGED_SIGNAL], 0);
}

static void
selection_model_items_changed_cb(GListModel *model,
                                 guint position,
                                 guint removed,
                                 guint added,
                                 WordListView *view)
{
  /* GtkTreeSelection emitted "changed" when selected rows went away;
   * GtkSelectionModel doesn't always emit "selection-changed" then,
   * so forward any change of the rows as well. */
  g_signal_emit(view, word_list_view_signals[SELECTION_CHANGED_SIGNAL], 0);
}

static GtkSelectionModel *
word_list_view_create_selection(WordListView *view, GtkSelectionMode mode)
{
  GListModel *model = G_LIST_MODEL(g_object_ref(view->sort_model));
  GtkSelectionModel *selection;

  switch (mode) {
  case GTK_SELECTION_NONE:
    selection = GTK_SELECTION_MODEL(gtk_no_selection_new(model));
    break;
  case GTK_SELECTION_MULTIPLE:
    selection = GTK_SELECTION_MODEL(gtk_multi_selection_new(model));
    break;
  case GTK_SELECTION_BROWSE:
    {
      GtkSingleSelection *single = gtk_single_selection_new(model);

      gtk_single_selection_set_autoselect(single, TRUE);
      gtk_single_selection_set_can_unselect(single, FALSE);
      selection = GTK_SELECTION_MODEL(single);
      break;
    }
  case GTK_SELECTION_SINGLE:
  default:
    {
      GtkSingleSelection *single = gtk_single_selection_new(model);

      gtk_single_selection_set_autoselect(single, FALSE);
      gtk_single_selection_set_can_unselect(single, TRUE);
      selection = GTK_SELECTION_MODEL(single);
      break;
    }
  }

  g_signal_connect_object(selection,
                          "selection-changed",
                          G_CALLBACK(selection_model_changed_cb),
                          view,
                          0);
  g_signal_connect_object(selection,
                          "items-changed",
                          G_CALLBACK(selection_model_items_changed_cb),
                          view,
                          0);

  return selection;
}

static void
word_list_view_init(WordListView *view)
{
  GtkWidget *columnview;

  gtk_widget_set_focusable(GTK_WIDGET(view), TRUE);
  gtk_widget_set_receives_default(GTK_WIDGET(view), TRUE);

  view->scrolled_window = gtk_scrolled_window_new();
  gtk_widget_set_parent(view->scrolled_window, GTK_WIDGET(view));
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(view->scrolled_window),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW(view->scrolled_window),
                                    TRUE);

  columnview = gtk_column_view_new(NULL);
  view->view = GTK_COLUMN_VIEW(columnview);
  gtk_column_view_set_reorderable(view->view, FALSE);

  /* GtkContainer is gone in GTK 4; a GtkScrolledWindow now takes its
   * single child directly. */
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(view->scrolled_window),
                                columnview);

  /* Word Type */
  view->word_type_column = word_list_view_append_column(view,
                                                        _("Word Type"),
                                                        "word-type",
                                                        FALSE,
                                                        NULL);
  gtk_column_view_column_set_visible(view->word_type_column, FALSE);

  /* Phon */
  view->phon_column = word_list_view_append_column(view,
                                                   _("Phonetic"),
                                                   "phon",
                                                   TRUE,
                                                   string_sorter_new("phon"));
  gtk_column_view_column_set_resizable(view->phon_column, TRUE);

  /* Desc */
  view->desc_column = word_list_view_append_column(view,
                                                   _("Literal"),
                                                   "desc",
                                                   TRUE,
                                                   string_sorter_new("desc"));
  gtk_column_view_column_set_resizable(view->desc_column, TRUE);

  /* CClass (part of speech) Code */
  view->cclass_code_column =
    word_list_view_append_column(view,
                                 _("Part of Speech"),
                                 "cclass-code",
                                 TRUE,
                                 string_sorter_new("cclass-code"));
  gtk_column_view_column_set_visible(view->cclass_code_column, FALSE);
  gtk_column_view_column_set_resizable(view->cclass_code_column, TRUE);

  /* Freq */
  view->freq_column = word_list_view_append_column(view,
                                                   _("Frequency"),
                                                   "freq",
                                                   TRUE,
                                                   numeric_sorter_new("freq"));
  gtk_column_view_column_set_visible(view->freq_column, FALSE);
  gtk_column_view_column_set_resizable(view->freq_column, TRUE);

  /* Okuri */
  view->okuri_column =
    word_list_view_append_column(view,
                                 _("Okuri"),
                                 "okuri",
                                 FALSE,
                                 numeric_sorter_new("okuri"));
  gtk_column_view_column_set_visible(view->okuri_column, FALSE);
  gtk_column_view_column_set_resizable(view->okuri_column, TRUE);

  view->store = g_list_store_new(WORD_LIST_ITEM_TYPE);
  view->sort_model = gtk_sort_list_model_new(
    G_LIST_MODEL(g_object_ref(view->store)),
    g_object_ref(gtk_column_view_get_sorter(view->view)));

  view->selection = word_list_view_create_selection(view, GTK_SELECTION_SINGLE);
  gtk_column_view_set_model(view->view, view->selection);
  /* the column view keeps its own reference */
  g_object_unref(view->selection);

  view->cclass_code_show = FALSE;
  view->freq_show = FALSE;
  view->okuri_show = FALSE;
  view->editable = FALSE;
  view->selection_mode = GTK_SELECTION_SINGLE;
}

static void
word_list_view_finalize(GObject *object)
{
  if (G_OBJECT_CLASS(parent_class)->finalize)
    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
word_list_view_dispose(GObject *object)
{
  WordListView *view = WORD_LIST_VIEW(object);

  if (view->dict) {
    uim_dict_unref(view->dict);
    view->dict = NULL;
  }

  /* GtkWidgetClass has no "destroy" vfunc in GTK 4; a custom widget
   * unparents its own children from dispose() instead (dispose can
   * run more than once, so g_clear_pointer's NULL-then-skip is what
   * makes that safe here). Unparenting drops the column view, and
   * with it the selection model it owned. */
  g_clear_pointer(&view->scrolled_window, gtk_widget_unparent);
  view->view = NULL;
  view->selection = NULL;
  view->word_type_column = NULL;
  view->phon_column = NULL;
  view->desc_column = NULL;
  view->cclass_code_column = NULL;
  view->freq_column = NULL;
  view->okuri_column = NULL;
  g_clear_object(&view->sort_model);
  g_clear_object(&view->store);

  G_OBJECT_CLASS(parent_class)->dispose(object);
}

GtkWidget *
word_list_view_new(void)
{
  return GTK_WIDGET(g_object_new(WORD_LIST_VIEW_TYPE, NULL));
}

/**
 * word_list_view_new_with_properties:
 * @first_property_name: name of first property to set
 * @Varargs: value of first property, followed by more properties,
 * %NULL-terminated
 *
 * This is a function for creating a word_list_view widget and setting
 * its properties in one go. For example you might write:
 * <literal>word_list_view_new ("editable", TRUE, "freq_show", FALSE, NULL)
 * </literal> to create a word_list_view with setting editable and hide
 * code column.
 *
 * Return value: a new #GtkWidget of word_list_view.
 **/
GtkWidget *
word_list_view_new_with_attributes(const gchar *first_property_name, ...)
{
  GtkWidget *widget;
  va_list args;

  va_start(args, first_property_name);
  widget = GTK_WIDGET(
    g_object_new_valist(WORD_LIST_VIEW_TYPE, first_property_name, args));
  va_end(args);

  return widget;
}

void
word_list_view_set_visible_cclass_code_column(WordListView *view, gboolean show)
{
  g_return_if_fail(IS_WORD_LIST_VIEW(view));

  if (view->cclass_code_show != show) {
    view->cclass_code_show = show;
    gtk_column_view_column_set_visible(view->cclass_code_column, show);
    g_object_notify(G_OBJECT(view), "cclass_code_show");
  }
}

gboolean
word_list_view_get_visible_cclass_code_column(WordListView *view)
{
  g_return_val_if_fail(IS_WORD_LIST_VIEW(view), FALSE);

  return view->cclass_code_show;
}

void
word_list_view_set_visible_freq_column(WordListView *view, gboolean show)
{
  g_return_if_fail(IS_WORD_LIST_VIEW(view));

  if (view->freq_show != show) {
    view->freq_show = show;
    gtk_column_view_column_set_visible(view->freq_column, show);
    g_object_notify(G_OBJECT(view), "freq_show");
  }
}

gboolean
word_list_view_get_visible_freq_column(WordListView *view)
{
  g_return_val_if_fail(IS_WORD_LIST_VIEW(view), FALSE);

  return view->freq_show;
}

void
word_list_view_set_visible_okuri_column(WordListView *view, gboolean show)
{
  g_return_if_fail(IS_WORD_LIST_VIEW(view));

  if (view->okuri_show != show) {
    view->okuri_show = show;
    gtk_column_view_column_set_visible(view->okuri_column, show);
    g_object_notify(G_OBJECT(view), "okuri_show");
  }
}

gboolean
word_list_view_get_visible_okuri_column(WordListView *view)
{
  g_return_val_if_fail(IS_WORD_LIST_VIEW(view), FALSE);

  return view->okuri_show;
}

void
word_list_view_set_editable(WordListView *view, gboolean editable)
{
  g_return_if_fail(IS_WORD_LIST_VIEW(view));

  editable = editable ? TRUE : FALSE;
  if (view->editable != editable) {
    view->editable = editable;
    /* the editable cells follow this notification through their
     * property bindings (see cell_setup_cb()) */
    g_object_notify(G_OBJECT(view), "editable");
  }
}

gboolean
word_list_view_get_editable(WordListView *view)
{
  g_return_val_if_fail(IS_WORD_LIST_VIEW(view), FALSE);

  return view->editable;
}

void
word_list_view_set_selection_mode(WordListView *view, GtkSelectionMode mode)
{
  g_return_if_fail(IS_WORD_LIST_VIEW(view));

  if (view->selection_mode != mode) {
    view->selection_mode = mode;
    view->selection = word_list_view_create_selection(view, mode);
    gtk_column_view_set_model(view->view, view->selection);
    g_object_unref(view->selection);
    g_object_notify(G_OBJECT(view), "selection_mode");
    g_signal_emit(view, word_list_view_signals[SELECTION_CHANGED_SIGNAL], 0);
  }
}

GtkSelectionMode
word_list_view_get_selection_mode(WordListView *view)
{
  g_return_val_if_fail(IS_WORD_LIST_VIEW(view), GTK_SELECTION_SINGLE);

  return view->selection_mode;
}

static void
word_list_view_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
  WordListView *view = WORD_LIST_VIEW(object);

  switch (prop_id) {
  case PROP_CCLASS_CODE_SHOW:
    word_list_view_set_visible_cclass_code_column(view,
                                                  g_value_get_boolean(value));
    break;
  case PROP_FREQ_SHOW:
    word_list_view_set_visible_freq_column(view, g_value_get_boolean(value));
    break;
  case PROP_OKURI_SHOW:
    word_list_view_set_visible_okuri_column(view, g_value_get_boolean(value));
    break;
  case PROP_EDITABLE:
    word_list_view_set_editable(view, g_value_get_boolean(value));
    break;
  case PROP_SELECTION_MODE:
    word_list_view_set_selection_mode(view, g_value_get_enum(value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static void
word_list_view_get_property(GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
  WordListView *view = WORD_LIST_VIEW(object);

  switch (prop_id) {
  case PROP_CCLASS_CODE_SHOW:
    g_value_set_boolean(value, view->cclass_code_show);
    break;
  case PROP_FREQ_SHOW:
    g_value_set_boolean(value, view->freq_show);
    break;
  case PROP_OKURI_SHOW:
    g_value_set_boolean(value, view->okuri_show);
    break;
  case PROP_EDITABLE:
    g_value_set_boolean(value, view->editable);
    break;
  case PROP_SELECTION_MODE:
    g_value_set_enum(value, view->selection_mode);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

void
word_list_view_set_dict(WordListView *view, uim_dict *dict)
{
  g_return_if_fail(IS_WORD_LIST_VIEW(view));
  g_return_if_fail(dict != NULL);

  word_list_view_clear(view);

  if (view->dict)
    uim_dict_unref(view->dict);

  view->dict = dict;

  if (view->dict) {
    uim_dict_ref(view->dict);
    if (view->dict->word_list)
      word_list_view_set_values(view, view->dict->word_list);
  }
}

void
word_list_view_set_values(WordListView *view, uim_word *data)
{
  GPtrArray *items;
  uim_word *w;

  g_return_if_fail(IS_WORD_LIST_VIEW(view));
  g_return_if_fail(data != NULL);

  items = g_ptr_array_new_with_free_func(g_object_unref);

  for (w = data; w; w = w->next) {
    if (w->phon == NULL)
      w->phon = g_strdup("");
    if (w->desc == NULL)
      w->desc = g_strdup("");
    if (w->cclass_code == NULL)
      w->cclass_code = g_strdup("");

    g_ptr_array_add(items, word_list_item_new(w));
  }

  /* append them all at once: one "items-changed" instead of one per
   * word */
  g_list_store_splice(view->store,
                      g_list_model_get_n_items(G_LIST_MODEL(view->store)),
                      0,
                      items->pdata,
                      items->len);
  g_ptr_array_unref(items);
}

void
word_list_view_clear(WordListView *view)
{
  g_return_if_fail(IS_WORD_LIST_VIEW(view));
  g_return_if_fail(view->store);

  g_list_store_remove_all(view->store);
}

void
word_list_view_refresh(WordListView *view)
{
  g_return_if_fail(IS_WORD_LIST_VIEW(view));
  g_return_if_fail(view->store);

  g_list_store_remove_all(view->store);

  if (view->dict) {
    uim_dict_refresh(view->dict);
    if (view->dict->word_list)
      word_list_view_set_values(view, view->dict->word_list);
  }
}

GList *
word_list_view_get_all_data_list(WordListView *view)
{
  GListModel *model;
  GList *list = NULL;
  guint i, n_items;

  g_return_val_if_fail(IS_WORD_LIST_VIEW(view), NULL);

  /* in display (i.e. sorted) order, as GtkTreeModel iteration was */
  model = G_LIST_MODEL(view->sort_model);
  n_items = g_list_model_get_n_items(model);
  for (i = 0; i < n_items; i++) {
    WordListItem *item = g_list_model_get_item(model, i);

    if (item->word)
      list = g_list_prepend(list, item->word);
    g_object_unref(item);
  }
  return g_list_reverse(list);
}

/* the selected row objects, in display order (transfer full) */
static GList *
word_list_view_get_selected_items(WordListView *view)
{
  GtkBitset *selected;
  GtkBitsetIter iter;
  guint position;
  GList *items = NULL;

  selected = gtk_selection_model_get_selection(view->selection);
  if (gtk_bitset_iter_init_first(&iter, selected, &position)) {
    do {
      items = g_list_prepend(
        items,
        g_list_model_get_item(G_LIST_MODEL(view->selection), position));
    } while (gtk_bitset_iter_next(&iter, &position));
  }
  gtk_bitset_unref(selected);

  return g_list_reverse(items);
}

gboolean
word_list_view_has_selection(WordListView *view)
{
  GtkBitset *selected;
  gboolean empty;

  g_return_val_if_fail(IS_WORD_LIST_VIEW(view), FALSE);

  selected = gtk_selection_model_get_selection(view->selection);
  empty = gtk_bitset_is_empty(selected);
  gtk_bitset_unref(selected);

  return !empty;
}

GList *
word_list_view_get_selected_data_list(WordListView *view)
{
  GList *items, *pos;
  GList *selected_data = NULL;

  g_return_val_if_fail(IS_WORD_LIST_VIEW(view), NULL);

  items = word_list_view_get_selected_items(view);
  for (pos = items; pos != NULL; pos = g_list_next(pos)) {
    WordListItem *item = pos->data;

    if (item->word)
      selected_data = g_list_prepend(selected_data, item->word);
  }
  g_list_free_full(items, g_object_unref);

  return g_list_reverse(selected_data);
}

void
word_list_view_remove_selected_data(WordListView *view)
{
  GList *items, *pos;

  g_return_if_fail(IS_WORD_LIST_VIEW(view));

  /* positions shift as rows are removed (and the selection and store
   * orders differ when sorted), so look each object up in the store */
  items = word_list_view_get_selected_items(view);
  for (pos = items; pos != NULL; pos = g_list_next(pos)) {
    guint position;

    if (g_list_store_find(view->store, pos->data, &position))
      g_list_store_remove(view->store, position);
  }
  g_list_free_full(items, g_object_unref);

  gtk_selection_model_unselect_all(view->selection);
}
