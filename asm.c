/*
Copyright 1995, 2004, 2005, 2006, 2007, 2008, 2009 Eric Smith <eric@brouhaha.com>
All rights reserved.
$Id$
*/

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pasm_types.h"
#include "symtab.h"
#include "util.h"
#include "pasm.h"


void usage (FILE *f)
{
  fprintf (f, "pasm assembler - %s\n", program_release);
  fprintf (f, "Copyright 1995, 2003-2009 Eric Smith <eric@brouhaha.com>\n");
  fprintf (f, "\n");
  fprintf (f, "usage: %s [options...] sourcefile\n", progname);
  fprintf (f, "options:\n");
  fprintf (f, "   -o objfile\n");
  fprintf (f, "   -l listfile\n");
}


parser_t *parser [1] =
  {
    pasm_parse
  };


int pass;
int errors;
int warnings;

bool parse_error;


section_t current_section;
addr_t section_pc [MAX_SECTION];
addr_t pc;		/* current pc */

typedef struct
{
  uword_t addr;
  uword_t value;
} literal_t;
#define MAX_LITERAL 256
literal_t literal_pool [MAX_LITERAL];
int literal_pool_size;


#define MAX_OBJECT 256
int object_code_words;
uword_t object_code [MAX_OBJECT];

bool show_value_flag;


char linebuf [MAX_LINE];
char *lineptr;

#define MAX_ERRBUF 2048
char errbuf [MAX_ERRBUF];
char *errptr;

#define SRC_TAB 32


#define MAX_INCLUDE_NEST 128

static int include_nest;

static char *src_fn   [MAX_INCLUDE_NEST];
static int lineno     [MAX_INCLUDE_NEST];
static FILE *src_file [MAX_INCLUDE_NEST];
static FILE *obj_file  = NULL;
static FILE *list_file = NULL;


symtab_t *symtab;


static void open_source (char *fn)
{
  if (++include_nest == MAX_INCLUDE_NEST)
    fatal (2, "include files nested too deep\n");
  src_fn [include_nest] = strdup (fn);
  lineno [include_nest] = 0;
  src_file [include_nest] = fopen (fn, "r");
  if (! src_file [include_nest])
    fatal (2, "can't open input file '%s'\n", fn);
}

static bool close_source (void)
{
  if (include_nest < 0)
    fatal (2, "mismatched source file close\n");
  fclose (src_file [include_nest]);
  free (src_fn [include_nest]);
  include_nest--;
  return include_nest >= 0;
}

void pseudo_include (char *s)
{
  char *p;
  char *s2;

  // prepend path prefix of current file
  p = path_prefix (src_fn [include_nest]);
  s2 = path_cat_n (2, p, s);
  open_source (s2);

  free (p);
  free (s);
}


#define MAX_COND_NEST_LEVEL 63
static int cond_nest_level;
static uint64_t cond_state;
static uint64_t cond_else;

static void cond_init (void)
{
  cond_nest_level = 0;
  cond_state = 1;
  cond_else = 0;
}

bool get_cond_state (void)
{
  return cond_state & 1;
}

int  get_lineno (void)
{
  return lineno [include_nest];
}

void pseudo_if (int val)
{
  if (cond_nest_level >= MAX_COND_NEST_LEVEL)
    {
      error ("conditionals nested too deep");
      return;
    }
  cond_nest_level++;
  cond_state <<= 1;
  cond_else <<= 1;
  cond_state |= (val != 0);
}

void pseudo_ifdef (char *s)
{
  uword_t val;

  if (cond_nest_level >= MAX_COND_NEST_LEVEL)
    {
      error ("conditionals nested too deep");
      return;
    }
  cond_nest_level++;
  cond_state <<= 1;
  cond_else <<= 1;
  cond_state |= lookup_symbol (symtab, s, & val, lineno [include_nest]);
}

void pseudo_else (void)
{
  if (! cond_nest_level)
    {
      error ("else without conditional");
      return;
    }
  if (cond_else & 1)
    {
      error ("second else for same conditional");
      return;
    }
  cond_else |= 1;
  cond_state ^= 1;
}

void pseudo_endif (void)
{
  if (! cond_nest_level)
    {
      error ("endif without conditional");
      return;
    }
  cond_nest_level--;
  cond_state >>= 1;
  cond_else >>= 1;
}



