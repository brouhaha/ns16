/*
Copyright 1995, 2004, 2005, 2006, 2007, 2008, 2009 Eric Smith <eric@brouhaha.com>
All rights reserved.
$Id: pasmy.y,v 1.1 2009/05/10 00:23:51 eric Exp eric $
*/

%name-prefix="pasm_"

%{
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "asm_types.h"
#include "symtab.h"
#include "asm.h"

void pasm_error (char *s);
%}

%union {
    uword_t integer;
    char *string;
  }

%token <integer> INTEGER
%token <string> IDENT
%token <string> STRING

%token DOT_ASECT
%token DOT_ASCII
%token DOT_ASM
%token DOT_BSECT
%token DOT_END
%token DOT_FORM
%token DOT_LIST
%token DOT_PAGE
%token DOT_TITLE
%token DOT_WORD

%token ADD
%token AISZ
%token AND
%token BOC
%token CAI
%token CFR
%token CRF
%token DECA
%token DSZ
%token HALT
%token ISZ
%token LD
%token LI
%token JMP
%token JSR
%token LSEX
%token OR
%token PFLG
%token PULL
%token PULLF
%token PUSH
%token PUSHF
%token RADC
%token RADD
%token RAND
%token RCPY
%token ROL
%token ROR
%token RTI
%token RTS
%token RXCH
%token RXOR
%token SFLG
%token SHL
%token SHR
%token SKAZ
%token SKG
%token SKNE
%token ST
%token SUBB
%token XCHRS

%type <string> ident

%type <integer> expr

%left '+' '-' '*' '/' '&' '!' '<' '>' NEG

%type <integer> ac
%type <integer> ac0
%type <integer> ac23
%type <integer> bit
%type <integer> nib
%type <integer> count
%type <integer> sb
%type <integer> ub
%type <integer> rela
%type <integer> ea
%type <integer> ea_lit

%type <integer> field_list
%type <integer> field

%%

line		:	pseudo_op
		|	pseudo_op_label
		|	label pseudo_op_label
		|	label
		|	label instruction
		|	instruction
		|
		|	error
		;

/*
 * Unfortunately the National Semiconductor assembler does allow the use
 * of mnemonics as labels.  We special-case the four that are used in
 * FIG-Forth.
 */
ident		: IDENT { $$ = $1; }
		| AND { $$ = "and"; }
		| OR { $$ = "or"; }
		| PUSH { $$ = "push"; }
		| PULL { $$ = "pull"; }
		;

label		: ident ':'	{ do_label ($1); }
		;

/*
   Had to omit parenthesized subexpressions:
		| '(' expr ')'      { $$ = $2; }
   from factor due to conflict with indexed addressing

   Note also that the National Semiconductor assembler does NOT have
   operator precedence.  It evaluates strictly left-to-right.
*/

expr		: '.' { $$ = pc; }
		| INTEGER { $$ = $1; }
		| STRING { $$ = ($1 [0] << 8) + ($1 [1] ? $1 [1] : 0x20); }
		| ident { if (! lookup_symbol (symtab, $1, &$$, get_lineno ()))
			    {
			      if (pass == 2)
				error ("undefined symbol '%s'\n", $1);
			      $$ = 0;
			    }
			}
		| expr '+' expr { $$ = $1 + $3; }
		| expr '-' expr { $$ = $1 - $3; }
		| expr '*' expr { $$ = $1 * $3; }
		| expr '/' expr { $$ = $1 / $3; }
		| expr '&' expr { $$ = $1 & $3; }
		| expr '!' expr { $$ = $1 | $3; }
		| expr '<' expr { $$ = $1 < $3; }
		| expr '>' expr { $$ = $1 > $3; }
		| '-' expr %prec NEG { $$ = - $2; }
		| '%' expr %prec NEG { $$ = ~ $2; }
		;

pseudo_op	: ps_ascii
		| ps_asect
		| ps_asm
		| ps_bsect
		| ps_equate
		| ps_form
		| ps_list
		| ps_org
		| ps_page
		| ps_title
		;

pseudo_op_label	: ps_end
		| ps_word
		;

ps_ascii	: DOT_ASCII STRING  { emit_string ($2); } ;

ps_asect	: DOT_ASECT { set_section (ASECT); show_value (pc); } ;

ps_asm		: DOT_ASM ident ;

ps_bsect	: DOT_BSECT { set_section (BSECT); show_value (pc); } ;

ps_end		: DOT_END expr ;

