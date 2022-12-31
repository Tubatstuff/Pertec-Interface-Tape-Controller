#ifndef _TAPEDRIVER_INC
#define _TAPEDRIVER_INC

#include <stdbool.h>
#include <stdint.h>

//  Tape Status values:

#define TSTAT_OFFLINE   0x80	// Drive is offline
#define TSTAT_HARDERR   0x40	// Formatter detected a hard error
#define TSTAT_CORRERR   0x20	// Formatter corrected an error
#define TSTAT_TAPEMARK  0x10	// Record is a tape mark
#define TSTAT_EOT       0x08	// End of tape encountered
#define TSTAT_BLANK     0x04	// Blank tape read
#define TSTAT_LENGTH	0x02	// Physical block longer than buffer
#define TSTAT_PROTECT	0x01	// Tape is write protected
#define TSTAT_NOERR     0x00	// No error detected

//  Global prototypes


unsigned int TapeRead( uint8_t *Buf, int Buflen, int *BytesRead);
unsigned int TapeWrite( uint8_t *Buf, int Buflen);
unsigned int SkipBlock( int Dir);
unsigned int SpaceFile( int Dir);
unsigned int TapeRewind( void);
unsigned int  TapeUnload( void);
void TapeInit( void);
void SetTapeAddress( uint16_t What);
void Set1600( void);
void Set6250( void);
bool IsTapeOnline( void);
bool IsTapeEOT(void);
bool IsTapeProtected( void);
uint16_t TapeStatus( void);
void IssueTapeCommand( uint16_t What);

#endif
