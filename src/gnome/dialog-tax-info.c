/********************************************************************\
 * dialog-tax-info.c -- tax information dialog                      *
 * Copyright (C) 2001 Gnumatic, Inc.                                *
 * Author: Dave Peticolas <dave@krondo.com>                         *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652       *
 * Boston, MA  02111-1307,  USA       gnu@gnu.org                   *
\********************************************************************/

#include "config.h"

#include <gnome.h>
#include <guile/gh.h>

#include "dialog-utils.h"
#include "gnc-account-tree.h"
#include "gnc-component-manager.h"
#include "gnc-engine-util.h"
#include "gnc-ui.h"
#include "messages.h"


#define DIALOG_TAX_INFO_CM_CLASS "dialog-tax-info"

/* This static indicates the debugging module that this .o belongs to.  */
/* static short module = MOD_GUI; */

static struct
{
  SCM payer_name_source;
  SCM form;
  SCM description;
  SCM help;

  SCM codes;
} getters;

typedef struct
{
  char *code;
  char *payer_name_source;
  char *form;
  char *description;
  char *help;
} TXFInfo;

typedef struct
{
  GtkWidget * dialog;

  GtkWidget * account_tree;

  GtkWidget * tax_related_button;
  GtkWidget * txf_category_clist;
  GtkWidget * txf_help_text;
  GtkWidget * current_account_button;
  GtkWidget * parent_account_button;

  GList * income_txf_infos;
  GList * expense_txf_infos;

  gboolean income;
  gboolean changed;
} TaxInfoDialog;


static gboolean getters_initialized = FALSE;
static gint last_width = 0;
static gint last_height = 0;


static void
initialize_getters (void)
{
  if (getters_initialized)
    return;

  getters.payer_name_source = gh_eval_str ("gnc:txf-get-payer-name-source");
  getters.form              = gh_eval_str ("gnc:txf-get-form");
  getters.description       = gh_eval_str ("gnc:txf-get-description");
  getters.help              = gh_eval_str ("gnc:txf-get-help");

  getters.codes             = gh_eval_str ("gnc:txf-get-codes");

  getters_initialized = TRUE;
}

static void
destroy_txf_info (gpointer data, gpointer user_data)
{
  TXFInfo *txf_info = data;

  g_free (txf_info->code);
  txf_info->code = NULL;

  g_free (txf_info->payer_name_source);
  txf_info->payer_name_source = NULL;

  g_free (txf_info->form);
  txf_info->form = NULL;

  g_free (txf_info->description);
  txf_info->description = NULL;

  g_free (txf_info->help);
  txf_info->help = NULL;

  g_free (txf_info);
}

static void
destroy_txf_infos (GList *infos)
{
  g_list_foreach (infos, destroy_txf_info, NULL);
  g_list_free (infos);
}

static void
gnc_tax_info_set_changed (TaxInfoDialog *ti_dialog, gboolean changed)
{
  GtkWidget *button;

  button = gnc_glade_lookup_widget (ti_dialog->dialog, "apply_button");
  gtk_widget_set_sensitive (button, changed);

  ti_dialog->changed = changed;
}

