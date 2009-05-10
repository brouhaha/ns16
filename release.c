/*
Copyright 2004, 2009 Eric Smith <eric@brouhaha.com>
All rights reserved.
$Id$
*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "util.h"

#ifdef PROGRAM_RELEASE
char *program_release = "release " MAKESTR(PROGRAM_RELEASE);
#else
char *program_release = "non-released";
#endif