ps_form		: DOT_FORM ident { create_form ($2); } ',' form_decl_list ;

form_decl_list	: form_decl_field
		| form_decl_list ',' form_decl_field
		;

form_decl_field	: expr            { add_form_field ($1, false, 0); }
		| expr '(' bit')' { add_form_field ($1, true, $3); }
		;

ps_list		: DOT_LIST ident '=' expr ;

ps_page		: DOT_PAGE
		| DOT_PAGE STRING ;

ps_title	: DOT_TITLE ident ',' STRING ;

ps_equate	: ident '=' expr { define_symbol ($1, $3); show_value ($3); } ;

ps_org		: '.' '=' expr { pc = $3; show_value ($3); } ;

ps_word		: DOT_WORD word_lit_list ;

word_lit_list	: word_lit
		| word_lit_list ',' word_lit
		;

word_lit	: expr { emit ($1); } ;

instruction	: form_subst
		| add_inst
	        | aisz_inst
		| and_inst
		| boc_inst
	        | cai_inst
		| cfr_inst
	        | crf_inst
		| deca_inst
		| dsz_inst
		| halt_inst
		| isz_inst
		| ld_inst
		| ld_ind_inst
		| li_inst
		| jmp_inst
		| jmp_ind_inst
		| jsr_inst
		| jsr_ind_inst
		| lsex_inst
		| or_inst
		| pflg_inst
		| pull_inst
		| pullf_inst
		| push_inst
		| pushf_inst
		| radc_inst
		| radd_inst
		| rand_inst
		| rcpy_inst
		| rol_inst
		| ror_inst
		| rti_inst
		| rts_inst
		| rxch_inst
		| rxor_inst
		| sflg_inst
		| shl_inst
		| shr_inst
		| skaz_inst
		| skg_inst
		| skne_inst
		| st_inst
		| st_ind_inst
		| subb_inst
		| xchrs_inst
	        ;

form_subst	: ident { current_form = find_form ($1);
		          if (current_form)
			    current_form->current_field = current_form->first_field;
                          else
		            error ("undefined form '%s'\n", $1);
                        }
		  field_list { if (current_form)
                                 emit (current_form->constant_value + $3);
                               else
                                 emit (0);
                             } ;

field_list	: field                { $$ = $1; }
		| field_list ',' field { $$ = $1 + $3; }
		;

field		: expr { $$ = field_value ($1); } ;

