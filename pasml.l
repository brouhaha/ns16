/*
Copyright 1995, 2003, 2004, 2006, 2007, 2008, 2009 Eric Smith <eric@brouhaha.com>
All rights reserved.
$Id: pasml.l,v 1.1 2009/05/10 00:23:51 eric Exp eric $
*/

%option prefix="pasm_"
%option noyywrap

%{
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "asm_types.h"
#include "pasmy.h"
#include "util.h"
#include "symtab.h"
#include "asm.h"

#undef YY_INPUT
#define YY_INPUT(buf,result,max_size) \
  { \
    int r = strlen (lineptr); \
    if (r > max_size) \
      r = max_size; \
    memcpy (buf, lineptr, r); \
    lineptr += r; \
    result = r; \
  }

%}

hexdigit	[0-9a-fA-F]
digit		[0-9]

lfirst		[a-zA-Z]
lmiddle		[a-zA-Z0-9]

%%

0{hexdigit}+	{ int tmp;
                  sscanf (yytext, "%x", & tmp);
                  pasm_lval.integer = tmp;
                  return INTEGER;
                }

{digit}+	{ pasm_lval.integer = atoi (yytext); return INTEGER; }

''''		{
		  pasm_lval.string = "'";
		  return STRING;
/* hack for a string containing a single tick - this really needs to be fixed
   to handle the general case of ticks in strings */
		}

'[^'\n]+'	{
		  pasm_lval.string = newstr (yytext + 1);
		  pasm_lval.string [strlen (pasm_lval.string) - 1] = '\0';
		  return STRING;
		}

"."asect   { return DOT_ASECT; }
"."ascii   { return DOT_ASCII; }
"."asm	   { return DOT_ASM; }
"."bsect   { return DOT_BSECT; }
"."end	   { return DOT_END; }
"."form	   { return DOT_FORM; }
"."list	   { return DOT_LIST; }
"."page	   { return DOT_PAGE; }
"."title   { return DOT_TITLE; }
"."word	   { return DOT_WORD; }

add        { return ADD; }
aisz       { return AISZ; }
and        { return AND; }
boc        { return BOC; }
cai        { return CAI; }
cfr        { return CFR; }
crf        { return CRF; }
deca       { return DECA; }
dsz        { return DSZ; }
halt       { return HALT; }
isz        { return ISZ; }
ld         { return LD; }
li         { return LI; }
jmp        { return JMP; }
jsr        { return JSR; }
lsex       { return LSEX; }
or         { return OR; }
pflg       { return PFLG; }
pull       { return PULL; }
pullf      { return PULLF; }
push       { return PUSH; }
pushf      { return PUSHF; }
radc       { return RADC; }
radd       { return RADD; }
rand       { return RAND; }
rcpy       { return RCPY; }
rol        { return ROL; }
ror        { return ROR; }
rtl        { return RTI; }
rts        { return RTS; }
rxch       { return RXCH; }
rxor       { return RXOR; }
sflg       { return SFLG; }
shl        { return SHL; }
shr        { return SHR; }
skaz       { return SKAZ; }
skg        { return SKG; }
skne       { return SKNE; }
st         { return ST; }
subb       { return SUBB; }
xchrs      { return XCHRS; }

{lfirst}{lmiddle}*	{
                          pasm_lval.string = newstr (yytext);
                          return IDENT;
			}

\;.*		;
[ \t]+		;
\n		;

.		{ return yytext [0]; }

%%