static GList *
load_txf_info (gboolean income)
{
  GList *infos = NULL;
  SCM category;
  SCM codes;

  initialize_getters ();

  category = gh_eval_str (income ?
                          "txf-income-categories" :
                          "txf-expense-categories");
  if (category == SCM_UNDEFINED)
  {
    destroy_txf_infos (infos);
    return NULL;
  }

  codes = gh_call1 (getters.codes, category);
  if (!gh_list_p (codes))
  {
    destroy_txf_infos (infos);
    return NULL;
  }

  while (!gh_null_p (codes))
  {
    TXFInfo *txf_info;
    SCM code_scm;
    char *str;
    SCM scm;

    code_scm  = gh_car (codes);
    codes     = gh_cdr (codes);

    scm = gh_call2 (getters.payer_name_source, category, code_scm);
    str = gh_symbol2newstr (scm, NULL);
    if (safe_strcmp (str, "not-impl") == 0)
    {
      free (str);
      continue;
    }

    txf_info = g_new0 (TXFInfo, 1);

    if (safe_strcmp (str, "none") == 0)
      txf_info->payer_name_source = NULL;
    else
      txf_info->payer_name_source = g_strdup (str);
    free (str);

    str = gh_symbol2newstr (code_scm, NULL);
    txf_info->code = g_strdup (str);
    free (str);

    scm = gh_call2 (getters.form, category, code_scm);
    str = gh_scm2newstr (scm, NULL);
    txf_info->form = g_strdup (str);
    free (str);

    scm = gh_call2 (getters.description, category, code_scm);
    str = gh_scm2newstr (scm, NULL);
    txf_info->description = g_strdup (str);
    free (str);

    scm = gh_call2 (getters.help, category, code_scm);
    str = gh_scm2newstr (scm, NULL);
    txf_info->help = g_strdup (str);
    free (str);

    infos = g_list_prepend (infos, txf_info);
  }

  return g_list_reverse (infos);
}

static GList *
tax_infos (TaxInfoDialog *ti_dialog)
{
  return
    ti_dialog->income ?
    ti_dialog->income_txf_infos : ti_dialog->expense_txf_infos;
}

static void
load_category_list (TaxInfoDialog *ti_dialog)
{
  GtkCList *clist = GTK_CLIST (ti_dialog->txf_category_clist);
  char *text[2];
  GList *codes;

  gtk_clist_freeze (clist);
  gtk_clist_clear (clist);

  codes = tax_infos (ti_dialog);

  for ( ; codes; codes = codes->next)
  {
    TXFInfo *txf_info = codes->data;

    text[0] = txf_info->form;
    text[1] = txf_info->description;

    gtk_clist_append (clist, text);
  }

  gtk_clist_thaw (clist);
}

static void
clear_gui (TaxInfoDialog *ti_dialog)
{
  gtk_toggle_button_set_active
    (GTK_TOGGLE_BUTTON (ti_dialog->tax_related_button), FALSE);

  gtk_clist_select_row (GTK_CLIST (ti_dialog->txf_category_clist), 0, 0);

  gtk_toggle_button_set_active
    (GTK_TOGGLE_BUTTON (ti_dialog->current_account_button), TRUE);
}

static TXFInfo *
txf_infos_find_code (GList *infos, const char *code)
{
  for (; infos; infos = infos->next)
  {
    TXFInfo *info = infos->data;

    if (safe_strcmp (code, info->code) == 0)
      return info;
  }

  return NULL;
}

static void
account_to_gui (TaxInfoDialog *ti_dialog, Account *account)
{
  gboolean tax_related;
  const char *str;
  TXFInfo *info;
  GList *infos;
  gint index;

  if (!account)
  {
    clear_gui (ti_dialog);
    return;
  }

  tax_related = xaccAccountGetTaxRelated (account);
  gtk_toggle_button_set_active
    (GTK_TOGGLE_BUTTON (ti_dialog->tax_related_button), tax_related);

  infos = tax_infos (ti_dialog);

  str = xaccAccountGetTaxUSCode (account);
  info = txf_infos_find_code (infos, str);
  if (info)
    index = g_list_index (infos, info);
  else
    index = 0;
  if (index < 0)
    index = 0;

  gtk_clist_select_row (GTK_CLIST (ti_dialog->txf_category_clist), index, 0);
  if (gtk_clist_row_is_visible (GTK_CLIST (ti_dialog->txf_category_clist),
                                index) != GTK_VISIBILITY_FULL)
    gtk_clist_moveto (GTK_CLIST (ti_dialog->txf_category_clist),
                      index, 0, 0.5, 0.0);

  str = xaccAccountGetTaxUSPayerNameSource (account);
  if (safe_strcmp (str, "parent") == 0)
    gtk_toggle_button_set_active
      (GTK_TOGGLE_BUTTON (ti_dialog->parent_account_button), TRUE);
  else
    gtk_toggle_button_set_active
      (GTK_TOGGLE_BUTTON (ti_dialog->current_account_button), TRUE);
}

