// Copyright 2009 Eric Smith <eric@brouhaha.com>
// All rights reserved.
// $Id: isim.c,v 1.2 2010/07/06 19:38:12 eric Exp eric $

// Limitations:
//  interrupts not supported
//  I/O instructions (RIN, ROUT) not supported
//  EIS, POWR I/O, Arithmetic CROM instructions not supported

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

typedef uint16_t word_t;

char *block_fn = "figforth_blocks";
FILE *block_f;

FILE *trace_f = NULL;
bool inst_trace = false;
bool word_trace = false;

uint16_t mem [65536];

uint16_t ac [4];  // accumulators
uint16_t pc;  // program counter

#define STACK_SIZE 16
int sp;  // index of top item on stack [0..stackSize-1], or -1 when empty
uint16_t stack [STACK_SIZE];

bool halt;

bool flags [16];  // general-purpose flags
#define cy (flags [13])
#define ov (flags [14])
#define lk (flags [15])

bool ext_flag [8];  // external flag outputs
#define int_en (ext_flag [1])
#define sel (ext_flag [1])

// external inputs
bool interrupt_line;

bool cont_in;
bool jc12;
bool jc13;
bool jc14;
bool jc15;

#define BYTE_MASK 0xff
#define WORD_MASK 0xffff

void reset (void)
{
  sp = 0;  // stack empty
  pc = 0x0000;
  ac [0] = 0x0000;
  ac [1] = 0x0000;
  ac [2] = 0x0000;
  ac [3] = 0x0000;
  memset (stack, 0, sizeof (stack));

  cont_in = false;
  jc12 = false;
  jc13 = false;
  jc14 = false;
  jc15 = false;
}

int signExtend (int b)
{
  if ((b & 0x80) != 0)
    return b | 0xff00;
  else
    return b & 0x00ff;
}

int signedValue (int v)
{
  if (v < 0x7fff)
    return v;
  return v - 0x10000;
}

int add (int a, int b, bool carryIn)
{
  int sum16 = a + b + (carryIn ? 1 : 0);
  cy = (sum16 >> 16) != 0;
  
  // sign extend operands to 17 bits
  if ((a & 0x8000) != 0)
    a |= 0x10000;
  if ((b & 0x8000) != 0)
    b |= 0x10000;
  int sum17 = a + b + (carryIn ? 1 : 0);
  ov = (sum17 >> 17) != (sum17 >> 16);

  return sum16 & WORD_MASK;
}

int rotateLeft (int data, int width, int count);

int rotateRight (int data, int width, int count)
{
  if (count == 0)
    return data;
  if (count < 0)
    return rotateLeft (data, width, -count);
  while (count > width)
    count -= width;
  while (count-- != 0)
    {
      if ((data & 1) != 0)
	data |= (1 << width);
      data >>= 1;
    }
  return data;
}

int rotateLeft (int data, int width, int count)
{
  if (count == 0)
    return data;
  if (count < 0)
    return rotateRight (data, width, -count);
  while (count >= width)
    count -= width;
  while (count-- != 0)
    {
      data <<= 1;
      if ((data & (1 << width)) != 0)
	data = data + 1 - (1 << width);
    }
  return data;
}

int shiftLeft (int data, int width, int count);

int shiftRight (int data, int width, int count)
{
  if (count == 0)
    return data;
  if (count < 0)
    return shiftLeft (data, width, -count);
  // $$$
  return data;
}

int shiftLeft (int data, int width, int count)
{
  if (count == 0)
    return data;
  if (count < 0)
    return shiftRight (data, width, -count);
  // $$$
  return data;
}

void push (int value)
{
  stack [sp] = value;
  if (++sp >= STACK_SIZE)
    sp = 0;
}

int pull (void)
{
  word_t data;

  if (--sp < 0)
    sp = STACK_SIZE - 1;
  data = stack [sp];
  stack [sp] = 0;
  return data;
}

bool stack_full (void)
{
  return stack [sp] != 0;
}

int getFR (void)
{
  int i;
  word_t data;

  data = 0;
  for (i = 15; i >= 0; i--)
    data = (data << 1) | flags [i];

  return data;
}

