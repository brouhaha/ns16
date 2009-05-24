/*
Copyright 1995, 2004, 2005, 2006, 2007, 2008, 2009 Eric Smith <eric@brouhaha.com>
All rights reserved.
$Id: pasm_types.h,v 1.1 2009/05/10 00:23:51 eric Exp eric $
*/

typedef uint16_t addr_t;

typedef uint16_t uword_t;
typedef int16_t  sword_t;

typedef struct
{
  uword_t word1;
  uword_t word2;
} udword_t;