static void
gui_to_accounts (TaxInfoDialog *ti_dialog)
{
  gboolean tax_related;
  const char *code;
  const char *pns;
  GList *accounts;
  TXFInfo *info;
  GList *infos;
  GList *node;

  tax_related = gtk_toggle_button_get_active
    (GTK_TOGGLE_BUTTON (ti_dialog->tax_related_button));

  infos = tax_infos (ti_dialog);

  info = g_list_nth_data
    (infos, GTK_CLIST (ti_dialog->txf_category_clist)->focus_row);
  g_return_if_fail (info != NULL);

  code = tax_related ? info->code : NULL;

  if (tax_related && info->payer_name_source)
  {
    gboolean current;

    current = gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (ti_dialog->current_account_button));

    pns = current ? "current" : "parent";
  }
  else
    pns = NULL;

  accounts = gnc_account_tree_get_current_accounts
    (GNC_ACCOUNT_TREE (ti_dialog->account_tree));

  for (node = accounts; node; node = node->next)
  {
    Account *account = node->data;

    xaccAccountBeginEdit (account);

    xaccAccountSetTaxRelated (account, tax_related);
    xaccAccountSetTaxUSCode (account, code);
    xaccAccountSetTaxUSPayerNameSource (account, pns);

    xaccAccountCommitEdit (account);
  }
}

static void
window_destroy_cb (GtkObject *object, gpointer data)
{
  TaxInfoDialog *ti_dialog = data;

  gnc_unregister_gui_component_by_data (DIALOG_TAX_INFO_CM_CLASS, ti_dialog);

  destroy_txf_infos (ti_dialog->income_txf_infos);
  ti_dialog->income_txf_infos = NULL;

  destroy_txf_infos (ti_dialog->expense_txf_infos);
  ti_dialog->expense_txf_infos = NULL;

  g_free (ti_dialog);
}

static void
select_subaccounts_clicked (GtkWidget *widget, gpointer data)
{
  TaxInfoDialog *ti_dialog = data;
  GNCAccountTree *tree;
  Account *account;

  tree = GNC_ACCOUNT_TREE (ti_dialog->account_tree);

  account = gnc_account_tree_get_focus_account (tree);
  if (!account)
    return;

  gnc_account_tree_select_subaccounts (tree, account, FALSE);

  gtk_widget_grab_focus (ti_dialog->account_tree);
}

static void
tax_info_ok_clicked (GtkWidget *widget, gpointer data)
{
  TaxInfoDialog *ti_dialog = data;

  if (ti_dialog->changed)
    gui_to_accounts (ti_dialog);

  gnc_close_gui_component_by_data (DIALOG_TAX_INFO_CM_CLASS, ti_dialog);
}

static void
tax_info_apply_clicked (GtkWidget *widget, gpointer data)
{
  TaxInfoDialog *ti_dialog = data;

  gui_to_accounts (ti_dialog);
  gnc_tax_info_set_changed (ti_dialog, FALSE);
}

static void
tax_info_cancel_clicked (GtkWidget *widget, gpointer data)
{
  TaxInfoDialog *ti_dialog = data;

  gnc_close_gui_component_by_data (DIALOG_TAX_INFO_CM_CLASS, ti_dialog);
}

static void
tax_info_show_income_accounts (TaxInfoDialog *ti_dialog, gboolean show_income)
{
  GNCAccountTree *tree;
  AccountViewInfo info;
  GNCAccountType type;
  GNCAccountType show_type;

  ti_dialog->income = show_income;

  tree = GNC_ACCOUNT_TREE (ti_dialog->account_tree);
  show_type = show_income ? INCOME : EXPENSE;

  gnc_account_tree_get_view_info (tree, &info);

  for (type = 0; type < NUM_ACCOUNT_TYPES; type++)
    info.include_type[type] = (type == show_type);

  gnc_account_tree_set_view_info (tree, &info);

  load_category_list (ti_dialog);
}

