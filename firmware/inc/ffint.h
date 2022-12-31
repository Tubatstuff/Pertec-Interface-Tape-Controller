//-------------------------------------------
// Integer type definitions for FatFs module 
//-------------------------------------------

#ifndef FF_INTEGER
#define FF_INTEGER

#include <stdint.h>

// These types MUST be 16-bit or 32-bit 

typedef int 		INT;
typedef unsigned int 	UINT;

// This type MUST be 8-bit 

typedef uint8_t		BYTE;

// These types MUST be 16-bit 

typedef int16_t		SHORT;
typedef uint16_t  	WORD;
typedef uint16_t  	WCHAR;

// These types MUST be 32-bit 

typedef int32_t		LONG;
typedef uint32_t  	DWORD;
typedef DWORD		LBA_t;		

// This type MUST be 64-bit (Remove this for ANSI C (C89) compatibility) 

typedef uint64_t    	QWORD;

#endif