halt_inst	: HALT              { emit (0x0000); } ;
cfr_inst	: CFR ac            { emit (0x0400 + ($2 << 8)); } ;
crf_inst	: CRF ac            { emit (0x0800 + ($2 << 8)); } ;
pushf_inst	: PUSHF             { emit (0x0c00); } ;
pullf_inst	: PULLF             { emit (0x1000); } ;
jsr_inst	: JSR ea_lit        { emit (0x1400 + $2); } ;
jmp_inst	: JMP ea_lit        { emit (0x1800 + $2); } ;
xchrs_inst	: XCHRS ac          { emit (0x1c00 + ($2 << 8)); } ;
rol_inst	: ROL ac ',' count ',' bit { emit (0x2000 + ($2 << 8) + ($4 << 1) + $6); } ;
ror_inst	: ROR ac ',' count ',' bit { emit (0x2400 + ($2 << 8) + ($4 << 1) + $6); } ;
shl_inst	: SHL ac ',' count ',' bit { emit (0x2800 + ($2 << 8) + ($4 << 1) + $6); } ;
shr_inst	: SHR ac ',' count ',' bit { emit (0x2c00 + ($2 << 8) + ($4 << 1) + $6); } ;
pflg_inst	: PFLG nib          { emit (0x3000 + ($2 << 8)); } ;
sflg_inst	: SFLG nib          { emit (0x3080 + ($2 << 8)); } ;
boc_inst	: BOC nib ',' rela  { emit (0x4000 + ($2 << 8) + $4); } ;
li_inst		: LI   ac ',' sb    { emit (0x5000 + ($2 << 8) + $4); } ;
rand_inst	: RAND ac ',' ac    { emit (0x5400 + ($4 << 8) + ($2 << 6)); } ;
rxor_inst	: RXOR ac ',' ac    { emit (0x5800 + ($4 << 8) + ($2 << 6)); } ;
rcpy_inst	: RCPY ac ',' ac    { emit (0x5c00 + ($4 << 8) + ($2 << 6)); } ;
push_inst	: PUSH ac           { emit (0x6000 + ($2 << 8)); } ;
pull_inst	: PULL ac           { emit (0x6400 + ($2 << 8)); } ;
radd_inst	: RADD ac ',' ac    { emit (0x6800 + ($4 << 8) + ($2 << 6)); } ;
rxch_inst	: RXCH ac ',' ac    { emit (0x6c00 + ($4 << 8) + ($2 << 6)); } ;
cai_inst	: CAI  ac ',' sb    { emit (0x7000 + ($2 << 8) + $4); } ;
radc_inst	: RADC ac ',' ac    { emit (0x7400 + ($4 << 8) + ($2 << 6)); } ;
aisz_inst	: AISZ ac ',' sb    { emit (0x7800 + ($2 << 8) + $4); } ;
rti_inst	: RTI ub            { emit (0x7c00 + $2); } ;
rts_inst	: RTS ub            { emit (0x8000 + $2); } ;
deca_inst	: DECA ac0 ',' ea   { emit (0x8800 + $4); } ;
isz_inst	: ISZ ea            { emit (0x8c00 + $2); } ;
subb_inst	: SUBB ac0 ',' ea   { emit (0x9000 + $4); } ;
jsr_ind_inst	: JSR '@' ea        { emit (0x9400 + $3); } ;
jmp_ind_inst	: JMP '@' ea        { emit (0x9800 + $3); } ;
skg_inst	: SKG ac0 ',' ea    { emit (0x9c00 + $4); } ;
ld_ind_inst	: LD ac ',' '@' ea  { emit (0xa000 + u_range ($2, 0, 0) + $5); } ;
or_inst		: OR ac0 ',' ea     { emit (0xa400 + $4); } ;
and_inst	: AND ac0 ',' ea    { emit (0xa800 + $4); } ;
dsz_inst	: DSZ ea            { emit (0xac00 + $2); } ;
st_ind_inst	: ST ac ',' '@' ea  { emit (0xb000 + u_range ($2, 0, 0) + $5); } ;
skaz_inst	: SKAZ ac0 ',' ea   { emit (0xb800 + $4); } ;
lsex_inst	: LSEX ac0 ',' ea   { emit (0xbc00 + $4); } ;
ld_inst		: LD ac ',' ea      { emit (0xc000 + ($2 << 10) + $4); } ;
st_inst		: ST ac ',' ea      { emit (0xd000 + ($2 << 10) + $4); } ;
add_inst	: ADD ac ',' ea     { emit (0xe000 + ($2 << 10) + $4); } ;
skne_inst	: SKNE ac ',' ea    { emit (0xf000 + ($2 << 10) + $4); } ;

ac		: expr { $$ = u_range ($1, 0, 3); } ;
ac0		: expr { $$ = u_range ($1, 0, 0); } ;
ac23		: expr { $$ = u_range ($1, 2, 3); } ;
bit		: expr { $$ = u_range ($1, 0, 1); } ;
nib		: expr { $$ = u_range ($1, 0, 15); } ;
count		: expr { $$ = u_range ($1, 0, 127); } ;
ub		: expr { $$ = u_range ($1, 0, 255); } ;
sb		: expr { $$ = (uword_t) (s_range ((sword_t) $1, -128, 127) & 0xff); } ;

rela		: expr { $$ = ($1 - (pc + 1)) & 0xff; } ;

ea		: expr { if ($1 < 256)
		           $$ = $1;
			 else
			   $$ = (1 << 8) + (s_range ($1 - (pc + 1), -128, 127) & 0xff); }
		| '(' ac23 ')' { $$ = $2 << 8; }
		| sb '(' ac23 ')' { $$ = ($3 << 8) + $1; }
		;

/*
 * Effective address for JMP and JSR direct may be out of range.  Instead of
 * flagging it as an error, convert to indirect (add 0x8000 to opcode) and use
 * literal pool in BSECT.
 */
ea_lit		: expr { if ($1 < 256)
		           $$ = $1;
			 else if (in_s_range($1 - (pc + 1), -128, 127))
			   $$ = (1 << 8) + (($1 - (pc + 1)) & 0xff);
			 else
			   $$ = 0x8000 + literal_pool_addr ($1);
		       }
		| '(' ac23 ')' { $$ = $2 << 8; }
		| sb '(' ac23 ')' { $$ = ($3 << 8) + $1; }
		;

%%

void pasm_error (char *s)
{
  error ("%s\n", s);
}

void parse (void)
{
  pasm_parse ();
}