static int
gnc_tax_info_update_accounts (TaxInfoDialog *ti_dialog)
{
  GNCAccountTree *tree;
  GtkWidget *label;
  GtkWidget *frame;
  int num_accounts;
  GList *accounts;
  char *string;

  tree = GNC_ACCOUNT_TREE (ti_dialog->account_tree);

  accounts = gnc_account_tree_get_current_accounts (tree);

  num_accounts = g_list_length (accounts);

  label = gnc_glade_lookup_widget (ti_dialog->dialog, "num_accounts_label");
  frame = gnc_glade_lookup_widget (ti_dialog->dialog, "tax_info_frame");

  string = g_strdup_printf ("%d", num_accounts);
  gtk_label_set_text (GTK_LABEL (label), string);
  g_free (string);

  gtk_widget_set_sensitive (frame, num_accounts > 0);

  return num_accounts;
}

static void
gnc_tax_info_income_cb (GtkWidget *w, gpointer data)
{
  TaxInfoDialog *ti_dialog = data;
  gboolean show_income;

  show_income = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w));

  tax_info_show_income_accounts (ti_dialog, show_income);

  gnc_account_tree_refresh (GNC_ACCOUNT_TREE (ti_dialog->account_tree));
  gnc_account_tree_expand_all (GNC_ACCOUNT_TREE (ti_dialog->account_tree));

  gnc_tax_info_update_accounts (ti_dialog);

  clear_gui (ti_dialog);
}

static void
gnc_tax_info_select_account_cb (GNCAccountTree *tree,
                                Account *account, gpointer data)
{
  TaxInfoDialog *ti_dialog = data;

  if (gnc_tax_info_update_accounts (ti_dialog) != 1)
  {
    gnc_tax_info_set_changed (ti_dialog, TRUE);
    return;
  }

  account_to_gui (ti_dialog, account);
  gnc_tax_info_set_changed (ti_dialog, FALSE);
}

static void
gnc_tax_info_unselect_account_cb (GNCAccountTree *tree,
                                  Account *account, gpointer data)
{
  TaxInfoDialog *ti_dialog = data;
  GList *accounts;

  accounts = gnc_account_tree_get_current_accounts (tree);

  if (gnc_tax_info_update_accounts (ti_dialog) != 0)
    return;

  clear_gui (ti_dialog);
  gnc_tax_info_set_changed (ti_dialog, FALSE);
}

static void
txf_code_select_row_cb (GtkCList *clist,
                        gint row,
                        gint column,
                        GdkEventButton *event,
                        gpointer user_data)
{
  TaxInfoDialog *ti_dialog = user_data;
  TXFInfo *txf_info;
  GtkAdjustment *adj;
  GtkWidget *scroll;
  GtkWidget *frame;
  GtkEditable *ge;
  const char *text;
  gint pos = 0;

  txf_info = g_list_nth_data (tax_infos (ti_dialog), row);

  ge = GTK_EDITABLE (ti_dialog->txf_help_text);

  text = (txf_info && txf_info->help) ? txf_info->help : "";

  gtk_editable_delete_text (ge, 0, -1);
  gtk_editable_insert_text (ge, text, strlen (text), &pos);

  scroll = gnc_glade_lookup_widget (GTK_WIDGET (clist), "help_scroll");

  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scroll));
  gtk_adjustment_set_value (adj, 0.0);

  frame = gnc_glade_lookup_widget (GTK_WIDGET (clist),
                                   "payer_name_source_frame");

  if (txf_info->payer_name_source)
  {
    gboolean current;

    gtk_widget_set_sensitive (frame, TRUE);

    current = (strcmp ("current", txf_info->payer_name_source) == 0);

    if (current)
      gtk_toggle_button_set_active
        (GTK_TOGGLE_BUTTON (ti_dialog->current_account_button), TRUE);
    else
      gtk_toggle_button_set_active
        (GTK_TOGGLE_BUTTON (ti_dialog->parent_account_button), TRUE);
  }
  else
  {
    gtk_widget_set_sensitive (frame, FALSE);
    gtk_toggle_button_set_active
      (GTK_TOGGLE_BUTTON (ti_dialog->current_account_button), TRUE);
  }

  gnc_tax_info_set_changed (ti_dialog, TRUE);
}

