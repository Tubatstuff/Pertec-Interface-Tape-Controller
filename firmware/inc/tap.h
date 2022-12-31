#ifndef _TAP_INC
#define _TAP_INC  1

//  A .TAP file basically consists of a 32-bit header record, followed by data,
//  followed by a matching 32-bit trailer record.   The exceptions are for
//  filemark (always 0 length data) and end-of-medium (nothing follows).  In
//  all other cases, the length is present in the lower 24 bits of the header,
//  with the upper 8 bits signifying either an error, or special condition.

//  Special constants for TAP header records--all are 32 bits, little-endian.

#define TAP_FILEMARK 	0x0		// 0 = filemark
#define TAP_EOM	 	0xffffffff	// -1 = end of medium
#define TAP_ERASE_GAP 	0xfffffffe	// -2 = erase gap
#define TAP_ERROR_FLAG 	0x80000000L	// error flag bit
#define TAP_LENGTH_MASK 0x00ffffffL	// mask for length

//  Nota Bene:
//  Sydex adds a record after the EOM marker that contains documentation
//  for the tape.  Free-form, ASCII.  Eventually, MIME-encoded images might
//  be added. Since almost all utilities stop examining data after EOM, this
//  generally has no effect on operability.

#endif