form_t *forms;
form_t *current_form;

void create_form (char *name)
{
  form_t *form;
  if (pass == 2)
    return;
  form = (form_t *) alloc (sizeof (form_t));
  form->name = newstr (name);
  form->leftmost_bit_avail = 15;
  form->next = forms;
  forms = form;
}

void add_form_field (int width, bool is_constant, int value)
{
  field_t *field;
  int rightmost_bit;

  if (pass == 2)
    return;
  rightmost_bit = forms->leftmost_bit_avail + 1 - width;
  forms->leftmost_bit_avail -= width;
  if (is_constant)
    {
      // don't link constant field into chain
      forms->constant_value |= (value << rightmost_bit);
      return;
    }
  field = (field_t *) alloc (sizeof (field_t));
  field->width = width;
  field->rightmost_bit = rightmost_bit;
  if (forms->last_field)
    {
      // not the first field, add to end of list
      forms->last_field->next = field;
      forms->last_field = field;
    }
  else
    {
      // first field, initialize list
      forms->first_field = field;
      forms->last_field = field;
    }
}

form_t *find_form (char *name)
{
  form_t *form;

  for (form = forms; form; form = form->next)
    if (strcmp (form->name, name) == 0)
      return form;

  return NULL;
}

uword_t field_value (uword_t v)
{
  if (current_form)
    {
      if (current_form->current_field)
	{
	  int rightmost_bit = current_form->current_field->rightmost_bit;
	  current_form->current_field = current_form->current_field->next;
	  return v << rightmost_bit;
	}
      error ("too many fields for form\n");
      return 0;
    }
  return 0;
}

#if 0
void print_form_definitions (void)
{
  form_t *form;
  field_t *field;

  for (form = forms; form; form = form->next)
    {
      printf ("form '%s' constant value %04x\n", form->name, form->constant_value);
      for (field = form->first_field; field; field = field->next)
	printf ("  field width %d rightmost bit %d\n", field->width, field->rightmost_bit);
    }
}
#endif


void output_listing_line (char *listbuf)
{
  if (list_file)
    fprintf (list_file, "%s\n", listbuf);
}


void output_error_line (char *listbuf)
{
  if (errptr != & errbuf [0])
    {
      fprintf (stderr, "%s\n", listbuf);
      if (list_file)
	fprintf (list_file, "%s", errbuf);
      fprintf (stderr, "%s",   errbuf);
    }
}


void format_listing (bool show_source)
{
  int i;
  char *listptr;
  char listbuf [MAX_LINE];

  listbuf [0] = '\0';
  listptr = & listbuf [0];
  if (show_source)
    listptr += sprintf (listptr, "%5d ", lineno [include_nest]);
  else
    listptr += sprintf (listptr, "      ");

  if (show_value_flag)
    listptr += sprintf (listptr, "     %04X    ", object_code [0]);
  else if (object_code_words == 0)
    listptr += sprintf (listptr, "             ");
  else
    listptr += sprintf (listptr, "%04X %04X A  ", pc, object_code [0]);
  if (show_source)
    strcat (listptr, linebuf);
  listptr += strlen (listptr);
  output_listing_line (listbuf);
  output_error_line (listbuf);

  for (i = 1; i < object_code_words; i++)
    {
      listbuf [0] = '\0';
      listptr = & listbuf [0];

      listptr += sprintf (listptr, "      ");  // no line number
      listptr += sprintf (listptr, "%04X %04X A  ", pc + i, object_code [i]);
      output_listing_line (listbuf);
    }
}


uword_t literal_pool_addr (uword_t value)
{
  int i;
  section_t prev_section = current_section;
  uword_t addr;

  for (i = 0; i < literal_pool_size; i++)
    {
      if (literal_pool [i].value == value)
	{
	  //printf ("Pass %d line %d looking for literal value %04x, found at %04x\n", pass, lineno [include_nest], value, literal_pool [i].addr);
	  return literal_pool [i].addr;
	}
    }

  if (pass == 2)
    {
      error ("literal entry not allocated during first pass\n");
      return 0;
    }

  prev_section = current_section;
  set_section (BSECT);
  addr = pc++;
  literal_pool [literal_pool_size].value = value;
  literal_pool [literal_pool_size].addr = addr;
  //printf ("Pass %d line %d allocated literal value %04x at addr %04x\n", pass, lineno [include_nest], value, addr);
  literal_pool_size++;
  set_section (prev_section);
  return addr;
}