void setFR (int value)
{
  int i;

  for (i = 0; i <= 15; i++)
    {
      flags [i] = value & 1;
      value >>= 1;
    }
}

void setFlag (int flag)
{
  ext_flag [flag] = true;
}

void pulseFlag (int flag)
{
  ext_flag [flag] = false;
}

#define STACK_LIMIT  0x1d8f

void printStack (void)
{
  int a;

  if ((ac [3] < (STACK_LIMIT - 100)) || (ac [3] > STACK_LIMIT))
    return;
  fprintf (trace_f, "stack: ");
  if (ac [3] >= STACK_LIMIT)
    {
      fprintf (trace_f, "empty ");
      return;
    }
  for (a = STACK_LIMIT - 1; a >= ac [3]; a--)
    {
      fprintf (trace_f, "%04x ", mem [a]);
    }
}

void printWordName (int addr)
{
  int c;
  int b = 1;
  addr -= 2;  // back up past PFA to last word of name
  if ((mem [addr] & 0x8080) == 0x8080)
    {
      // short word, we're done
    }
  else
    {
      // long word, find start of name
      do
	{
	  addr --;
	}
      while ((mem [addr] & 0x8000) == 0);
    }
  for (;;)
    {
      c = mem [addr];
      if (b == 0)
	c >>= 8;
      fprintf (trace_f, "%c", c & 0x7f);
      if ((c & 0x80) != 0)
	break;
      b++;
      if (b == 2)
	{
	  addr++;
	  b = 0;
	}
    }
}

const char *boc_cond_name [16] =
  {
    [0x0] = "int",
    [0x1] = "zero",
    [0x2] = "positive",
    [0x3] = "bit0",
    [0x4] = "bit1",
    [0x5] = "nonzero",
    [0x6] = "cpi",
    [0x7] = "cps",
    [0x8] = "stack_full",
    [0x9] = "ien",
    [0xa] = "cy/ov",
    [0xb] = "negative",
    [0xc] = "jc12",
    [0xd] = "jc13",
    [0xe] = "jc14",
    [0xf] = "jc15"
  };

const char *ext_flag_name [8] =
  {
    [0x0] = "f8",
    [0x1] = "int_en",
    [0x2] = "sel",
    [0x3] = "f11",
    [0x4] = "f12",
    [0x5] = "f13",
    [0x6] = "f14",
    [0x7] = "f15"
  };

