//  usbserial - procedures for usb and UART serial I/O.

#ifndef _USBSERIAL
#define _USBSERIAL

int USInit( void);  		// initialize
void USClear( void);		// clear buffer contents
int USPutchar( char);   	// put a character
int USWritechar( char);		// "raw" write character
int USGetchar( void);   	// get a character
int USCharReady( void); 	// test if character ready
void USPuts( char *What);	// put string
int USWriteBlock( uint8_t *What, int Count);	// write a block of data

#endif