void write_literal_pool (void)
{
  int i;

  set_section (BSECT);
  for (i = 0; i < literal_pool_size; i++)
    {
      object_code_words = 0;
      show_value_flag = false;

      pc = literal_pool [i].addr;
      emit (literal_pool [i].value);
      format_listing (false);
    }
}

static void parse_source_line (void)
{
#if 0
  lineptr = & linebuf [0];
  parse_error = false;

  asm_cond_parse ();
  if (! parse_error)
    return;  // successfully parsed a conditional assembly directive

  if (! (cond_state & 1))
    return;  // conditional false, don't try to parse
#endif

  lineptr = & linebuf [0];
  parse_error = false;
  pasm_parse ();
  if (! parse_error)
    return;
}


static void process_line (char *inbuf)
{
  // remove trailing whitespace including newline if present
  trim_trailing_whitespace (inbuf);

  expand_tabs (linebuf,
	       sizeof (linebuf),
	       inbuf,
	       8);

  lineno [include_nest]++;

  errptr = & errbuf [0];
  errbuf [0] = '\0';

  object_code_words = 0;
  show_value_flag = false;

  parse_source_line ();

  if (pass == 2)
    format_listing (true);

  pc += object_code_words;
}


static void do_pass (int p)
{
  char inbuf [MAX_LINE];

  pass = p;
  errors = 0;
  warnings = 0;

  cond_init ();

  current_section = ASECT;
  memset (section_pc, 0, sizeof (section_pc));
  pc = 0;

  printf ("starting pass %d\n", pass);

  while (1)
    {
      if (! fgets (inbuf, MAX_LINE, src_file [include_nest]))
	{
	  if (ferror (src_file [include_nest]))
	    fatal (2, "error reading source file\n");
	  if (close_source ())
	    continue;
	  else
	    break;
	}

      process_line (inbuf);
    }

  if (cond_nest_level != 0)
    error ("unterminated conditional(s)\n");

  printf ("finished pass %d\n", pass);
}


static void parse_define_option (char *opt)
{
  char *name;
  long value;

  name = opt + 2;  // skip the "-D"
  if (! *name)
    goto syntax_err;  // must be a name

  char *equals = strchr (name, '=');
  if (equals)
    {
      char *val_end_ptr;
      *equals = '\0';
      if (! * (equals + 1))  // must be a value after the equals
	goto syntax_err;
      value = strtol (equals + 1, & val_end_ptr, 0);
      if (* val_end_ptr)
	goto syntax_err;  // extra characters that can't be parsed as a value
    }
  else
    value = 1;

  fprintf (stderr, "defining '%s' to have value %ld\n", name, value);

  if (! create_symbol (symtab, name, value, 0))
    fatal (1, "duplicate symbol in '-D' option: '%s'\n", name);

  return;

 syntax_err:
  fatal (1, "malformed '-D' option\n");
  return;
}


int main (int argc, char *argv[])
{
  char *src_fn = NULL;
  char *obj_fn = NULL;
  char *list_fn = NULL;

  progname = argv [0];

  symtab = alloc_symbol_table ();
  if (! symtab)
    fatal (2, "symbol table allocation failed\n");

  while (--argc)
    {
      argv++;
      if (*argv [0] == '-')
	{
	  if (strcmp (argv [0], "-o") == 0)
	    {
	      if (argc < 2)
		fatal (1, "'-o' must be followed by object filename\n");
	      obj_fn = argv [1];
	      argc--;
	      argv++;
	    }
	  else if (strcmp (argv [0], "-l") == 0)
	    {
	      if (argc < 2)
		fatal (1, "'-l' must be followed by listing filename\n");
	      list_fn = argv [1];
	      argc--;
	      argv++;
	    }
	  else if (strncmp (argv [0], "-D", 2) == 0)
	    {
	      parse_define_option (argv [0]);
	    }
	  else
	    fatal (1, "unrecognized option '%s'\n", argv [0]);
	}
      else if (src_fn)
	fatal (1, "only one source file may be specified\n");
      else
	src_fn = argv [0];
    }

  if (! src_fn)
    fatal (1, "source file must be specified\n");

  if (obj_fn)
    {
      obj_file = fopen (obj_fn, "w");
      if (! obj_file)
	fatal (2, "can't open input file '%s'\n", obj_fn);
    }

  if (list_fn)
    {
      list_file = fopen (list_fn, "w");
      if (! list_file)
	fatal (2, "can't open listing file '%s'\n", list_fn);
    }

  include_nest = -1;
  open_source (src_fn);
  do_pass (1);

  open_source (src_fn);
  do_pass (2);

  write_literal_pool ();

  if (list_file)
    {
      fprintf (list_file, "\nsymbols:\n\n");
      print_symbol_table (symtab,
			  list_file);
      fprintf (list_file, "\n");
    }

  err_printf ("%d errors detected, %d warnings\n", errors, warnings);

  if (obj_file)
    fclose (obj_file);
  if (list_file)
    fclose (list_file);

  //print_form_definitions ();

  exit (0);
}