void disassembleInstruction (int addr, int instruction, char *buf)
{
  int inst1110 = (instruction >> 10) & 0x3;
  int inst98 = (instruction >> 8) & 0x3;
  int instLowByte = instruction & 0xff;
  int ea = 0;
  int target = (addr + 1 + signExtend (instruction & BYTE_MASK)) & WORD_MASK;

  switch (inst98)
    {
    case 0:
      ea = instLowByte;
      break;
    case 1:
      ea = (addr + 1 + signExtend (instLowByte)) & WORD_MASK;
      break;
    case 2:
    case 3:
      ea = (ac [inst98] + signExtend (instLowByte)) & WORD_MASK;
    }
  
  switch (instruction >> 12)
    {
    case 0x00:
      if ((instruction & 0x0800) == 0)
	{
	  switch ((instruction >> 7) & 0xf)
	    {
	    case 0x0:  sprintf (buf, "HALT");
	      return;
	    case 0x1:  sprintf (buf, "PUSHF");
	      return;
	    case 0x2:  sprintf (buf, "RTI %d", instruction & 0x7f);
	      return;
	    // case 0x3:  RSTK, SSTK, RSAC, SSAC, MOUT, MIN, BOUT, BIN) POWR I/O
	    case 0x4:  sprintf (buf, "RTS %d", instruction & 0x7f);
	      return;
	    case 0x5:  sprintf (buf, "PULLF");
	      return;
	    // case 0x6:  JSRP (EIS)
	    case 0x7:  sprintf (buf, "JSRI %05x", 0xff80 + (instruction & 0x7f));
	      return;
	    case 0x8:  sprintf (buf, "RIN %d", instruction & 0x7f);
	      return;
	    // case 0x9:  MPY, DIV, DADD, DSUB, LDB, STB (EIS)
	    // case 0xa:  JMPP, ISCAN, JINT (EIS), SCAN, SRCH, MSCAN (POWR I/O)
	    case 0xc:  sprintf (buf, "ROUT %d", instruction & 0x7f);
	      return;
	    // case 0xe:  SETST, CLRST, SETBIT, CLRBIT, SKSTF, SKBIT, CMPBIT (EIS)
	    }
	}
      else
	{
	  if ((instruction & 0x0080) == 0)
	    sprintf (buf, "SFLG %s,%d", ext_flag_name [(instruction >> 8) & 7], instruction & 0x7f);
	  else
	    sprintf (buf, "PFLG %s,%d", ext_flag_name [(instruction >> 8) & 7], instruction & 0x7f);
	  return;
	}
      sprintf (buf, "HALT");
      return;
    case 0x1:
      sprintf (buf, "BOC %s, %05x", boc_cond_name [(instruction >> 12) & 0xf], target);
      return;
    case 0x2:
      switch ((instruction >> 10) & 3)
	{
	case 0:
	  sprintf (buf, "JMP %05x", ea);
	  return;
	case 1:
	  sprintf (buf, "JMP @%05x", ea);
	  return;
	case 2:
	  sprintf (buf, "JSR %05x", ea);
	  return;
	case 3:
	  sprintf (buf, "JSR @%05x", ea);
	  return;
	}
    case 0x3:
      switch (instruction & 0x83)
	{
	case 0x00:
	  break;
	case 0x01:
	  break;
	case 0x02:
	  break;
	case 0x03:
	  break;
	case 0x80:
	  sprintf (buf, "RXCH %d,%d", inst1110, inst98);
	  break;
	case 0x81:
	  sprintf (buf, "RCPY %d,%d", inst1110, inst98);
	  break;
	case 0x82:
	  sprintf (buf, "RXOR %d,%d", inst1110, inst98);
	  break;
	case 0x83:
	  sprintf (buf, "RAND %d,%d", inst1110, inst98);
	  break;
	}
    case 0x4:
      switch ((instruction >> 10) & 3)
	{
	case 0:
	  sprintf (buf, "PUSH %d", inst98);
	  return;
	case 1:
	  sprintf (buf, "PULL %d", inst98);
	  return;
	case 2:
	  sprintf (buf, "AISZ %d,%05x", inst98, signExtend (instLowByte));
	  return;
	case 3:
	  sprintf (buf, "LI %05x", signExtend (instLowByte));
	  return;
	}
    case 0x5:
      switch ((instruction >> 10) & 3)
	{
	case 0:
	  sprintf (buf, "CAI %d,%05x", inst98, signExtend (instLowByte));
	  return;
	case 1:
	  sprintf (buf, "XCHRS %d", inst98);
	  return;
	case 2:
	  if ((instLowByte & 0x80) == 0)
	    sprintf (buf, "ROL %d,%d", inst98, instLowByte);
	  else
	    sprintf (buf, "ROR %d,%d", inst98, -signExtend (instLowByte));
	  return;
	case 3:
	  if ((instLowByte & 0x80) == 0)
	    sprintf (buf, "SHR %d,%d", inst98, instLowByte);
	  else
	    sprintf (buf, "SHL %d,%d", inst98, -signExtend (instLowByte));
	  return;
	}
    case 0x6:
      if ((instruction & 0x0800) == 0)
	sprintf (buf, "AND %d,%05x", (instruction >> 10) & 1, ea);
      else
	sprintf (buf, "OR %d,%05x", (instruction >> 10) & 1, ea);
      return;
    case 0x7:
      switch ((instruction >> 10) & 3)
	{
	case 2:
	  sprintf (buf, "ISZ %05x", ea);
	  return;
	case 3:
	  sprintf (buf, "DSZ %05x", ea);
	  return;
	}
      break;
    case 0x8:
      sprintf (buf, "LD %d,%05x", inst1110, ea);
      return;
    case 0x9:
      sprintf (buf, "LD %d,@%05x", inst1110, ea);
      return;
    case 0xa:
      sprintf (buf, "ST %d,%05x", inst1110, ea);
      return;
    case 0xb:
      sprintf (buf, "ST %d,@%05x", inst1110, ea);
      return;
    case 0xc:
      sprintf (buf, "ADD %d,%05x", inst1110, ea);
      return;
    case 0xd:
      sprintf (buf, "SUB %d,%05x", inst1110, ea);
      return;
    case 0xe:
      sprintf (buf, "SKG %d,%05x", inst1110, ea);
      return;
    case 0xf:
      sprintf (buf, "SKNE %d,%05x", inst1110, ea);
      return;
    default:
      sprintf (buf, "ill op %04x", instruction);
      return;
    }
}

