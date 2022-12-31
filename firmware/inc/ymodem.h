//*  YMODEM definitions.
//

#ifndef _ymodem_included_
#define _ymodem_included_

// Error code returns.

typedef enum 
{
  XERR_SUCCESS  = 0,
  XERR_MOUNT_ERROR,	// couldn't mount sd card
  XERR_NO_FILE,		// can't find the file
  XERR_TIMEOUT,		// conversation timed out
  XERR_ABORT,		// CAN received
  XERR_CORRUPT,		// Data corrupted error
  XERR_ENDFILE		// End of file (not really an error)
} XERR_CODE;


XERR_CODE SendYmodem ( char *FileName);
XERR_CODE ReceiveYmodem( void);

#endif