static void
tax_related_toggled_cb (GtkToggleButton *togglebutton,
                        gpointer user_data)
{
  TaxInfoDialog *ti_dialog = user_data;
  GtkWidget *frame;
  gboolean on;

  on = gtk_toggle_button_get_active (togglebutton);

  frame = gnc_glade_lookup_widget (GTK_WIDGET (togglebutton),
                                   "txf_categories_frame");
  gtk_widget_set_sensitive (frame, on);

  gnc_tax_info_set_changed (ti_dialog, TRUE);
}

static void
current_account_toggled_cb (GtkToggleButton *togglebutton,
                            gpointer user_data)
{
  TaxInfoDialog *ti_dialog = user_data;

  gnc_tax_info_set_changed (ti_dialog, TRUE);
}

static void
gnc_tax_info_dialog_create (GtkWidget * parent, TaxInfoDialog *ti_dialog)
{
  GtkWidget *dialog;
  GtkObject *tido;
  GladeXML  *xml;

  xml = gnc_glade_xml_new ("tax.glade", "Tax Information Dialog");

  dialog = glade_xml_get_widget (xml, "Tax Information Dialog");
  ti_dialog->dialog = dialog;
  tido = GTK_OBJECT (dialog);

  ti_dialog->income_txf_infos = load_txf_info (TRUE);
  ti_dialog->expense_txf_infos = load_txf_info (FALSE);

  gnome_dialog_button_connect (GNOME_DIALOG (dialog), 0,
                               GTK_SIGNAL_FUNC (tax_info_ok_clicked),
                               ti_dialog);

  gnome_dialog_button_connect (GNOME_DIALOG (dialog), 1,
                               GTK_SIGNAL_FUNC (tax_info_apply_clicked),
                               ti_dialog);

  gnome_dialog_button_connect (GNOME_DIALOG (dialog), 2,
                               GTK_SIGNAL_FUNC (tax_info_cancel_clicked),
                               ti_dialog);

  gtk_signal_connect (tido, "destroy",
                      GTK_SIGNAL_FUNC (window_destroy_cb), ti_dialog);

  /* parent */
  if (parent != NULL)
    gnome_dialog_set_parent (GNOME_DIALOG (dialog), GTK_WINDOW (parent));

  /* default to ok */
  gnome_dialog_set_default (GNOME_DIALOG(dialog), 0);

  /* tax information */
  {
    GtkWidget *button;
    GtkWidget *clist;
    GtkWidget *text;

    button = glade_xml_get_widget (xml, "tax_related_button");
    ti_dialog->tax_related_button = button;

    gtk_signal_connect (GTK_OBJECT (button), "toggled",
                        GTK_SIGNAL_FUNC (tax_related_toggled_cb), ti_dialog);

    text = glade_xml_get_widget (xml, "txf_help_text");
    gtk_text_set_word_wrap (GTK_TEXT (text), TRUE);
    ti_dialog->txf_help_text = text;

    /* set text height */
    {
      GtkStyle *style = gtk_widget_get_style (text);
      GdkFont *font = NULL;

      if (style != NULL)
        font = style->font;

      if (font)
        gtk_widget_set_usize (text, 0, (font->ascent + font->descent) * 5 + 6);
    }

    clist = glade_xml_get_widget (xml, "txf_category_clist");
    gtk_clist_column_titles_passive (GTK_CLIST (clist));
    ti_dialog->txf_category_clist = clist;

    gtk_signal_connect (GTK_OBJECT (clist), "select-row",
                        GTK_SIGNAL_FUNC (txf_code_select_row_cb), ti_dialog);

    button = glade_xml_get_widget (xml, "current_account_button");
    ti_dialog->current_account_button = button;

    button = glade_xml_get_widget (xml, "parent_account_button");
    ti_dialog->parent_account_button = button;

    gtk_signal_connect (GTK_OBJECT (button), "toggled",
                        GTK_SIGNAL_FUNC (current_account_toggled_cb),
                        ti_dialog);
  }

  /* account tree */
  {
    GtkWidget *income_radio;
    GNCAccountTree *tree;
    GtkWidget *scroll;

    ti_dialog->account_tree = gnc_account_tree_new ();
    tree = GNC_ACCOUNT_TREE (ti_dialog->account_tree);

    gtk_clist_column_titles_hide (GTK_CLIST (ti_dialog->account_tree));
    gtk_clist_set_selection_mode (GTK_CLIST (ti_dialog->account_tree),
                                  GTK_SELECTION_EXTENDED);
    gnc_account_tree_hide_all_but_name (tree);

    tax_info_show_income_accounts (ti_dialog, FALSE);

    gnc_account_tree_refresh (tree);
    gnc_account_tree_expand_all (tree);

    gtk_signal_connect (GTK_OBJECT(tree), "select_account",
                        GTK_SIGNAL_FUNC(gnc_tax_info_select_account_cb),
                        ti_dialog);

    gtk_signal_connect (GTK_OBJECT(tree), "unselect_account",
                        GTK_SIGNAL_FUNC(gnc_tax_info_unselect_account_cb),
                        ti_dialog);

    gtk_widget_show (ti_dialog->account_tree);

    scroll = glade_xml_get_widget (xml, "account_scroll");
    gtk_container_add (GTK_CONTAINER (scroll), ti_dialog->account_tree);

    income_radio = glade_xml_get_widget (xml, "income_radio");
    gtk_signal_connect (GTK_OBJECT (income_radio), "toggled",
                        GTK_SIGNAL_FUNC (gnc_tax_info_income_cb),
                        ti_dialog);
  }

  /* select subaccounts button */
  {
    GtkWidget *button;

    button = glade_xml_get_widget (xml, "select_subaccounts_button");
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
                        GTK_SIGNAL_FUNC (select_subaccounts_clicked),
                        ti_dialog);
  }

  gnc_tax_info_update_accounts (ti_dialog);
  clear_gui (ti_dialog);
  gnc_tax_info_set_changed (ti_dialog, FALSE);

  if (last_width == 0)
    gnc_get_window_size ("tax_info_win", &last_width, &last_height);

  if (last_height == 0)
  {
    last_height = 400;
    last_width = 500;
  }

  gtk_window_set_default_size (GTK_WINDOW(ti_dialog->dialog),
                               last_width, last_height);
}

