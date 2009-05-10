/*
Copyright 1995, 2004, 2008, 2009 Eric Smith <eric@brouhaha.com>
All rights reserved.
$Id: symtab.c 1094 2008-03-31 01:51:40Z eric $
*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pasm_types.h"
#include "symtab.h"
#include "util.h"


typedef struct ref_t
{
  int lineno;
  struct ref_t *left;
  struct ref_t *right;
} ref_t;

typedef struct sym_t
{
  char *name;
  uword_t value;
  int lineno;
  ref_t *ref_root;
  struct sym_t *left;
  struct sym_t *right;
} sym_t;


struct symtab_t
{
  sym_t *root;
};


symtab_t *alloc_symbol_table (void)
{
  symtab_t *table;
  table = calloc (1, sizeof (symtab_t));
  return (table);
}

static void free_entry (sym_t *p)
{
  if (p->left)
    free_entry (p->left);
  if (p->right)
    free_entry (p->right);
  free (p);
}

void free_symbol_table (symtab_t *t)
{
  free_entry (t->root);
  free (t);
}

static bool insert_symbol (sym_t **p, sym_t *newsym)
{
  int i;

  if (! *p)
    {
      (*p) = newsym;
      return true;
    }

  i = strcasecmp (newsym->name, (*p)->name);

  if (i == 0)
    return false;
  else if (i < 0)
    return insert_symbol (& ((*p)->left), newsym);
  else
    return insert_symbol (& ((*p)->right), newsym);
}


/* returns true for success, false if duplicate name */
bool create_symbol (symtab_t *table, char *name, uword_t value, int lineno)
{
  sym_t *newsym;

  newsym = calloc (1, sizeof (sym_t));
  if (! newsym)
    {
      fprintf (stderr, "memory allocation failure\n");
      exit (2);
    }

  newsym->name = newstr (name);
  newsym->value = value;
  newsym->lineno = lineno;

  return (insert_symbol (& table->root, newsym));
}


// Using an unbalanced binary tree for the references is not very
// efficient, because the entries are inserted mostly in order.
static bool insert_reference (ref_t **r, ref_t *newref)
{
  if (! *r)
    {
      (*r) = newref;
      return true;
    }

  if (newref->lineno == (*r)->lineno)
    return false;
  else if (newref->lineno < (*r)->lineno)
    return insert_reference (& ((*r)->left), newref);
  else
    return insert_reference (& ((*r)->right), newref);
}

static void add_reference (sym_t *p, int lineno)
{
  ref_t *ref;

  ref = calloc (1, sizeof (ref_t));
  ref->lineno = lineno;
  insert_reference (& p->ref_root, ref);
}


/* returns true for success, false if not found */
bool lookup_symbol (symtab_t *table, char *name, uword_t *value, int lineno)
{
  sym_t *p = table->root;
  int i;

  while (p)
    {
      i = strcasecmp (name, p->name);
      if (i == 0)
	{
	  *value = p->value;
	  if (lineno)
	    add_reference (p, lineno);
	  return true;
	}
      if (i < 0)
	p = p->left;
      else
	p = p->right;
    }
  return false;
}

static void default_value_fmt_fn (uword_t value, char *buf, int buf_len)
{
  snprintf (buf, buf_len, "%04x", value);
}

static void print_references (FILE *f,
			      sym_t *p,
			      ref_t *r)
{
  if (! r)
    return;

  print_references (f, p, r->left);

  if (r->lineno != p->lineno)
    fprintf (f, " %d", r->lineno);

  print_references (f, p, r->right);
}


static void print_symbols (FILE *f,
			   sym_t *p)
{
  char buf [80];

  if (! p)
    return;

  print_symbols (f, p->left);

  default_value_fmt_fn (p->value, buf, sizeof (buf));
  fprintf (f, "%s %s %d  ", buf, p->name, p->lineno);
  print_references (f, p, p->ref_root);
  fprintf (f, "\n");

  print_symbols (f, p->right);
}

void print_symbol_table (symtab_t *table,
			 FILE *f)
{
  print_symbols (f, table->root);
}
