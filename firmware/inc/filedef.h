#ifndef _FILEDEF_INC
#define FILEDEF_INC

// File global definitions.

#include "ff.h"

#ifndef MAIN
#define SCOPE extern
#else
#define SCOPE
#endif

SCOPE char
  CurrentPath[256];             // current search path

SCOPE FATFS
  SDfs;

#endif