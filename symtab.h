/*
Copyright 1995, 2004, 2008, 2009 Eric Smith <eric@brouhaha.com>
All rights reserved.
$Id: symtab.h 1094 2008-03-31 01:51:40Z eric $
*/

typedef struct symtab_t symtab_t;

/* create a symbol table, returns handle to be passed in to all other calls */
symtab_t *alloc_symbol_table (void);

/* free a symbol table */
void free_symbol_table (symtab_t *table);

/* returns true for success, false if duplicate name */
bool create_symbol (symtab_t *table, char *name, uword_t value, int lineno);

/* returns true for success, false if not found */
bool lookup_symbol (symtab_t *table, char *name, uword_t *value, int lineno);

void print_symbol_table (symtab_t *table, FILE *f);
