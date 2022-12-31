#ifndef GLOBAL_H_DEFINED
#define GLOBAL_H_DEFINED 1

#define VERSION "0.99"

#ifndef MAIN
#define SCOPE extern
#else
#define SCOPE
#endif

#ifndef BLOCK_SIZE
#define BLOCK_SIZE 512
#endif

#define NO_OP {}          // avoids misplaced semicolons

//  Milliseconds since boot.

SCOPE volatile uint32_t Milliseconds;      // Just keeps counting

//  Common Buffer.

SCOPE uint8_t __attribute__ ((aligned(4)))
    Buffer[BLOCK_SIZE]; // buffer block size

//	Tape buffer - 65K bytes.

#define TAPE_BUFFER_SIZE 65536

SCOPE uint8_t __attribute__ ((aligned(4))) 
    TapeBuffer[TAPE_BUFFER_SIZE];

SCOPE int 
  TapeRetries;			// how many retries on tape reads?

SCOPE int
  StopTapemarks;		// stop at how many consecutivetapemarks?
SCOPE bool
  StopAfterError;		// true if stopping after error

SCOPE uint16_t
  TapeAddress;			// address of tape drive

#undef SCOPE
#endif