void executeInstruction ()
{
  // temporaries
  int instruction;
  int inst10;
  int inst1110;
  int inst98;
  int inst76;
  int instLowByte;
  int ea = 0;
  bool condition = false;
  int temp;
  
  if (halt)
    return;
  instruction = mem [pc];
  if (inst_trace)
    {
      char buf [80];
      int i;

      printStack ();
      fprintf (trace_f, "\n");
      for (i = 0; i < 4; i++)
	fprintf (trace_f, "AC%d=%04x ", i, ac [i]);
      fprintf (trace_f, "%s %s ",
	       cy ? "cy" : "  ",
	       lk ? "link" : "    ");
      disassembleInstruction (pc, instruction, buf);
      fprintf (trace_f, "PC=%04x, instruction=%04x: %s\n", pc, instruction, buf);
    }
  if ((word_trace) && (pc == 0x010b))
    {
      if (! inst_trace)
	printStack ();
      fprintf (trace_f,"\n");
      fprintf (trace_f,"executing word at %04x: %04x ", ac [2], mem [ac [2]]);
      printWordName (mem [ac [2]]);
      fprintf (trace_f,"\n");
    }
  pc = (pc + 1) & WORD_MASK;

  inst10 = (instruction >> 10) & 0x1;
  inst1110 =  (instruction >> 10) & 0x03;
  inst98 = (instruction >> 8) & 0x03;
  inst76 = (instruction >> 6) & 0x03;
  instLowByte = instruction & BYTE_MASK;

  switch (inst98)
    {
    case 0:  // base page
      ea = instLowByte;
    case 1:  // PC relative
      ea = (pc + signExtend (instLowByte)) & WORD_MASK;
      break;
    case 2:  // indexed
    case 3:
      ea = (ac [inst98] + signExtend (instLowByte)) & WORD_MASK;
    }

  switch (instruction >> 12)
    {
    case 0x00:
      if ((instruction & 0x0800) == 0)
	{
	  switch ((instruction >> 7) & 0xf)
	    {
	    case 0x0:  // HALT
	      halt = true;
	      break;
	    case 0x1:  // PUSHF
	      push (getFR ());
	      break;
	    case 0x2:  // RTI
	      pc = (pull () + instLowByte) & WORD_MASK;
	      int_en = true;
	      break;
	    // case 0x3:  RSTK, SSTK, RSAC, SSAC, MOUT, MIN, BOUT, BIN) POWR I/O
	    case 0x4:  // RTS
	      pc = (pull () + instLowByte) & WORD_MASK;
	      break;
	    case 0x5:  // PULLF
	      setFR (pull ());
	      break;
	    // case 0x6:  JSRP (EIS)
	    case 0x7:  // JSRI
	      push (pc);
	      pc = 0xff80 + (instLowByte & 0x7f);
	      break;
	    case 0x8:  // RIN
	      halt = true; // $$$
	      break;
	    // case 0x9:  MPY, DIV, DADD, DSUB, LDB, STB (EIS)
	    // case 0xa:  JMPP, ISCAN, JINT (EIS), SCAN, SRCH, MSCAN (POWR I/O)
	    case 0xc:  // ROUT
	      halt = true; // $$$
	      break;
	    // case 0xe:  SETST, CLRST, SETBIT, CLRBIT, SKSTF, SKBIT, CMPBIT (EIS)
	    }
	}
      else
	{
	  if ((instruction & 0x0080) == 0)
	    setFlag ((instruction >> 8) & 0x0f);  // SFLG
	  else
	    pulseFlag ((instruction >> 8) & 0x0f);  // PFLG
	}
      break;
    case 0x01:	// BOC:  branch on condition
      switch ((instruction >> 8) & 0xf)
	{
	case 0x0:  condition = stack_full ();   break;
	case 0x1:  condition = (ac [0] == 0);  break;
	case 0x2:  condition = (((ac [0] >> 15) & 1) == 0);  break;
	case 0x3:  condition = ((ac [0] & 1) != 0);  break;
	case 0x4:  condition = (((ac [0] >> 1) & 1) != 0);  break;
	case 0x5:  condition = (ac [0] != 0);  break;
	case 0x6:  condition = (((ac [0] >> 1) & 2) != 0);  break;
	case 0x7:  condition = cont_in;  break;
	case 0x8:  condition = lk;  break;
	case 0x9:  condition = int_en;  break;
	case 0xa:
	  if (sel)
	    condition = ov;
	  else
	    condition = cy;
	  break;
	case 0xb:  condition = (((ac [0] >> 15) & 1) != 0);  break;
	case 0xc:  condition = jc12;  break;
	case 0xd:  condition = jc13;  break;
	case 0xe:  condition = jc14;  break;
	case 0xf:  condition = jc15;  break;
	}
      if (condition)
	pc = (pc + signExtend (instruction & BYTE_MASK)) & WORD_MASK;
      break;
    case 0x02:
      if (instruction >> 11)
	{
	  push (pc);	  // JSR, JSR@: push PC
	}
      if (instruction >> 10)
	{
	  pc = mem [ea];  // JMP@, JSR@: indirect
	}
      pc = ea;
      break;
    case 0x03:
      if ((instruction & 0x0080) == 0)
	{
	  // RADD
	  ac [inst98] = add (ac [inst98], ac [inst76], false);
	}
      else
	switch (instruction & 0x03)
	  {
	  case 0x00:  // RXCH
	    temp = ac [inst98];
	    ac [inst98] = ac [inst76];
	    ac [inst76] = temp;
	    break;
	  case 0x01:  // RCPY
	    ac [inst98] = ac [inst76];
	    break;
	  case 0x02:  // RXOR
	    ac [inst98] ^= ac [inst76];
	    break;
	  case 0x03:  // RAND
	    ac [inst98] &= ac [inst76];
	    break;
	  }
      break;
    case 0x04:
      switch ((instruction >> 10) & 0x03)
	{
	case 0x00:  // PUSH
	  push (ac [inst98]);
	  break;
	case 0x01:  // PULL
	  ac [inst98] = pull ();
	  break;
	case 0x02:  // AISZ
	  ac [inst98] = (ac[inst98] + signExtend (instLowByte)) & WORD_MASK;
	  if (ac [inst98] == 0)
	    pc = (pc + 1) & WORD_MASK;
	  break;
	case 0x03:  // LI
	  ac [inst98] = signExtend (instLowByte);
	  break;
	}
      break;
    case 0x05:
      switch ((instruction >> 10) & 0x03)
	{
	case 0x00:  // CAI
	  ac [inst98] = ((ac [inst98] ^ WORD_MASK) + signExtend (instLowByte)) & WORD_MASK;
	  break;
	case 0x01:  // XCHRS
	  temp = ac [inst98];
	  if (sp < 0)
	    ac [inst98] = WORD_MASK;
	  else
	    {
	      ac [inst98] = stack [sp];
	      stack [sp] = temp;
	    }
	  break;
	case 0x02:  // ROL/ROR
	  if (instLowByte == 0)
	    break;
	  if (sel)
	    temp = rotateLeft (ac [inst98] |
			       (lk ? (1 << 16) : 0), 17, instLowByte);
	  else
	    temp = rotateLeft (ac [inst98], 16, instLowByte);
	  ac [inst98] = temp & WORD_MASK;
	  if (sel)
	    lk = ((temp >> 16) & 1) != 0;
	  break;
	case 0x03:  // SHL/SHR
	  if (instLowByte == 0)
	    break;
	  if (sel)
	    temp = shiftLeft (ac [inst98] |
			       (lk ? (1 << 16) : 0), 17, instLowByte);
	  else
	    temp = shiftLeft (ac [inst98], 16, instLowByte);
	  ac [inst98] = temp & WORD_MASK;
	  if (sel)
	    lk = ((temp >> 16) & 1) != 0;
	  break;
	}
      break;
    case 0x06:
      if ((instruction & 0x0800) == 0)
	{
	  // AND
	  ac [inst10] = ac [inst10] & mem [ea];
	}
      else
	{
	  // OR
	  ac [inst10] = ac [inst10] | mem [ea];
	}
      break;
    case 0x07:
      if ((instruction & 0x0800) == 0)
	{
	  // SKAZ
	  condition = ((ac [inst10] & mem [ea]) == 0);
	  if (condition)
	    pc = (pc + 1) & WORD_MASK;
	}
      else if ((instruction & 0x0400) == 0)
	{
	  // ISZ
	  mem [ea] = (mem [ea] + 1) & WORD_MASK;
	  condition = (mem [ea] == 0);
	  if (condition)
	    pc = (pc + 1) & WORD_MASK;
	}
      else
	{
	  // DSZ
	  mem [ea] = (mem [ea] - 1) & WORD_MASK;
	  condition = (mem [ea] == 0);
	  if (condition)
	    pc = (pc + 1) & WORD_MASK;
	}
      break;
    case 0x08:  // LD
      ac [inst1110] = mem [ea];
      break;
    case 0x09:  // LD@
      ac [inst1110] = mem [ mem [ea]];
      break;
    case 0x0a:  // ST
      mem [ea] = ac [inst1110];
      break;
    case 0x0b:  // ST@
      mem [mem [ea]] = ac [inst1110];
      break;
    case 0x0c:  // ADD
      ac [inst1110] = add (ac [inst1110], mem [ea], false);
      break;
    case 0x0d:  // SUB
      ac [inst1110] = add (ac [inst1110], mem [ea] ^ WORD_MASK, 1);
      break;
    case 0x0e:  // SKG
      condition = (signedValue (ac [0]) >
		   signedValue (mem [ea]));
      if (condition)
	pc = (pc + 1) & WORD_MASK;
      break;
    case 0x0f:  // SKNE
      condition = (ac [inst1110] != mem [ea]);
      if (condition)
	pc = (pc + 1) & WORD_MASK;
      break;
    default:
      halt = true;  // $$$ illegal opcode
      break;
    }
}

