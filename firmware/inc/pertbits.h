#ifndef _PERTBITS_
#define _PERTBITS_

//*	Pertec register bit definitions.
//	--------------------------------
//
//	Nota Bene:  In all of the following, the bit meaning is 
//	low-active.  That is a "0" implies true; a "1" implies false.
//

//	Status register 0 bits

#define PS0_IRP		1		// Read data parity bit
#define PS0_IDBY	2		// Data busy
#define PS0_ISPEED	4		// Drive is operating in high speed
#define PS0_RDAVAIL	8		// Read data available
#define PS0_WREMPTY	16		// Write data buffer empty
#define PS0_IFMK	32		// File mark detected
#define	PS0_IHER	64		// Hard error encountered
#define PS0_ICER	128		// Corrected error encountered

//	Status register 1 bits

#define	PS1_INRZ	(1 << 8)	// Varies, can be 800 bpi mode
#define PS1_EOT		(2 << 8)	// End of tape
#define PS1_IONL	(4 << 8)	// Online
#define PS1_IFPT	(8 << 8)	// File protect (no ring)
#define PS1_IRWD	(16 << 8)  	// Drive is rewinding
#define PS1_ILDP	(32 << 8)	// Tape is at load point
#define PS1_IRDY	(64 << 8)	// Drive is loaded and ready
#define PS1_IFBY	(128 << 8)	// Formatter busy

//	Command bits.

//	Command register 0

#define PC_IGO		1		// "GO" pulsed to start motion
#define PC_ILWD		2		// "Last word" during write
#define PC_ILOL		4		// Load and go online (few drives)
#define PC_IREV		8		// Reverse
#define PC_IREW		16		// Rewind
#define PC_IWRT		32		// Write operation
#define PC_IRTH1	64		// Read threshold 1 
#define PC_IRTH2	128		// Read threshold 2

//	Command Register 1

#define PC_IWFM		(1 << 8)	// Write file mark
#define PC_IERASE	(2 << 8)	// Long gap erase
#define	PC_IEDIT	(4 << 8)	// Edit mode
#define PC_IRWU		(8 << 8)	// Rewind and unload
#define PC_IHSP		(16 << 8)	// High speed operation
#define PC_ITAD1	(32 << 8)	// drive address bit 1
#define PC_ITAD0	(64 << 8)	// drive address bit 0
#define PC_IFAD		(128 << 8)	// Formatter address

//	For Control Register bits, please see "gpiodefs.h".

#endif