static void
close_handler (gpointer user_data)
{
  TaxInfoDialog *ti_dialog = user_data;

  gdk_window_get_geometry (GTK_WIDGET(ti_dialog->dialog)->window,
                           NULL, NULL, &last_width, &last_height, NULL);

  gnc_save_window_size ("tax_info_win", last_width, last_height);

  gnome_dialog_close (GNOME_DIALOG (ti_dialog->dialog));
}

static void
refresh_handler (GHashTable *changes, gpointer user_data)
{
  TaxInfoDialog *ti_dialog = user_data;

  gnc_account_tree_refresh (GNC_ACCOUNT_TREE (ti_dialog->account_tree));

  gnc_tax_info_update_accounts (ti_dialog);
}

/********************************************************************\
 * gnc_tax_info_dialog                                              *
 *   opens up a window to set account tax information               *
 *                                                                  * 
 * Args:   parent  - the parent of the window to be created         *
 * Return: nothing                                                  *
\********************************************************************/
void
gnc_tax_info_dialog (GtkWidget * parent)
{
  TaxInfoDialog *ti_dialog;
  gint component_id;

  ti_dialog = g_new0 (TaxInfoDialog, 1);

  gnc_tax_info_dialog_create (parent, ti_dialog);

  component_id = gnc_register_gui_component (DIALOG_TAX_INFO_CM_CLASS,
                                             refresh_handler, close_handler,
                                             ti_dialog);

  gnc_gui_component_watch_entity_type (component_id,
                                       GNC_ID_ACCOUNT,
                                       GNC_EVENT_MODIFY | GNC_EVENT_DESTROY);

  gtk_widget_grab_focus (ti_dialog->account_tree);

  gtk_widget_show (ti_dialog->dialog);
}