int loadLine (char *fn, int lineNo, char *buf, int expectedAddr)
{
  int addr = expectedAddr;
  int data;
  int w;

  w = sscanf (buf, "%x: %x", & addr, & data);
  if (w != 2)
    {
      printf ("%s[%d]: bogus '%s'\n", fn, lineNo, buf);
    }

  if (addr != expectedAddr)
    {
      // printf ("jumped from %04x to %04x\n", expectedAddr, addr);
      expectedAddr = addr;
    }
  
  mem [addr++] = data;
  return addr;
}

void loadHexFile (char *name)
{
  FILE *f;
  char buf [120];
  int lineCount = 0;
  int expectedAddr = -1;

  f = fopen (name, "r");
  if (! f)
    {
      fprintf (stderr, "can't open hex file '%s'\n", name);
      exit (2);
    }

  while (fgets (buf, sizeof (buf), f))
    {
      lineCount++;
      expectedAddr = loadLine (name, lineCount, buf, expectedAddr);
    }

  fclose (f);
}


bool consoleInputAvail ()
{
  return false;  // $$$
}

// blocking read one character from console
int consoleInputCharacter (void)
{
  int b = fgetc (stdin);
  b &= 0x7f;
  if (b == '\n')
    b = '\r';
  return b;
}

void consoleOutputCharacter (int c)
{
  if (c == '\r')
    c = '\n';
  fprintf (stdout, "%c", c);
}