void define_symbol (char *s, uword_t value)
{
  uword_t prev_val;

  if (pass == 1)
    {
      if (! create_symbol (symtab, s, value, lineno [include_nest]))
	error ("multiply defined symbol '%s'\n", s);
    }
  else if (! lookup_symbol (symtab, s, & prev_val, lineno [include_nest]))
    error ("undefined symbol '%s'\n", s);
  else if (prev_val != value)
    error ("phase error for symbol '%s'\n", s);
}


void do_label (char *s)
{
  define_symbol (s, pc);
}


static void write_obj (uword_t addr, uword_t opcode)
{
  if (obj_file)
    fprintf (obj_file, "%04x: %04x\n", addr, opcode);
}


void set_section (section_t section)
{
  if (current_section == section)
    return;

  section_pc [current_section] = pc;
  current_section = section;
  pc = section_pc [current_section];
}


void show_value (uword_t value)
{
  if (object_code_words)
    {
      // $$$ should never happen
      return;
    }
  object_code [0] = value;
  show_value_flag = true;
}


void emit (uword_t op)
{
  if (show_value_flag)
    {
      // $$$ should never happen
      show_value_flag = false;
      object_code_words = 0;
    }

  if (pass == 2)
    write_obj (pc + object_code_words, op);

  object_code [object_code_words++] = op;
}

void emit_string (char *s)
{
  while (*s)
    {
      int c1 = *s;
      int c2 = *(s+1);
      emit ((c1 << 8) | (c2 ? c2 : ' '));
      if (! c2)
	break;
      s += 2;
    }
}


uword_t u_range (uword_t val, uword_t min, uword_t max)
{
  if ((val < min) || (val > max))
    {
      error ("value out of range [%u to %u], using %u", min, max, min);
      return min;
    }
  return val;
}


sword_t s_range (sword_t val, sword_t min, sword_t max)
{
  if ((val < min) || (val > max))
    {
      error ("value out of range [%d to %d], using %d", min, max, min);
      return min;
    }
  return val;
}

bool in_s_range (sword_t val, sword_t min, sword_t max)
{
  return (val >= min) && (val <= max);
}


/*
 * print to both listing file and standard error
 *
 * Use this for general messages.  Don't use this for warnings or errors
 * generated by a particular line of the source file.  Use error() or
 * warning() for that.
 */
int err_vprintf (char *format, va_list ap)
{
  int res;

  if (list_file && (pass == 2))
    {
      va_list ap_copy;

      va_copy (ap_copy, ap);
      vfprintf (list_file, format, ap_copy);
      va_end (ap_copy);
    }
  res = vfprintf (stderr, format, ap);
  return (res);
}

int err_printf (char *format, ...)
{
  int res;
  va_list ap;

  va_start (ap, format);
  res = err_vprintf (format, ap);
  va_end (ap);
  return (res);
}


/* generate error or warning messages and increment appropriate counter */
int error   (char *format, ...)
{
  int res;
  va_list ap;

  err_printf ("error in file %s line %d: ", src_fn [include_nest], lineno [include_nest]);
  va_start (ap, format);
  res = err_vprintf (format, ap);
  va_end (ap);
  errptr += res;
  errors ++;
  return (res);
}

int asm_warning (char *format, ...)
{
  int res;
  va_list ap;

  err_printf ("warning in file %s line %d: ", src_fn [include_nest], lineno [include_nest]);
  va_start (ap, format);
  res = err_vprintf (format, ap);
  va_end (ap);
  errptr += res;
  warnings ++;
  return (res);
}