int get_mem_byte (int addr)
{
  if (addr & 1)
    return mem [addr >> 1] & 0xff;
  else
    return mem [addr >> 1] >> 8;
}

void put_mem_byte (int addr, int b)
{
  if (addr & 1)
    mem [addr >> 1] = ((mem [addr >> 1]) & 0xff00) | (b & 0xff);
  else
    mem [addr >> 1] = ((mem [addr >> 1]) & 0x00ff) | ((b & 0xff) << 8);
}

// addr is word addr
#define BLOCK_SIZE 128
#define FIRST_BLOCK 8
void block_io (int addr, int block, bool read)
{
  int baddr = addr << 1;
  int count;
  int b;
  int new_pos;

  fprintf (stdout, "%sing block %d, addr %04x, byte addr %04x\n",
	   (read ? "read" : "write"), block, addr, baddr);
  if (block < FIRST_BLOCK)
    return;

  new_pos = (block - FIRST_BLOCK) * BLOCK_SIZE;
  fprintf (stderr, "seeking to %d\n", new_pos);
  if (fseek (block_f, new_pos, SEEK_SET) < 0)
    {
      fprintf (stderr, "error seeking to %d\n", new_pos);
      return;
    }
  count = BLOCK_SIZE;
  while (count--)
    {
      if (read)
	{
	  b = fgetc (block_f);
	  if (b < 0)
	    {
	      fprintf (stdout, "end of file\n");
	      return;
	    }
	  put_mem_byte (baddr++, b);
	}
      else
	{
	  b = get_mem_byte (baddr++);
	  fputc (b, block_f);
	}
    }
}

#define ABSTTY_BASE    0x7e00
#define ABSTTY_SIZE    0x0100

#define ABSTTY_GETC    0x7e3b
#define ABSTTY_SAV     0x7e94
#define ABSTTY_PUTC    0x7e59
#define ABSTTY_GECO    0x7e73
//#define ABSTTY_DPLX    0x7e9c
#define ABSTTY_MESG    0x7ec3
#define ABSTTY_PUT2C   0x7ed3
#define ABSTTY_RESET   0x7eda
#define ABSTTY_INTEST  0x7edf
#define ABSTTY_LDM     0x7eea
#define ABSTTY_STM     0x7ef2  // 0x7efa according to IMP-16P man V1 p.7-19

#define ABSTTY_BLOCKIO 0x7eff  // my own hack for disk I/O

void run (void)
{
  int c;

  loadHexFile ("figforth_imp16.obj");

  block_f = fopen (block_fn, "r+b");
  if (! block_f)
    {
      fprintf (stderr, "can't open block file '%s'\n", block_fn);
      exit (2);
    }
	
  pc = 0x10;
  halt = false;
  while (! halt)
    {
      if ((pc >= ABSTTY_BASE) &&
	  (pc <= ABSTTY_BASE + ABSTTY_SIZE))
	{
	  switch (pc)
	    {
	    case ABSTTY_GETC:
	      ac [0] = consoleInputCharacter ();
	      pc = pull ();
	      continue;;
	    case ABSTTY_PUTC:
	      c = ac [0] & 0x7f;
	      consoleOutputCharacter (c);
	      if (c == 0x0d)
		consoleOutputCharacter (0x0a);
	      pc = pull ();
	      continue;
	    case ABSTTY_INTEST:
	      // return with skip if no input ready
	      if (consoleInputAvail ())
		pc = pull ();
	      else
		pc = (pull () + 1) & WORD_MASK;
	      continue;
	    case ABSTTY_BLOCKIO:
	      block_io (mem [ac [3] + 2], mem [ac [3] + 1], mem [ac [3]] != 0);
	      pc = pull ();
	      continue;
	    default:
 	      halt = true;
	      continue;
	    }
	}
      else
	executeInstruction ();
    }
  printf ("halted at %04x\n", pc);
}


static struct termios orig_trm;

void get_tty_settings (void)
{
  tcgetattr(STDIN_FILENO, & orig_trm); // get the current settings
}

void restore_tty_settings (void)
{
  tcsetattr(STDIN_FILENO, TCSANOW, & orig_trm); // restore original settings
}

void set_tty_raw (bool raw)
{
  struct termios trm;

  tcgetattr(STDIN_FILENO, & trm); /* get the current settings */

  if (raw)
    {
      trm.c_cc[VMIN] = 1;     /* return after one byte read */
      trm.c_cc[VTIME] = 0;    /* block forever until 1 byte is read */

      trm.c_lflag &= ~(ECHO | ICANON | IEXTEN);
      /* echo off, canonical mode off, extended input
	 processing off */

    }

  tcsetattr(STDIN_FILENO, TCSANOW, &trm); /* set the terminal with the new
					     settings */
}

int main (int argc, char *argv [])
{
  get_tty_settings ();
  set_tty_raw (true);
  run ();
  restore_tty_settings ();
  exit (0);
}
