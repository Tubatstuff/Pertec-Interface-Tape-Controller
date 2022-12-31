//*	Tape Utilities.  
//  	---------------
//
//	Note that low-level tape interface is done in tapedriver.c
//

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

// MCU-specific definitions.

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/dma.h>

#include "license.h"

// Local definitions.

#include "comm.h"
#include "gpiodef.h"
#include "miscsubs.h"
#include "rtcsubs.h"
#include "globals.h"
#include "filedef.h"
#include "tapeutil.h"
#include "tapedriver.h"
#include "pertbits.h"
#include "tap.h"

//  Local variables.

static uint32_t
  LastRecordLength,                   // length of last record
  LastRecordCount,                    // last record count
  TapePosition,
  BytesCopied;                        // number of bytes copies   

// Local prototypes.

static void GetComment( char *Filename);
static void AddRecordCount( uint32_t RecordLength);
static void FlushRecordCount( void);
static char *TranslateError( uint16_t Status);
static bool CheckForEscape( void);

//* N.B. - Routines invoked from cli are prefixed with "Cmd"

//*	Drive Status table.
//

typedef struct _drive_stat_table
{

  uint16_t BitVal;		// bit in status register
  char *SetTxt;			// what is the value of it set
} DRIVE_STATUS_TEXT;	

static DRIVE_STATUS_TEXT DriveStatusTable[] =
{
  {PS0_IRP, "Odd"},
  {PS0_IDBY, "Data busy"},
  {PS0_ISPEED, "High speed"},
  {PS0_RDAVAIL, "Read full"},
  {PS0_WREMPTY, "Write empty"},
  {PS0_IFMK, "Tape mark"},
  {PS0_IHER, "Hard error"},
  {PS0_ICER, "Soft error"},
  {PS1_INRZ, "NRZI mode"},
  {PS1_EOT, "EOT"},
  {PS1_IONL, "Online"},
  {PS1_IFPT, "Protected"},
  {PS1_IRWD, "Rewinding"},
  {PS1_ILDP, "Load point"},
  {PS1_IRDY, "Ready"},
  {0,0}
};


//*   Translate 16-bit error status to ASCII.
//    ---------------------------------------
//
//	On entry, error status.
//	On return, pointer to ASCII message and severity flag.
//

static char *TranslateError( uint16_t Status)
{

//  The three parallel static structures.

  static const uint16_t  errFlag[] =
    { TSTAT_OFFLINE, TSTAT_HARDERR, TSTAT_CORRERR, TSTAT_TAPEMARK,
      TSTAT_EOT, TSTAT_BLANK, TSTAT_LENGTH, TSTAT_PROTECT, TSTAT_NOERR};
  static const char *errMsg[] =
    { "Drive is offline",
      "Unrecoverable data error",
      "Corrected data error",
      "Tape mark hit",
      "End of tape",
      "Blank tape read",
      "Buffer overrun",
      "Write protected",
      0
    };
             
  int i;
    
  for( i = 0; errMsg[i]; i++)
  {
    if ( Status & errFlag[i])
      return (char *) errMsg[i];
  } // for
    
// No errors, so return a null string.

  return "";
} // TranslateError

//*  CmdShowStatus - Show detailed tape status
//   -----------------------------------------
//
//	Runs through all of the status bits, prints the ones that are set.
//

void CmdShowStatus( char *args[])
{

  int 
    i;
  uint16_t 
    stat;

  (void) args;
  stat = TapeStatus();          // fetch tape status
  Uprintf( "\nTape Status = %04x\n\n", stat);
  for ( i = 0; i < 16; i++)
  {
    if ( DriveStatusTable[i].SetTxt == 0)
      break;			// end of table
    if ( stat & DriveStatusTable[i].BitVal)
    {
      Uprintf( " %04x  %s\n", 
        DriveStatusTable[i].BitVal, DriveStatusTable[i].SetTxt); 
    }
  } // look for a status
  Uprintf( "\n");
} // CmdShowStatus


//*  ShowBriefStatus - Show tape status.
//   ---------------------------------
//
//

void ShowBriefStatus(void)
{

  uint16_t 
    stat;
  int 
    i;
    
//  Mask out the ones we want to show.

  stat = TapeStatus();		// fetch tape status 
  stat &= (PS1_IONL | PS1_ILDP | PS1_EOT | PS1_IRDY | PS1_IFPT);
  if ( stat & PS1_ILDP)
    TapePosition = 0;			// if at loadpoint
  Uprintf( "\nTape Status: ");
  for ( i = 0; i < 16; i++)
  {
    if ( DriveStatusTable[i].SetTxt == 0)
      break;			// end of table
    if ( stat & DriveStatusTable[i].BitVal)
    {
      Uprintf( " %s", DriveStatusTable[i].SetTxt); 
    }
  } // look for a status
  Uprintf( " Stop on %d tapemarks\n\n", StopTapemarks);
} // ShowBriefStatus

//  CmdSetAddr - Set drive/formatter address.
//  -----------------------------------------
//
//      Format is F10
//

void CmdSetAddr( char *args[])
{

  uint16_t taddr;
  
  taddr =  (uint16_t) strtoul( args[0], NULL,16);
  SetTapeAddress( taddr);
  Uprintf( "Set tape address to %d\n", taddr);
  return;
} // CmdSetAddr


//*	CmdSetRetries - Set number of retries.
//	--------------------------------------
//
//	Note that in our case, a "retry" involves a read-reverse
//	followed by a read-forward.
//

void CmdSetRetries( char *args[])
{

  if ( args[0])
  { // if present
    if (isdigit(*args[0]))
    { // has to be numeric
      TapeRetries = *args[0] - '0';	// get the number
    }
    else
      Uprintf( "Specify a number between 0 and 9, inclusive\n");
  } // if present
  Uprintf( "Tape error retries: %d\n", TapeRetries);
  return;
} // CmdSetRetries

//*    CmdSetStop - Set Stop Condition.
//     --------------------------------
//
//      End can be 0 (read to EOI), 1 or 2 consecutive tapemarks.
//

void CmdSetStop( char *args[])
{

  if ( args[0])
  {
    StopAfterError = false;
    StopTapemarks = atoi( args[0]);
    if ( StopTapemarks <= 0)
    {
      Uprintf( "Error - tape mark count must be positive.\n");
      StopTapemarks = 2;
    }
    
    if ( args[1])
    { // look for an "E"
      if ( toupper( *args[1]) == 'E')
        StopAfterError = true;
      else
        Uprintf( "\nDon\'t understand \"%c\" - did you mean \"E\"?\n",
          *args[1]);
    }
  } // if we have one argument
  else
    StopTapemarks = 2;			// default to 2

  Uprintf( "Reading stops after %d consecutive tapemarks\n",
    StopTapemarks);
  if ( StopAfterError)
    Uprintf( "...or at first error\n");
  return;
} // CmdSetStop

//*	CmdInitTape - Initialize Tape system.
//	----------------------------------
//
//	Just calls TapeInit in tapedriver.c
//

void CmdInitTape( char *args[])
{

  (void) args;
  
  TapeInit();
  return;
} // CmdInitTape

//*	CmdRewindTape - Rewind tape immediately.
//	----------------------------------------
//
//	Calls the tapedriver.c routine.
//

void CmdRewindTape( char *args[])
{

  unsigned int 
    stat;

  (void) args;
  
  TapePosition = 0;		// say we're rewind
  stat = TapeRewind();		// invoke routine in tapedriver.c
  if ( stat != TSTAT_NOERR)
    Uprintf( "%s\n", TranslateError( stat));
  
  return;
} // CmdRewindTape  

//*	CmdUnload - Unload tape and go offline.
//	---------------------------------------
//

void CmdUnloadTape( char *args[])
{

  unsigned int
    stat;
    
  (void) args;
  
  stat = TapeUnload();
  if ( stat != TSTAT_NOERR)
    Uprintf( "%s\n", TranslateError( stat));

  return;
} // CmdUnloadTape	


//*	CmdReadForward - Read and display the next tape block.
//	------------------------------------------------------
//
//	Displays any error message and the number of bytes read as
//	well as the first 256 bytes of the block.
//

void CmdReadForward( char *args[])
{
 
  unsigned int
    status;
  int
    bytesRead;
 
#define BLOCK_DISPLAY_COUNT 256
 
  (void) args;

  if (!IsTapeOnline())
  {
    Uprintf( "Error - tape drive is offline.\n");
    return;
  }
  
  status = TapeRead( TapeBuffer, TAPE_BUFFER_SIZE, &bytesRead);
  if (status != TSTAT_NOERR)
    Uprintf( "%s\n ", TranslateError( status));	// show any error
  
  if ( bytesRead)
  {
    Uprintf("%d bytes in block %d:\n", bytesRead, TapePosition);
    ShowBuffer( TapeBuffer, 
      (bytesRead < BLOCK_DISPLAY_COUNT) ? bytesRead : BLOCK_DISPLAY_COUNT);
  }
  TapePosition++;
  Uprintf("\n");
  return;
} // CmdReadForward

//*	CmdSkip - Space one or more blocks.
//	-----------------------------------
//

void CmdSkip( char *args[])
{

  int
   skDir,
   skCount;
  unsigned int
    status;

  if ( args[0])
    skCount = atoi( args[0]);
  else
    skCount = 1;			// assume space 1 file forward 
  
  if (skCount == 0)
    return;				// if not moving
    
  if (!IsTapeOnline())
  {
    Uprintf( "Error - tape drive is offline.\n");
    return;
  }
  
  skDir = 1;		// assume forward space
  if ( skCount < 0)
  {
    skCount = -skCount;		// make count positive
    skDir = -1;
  } // determine direction

  while( skCount--)
  {
    status = SkipBlock( skDir);
    if ( TapeStatus() & PS1_ILDP)
    {
      TapePosition = 0;
      break;				// doesn't matter, if LP hit, quit
    }

    if ( status != TSTAT_NOERR)
    {
      Uprintf( "%s\n", TranslateError( status));
      break;
    } // if error
    
    if (skDir < 0)
    {
      if (TapePosition)
        TapePosition --;
    }
    else
      TapePosition ++;
  } // do some backspacing
  return;  
} // CmdSkip

//*	CmdSpace - Sspace one or more files.
//	------------------------------------
//

void CmdSpace( char *args[])
{

  int
   spDir,
   spCount;
  unsigned int
    status;

  if ( args[0])
    spCount = atoi( args[0]);
  else
    spCount = 1;			// assume space 1 file forward 
  
  if (spCount == 0)
    return;				// if not moving
    
  if (!IsTapeOnline())
  {
    Uprintf( "Error - tape drive is offline.\n");
    return;
  }
  
  spDir = 1;		// assume forward space
  if ( spCount < 0)
  {
    spCount = -spCount;		// make count positive
    spDir = -1;
  } // determine direction

  while( spCount--)
  {
    status = SpaceFile( spDir);
    if ( status != TSTAT_NOERR)
    {
      Uprintf( "%s\n", TranslateError( status));
      break;
    } // if error
  } // do some backspacing
  TapePosition = 0;		// relative to file mark in any case
  return;  
} // CmdSpace


//*  CmdTapeDebug - Debugging routine.
//   ---------------------------------
//
//	Useful for testing register function.  Right now, just 
//	sets the command register.
//

void CmdTapeDebug( char *args[])
{

  uint16_t thisCommand;

  if ( args[0])
  {
    thisCommand = (uint16_t) strtol(args[0], NULL, 16);
    Uprintf( "\nSending %08x to command register.\n", thisCommand);
    IssueTapeCommand( thisCommand);
  }
  return;
} //  CmdTapeDebug

//*	CmdCreateImage - Read tape and write an image file.
//	------------------------------------------------
//
//	Currently, the only required argument is the image file name.
//	We may add others later.
//

void CmdCreateImage( char *args[])
{

  FRESULT
    fres;               // file result codes

  FIL 
    tf;                 // our file structure

  bool
    noRewind,		// if true, skip rewinding
    abort;              // flag that we have to stop 

  int
    fileCount,          // how many files?
    tapeMarkSeen;       // how many tape marks in a row?

  uint32_t
    tapeHeader;		// record header/trailer    

  UINT 
    wc;                 // write count (returned)

  if ( !args[0])
  {
    Uprintf( "This command requires an image file name!\n");
    return;
  } // if no arguments
  
  noRewind = false;			// rewind assumed
  if ( args[1])
  {
    if ( toupper( *args[1]) == 'N')
      noRewind = true;			// don't rewind before or after
  } // see if no rewinding
  
//  Rewind the tape if necessary.  If offline, quit.

  if ( !IsTapeOnline())
  {
    Uprintf( "\nTape is offline.\n");
    return;
  } // tape isn't online
    
  if ( !noRewind)
    TapeRewind();

// Note that if not rewinding, our block count is relative to the last
// known position of the tape.

//  Open the file for writing.

  if ( (fres = f_open( &tf, args[0], FA_CREATE_ALWAYS | FA_WRITE)) != FR_OK)
  {
    Uprintf( "\nError in creating file. Error = %d\n", fres);
    return;
  } // if open error         

//  Now copy things.

  LastRecordCount = 0;
  TapePosition = 0;              // block counter
  tapeMarkSeen = 0;
  BytesCopied = 0;
  fileCount = 0;
  abort = false;

  ShowRTCTime();

  while( true)
  { // read until done or abort
  
    unsigned int
      readStat;			// status
    int
      readCount;		// count of bytes read
     
    if ( (abort = CheckForEscape()) )  // Check for ESC key
      break;

//      Read forward.

    readStat = TapeRead( TapeBuffer, TAPE_BUFFER_SIZE, &readCount);
    tapeHeader = readCount;	// save the record count

//	Have a look at the returned status.

    if ( StopAfterError && 
      (readStat & TSTAT_HARDERR))
    {
      Uprintf( "Stopping at error or blank.\n");
      break;
    }

    if ( (readStat & TSTAT_BLANK) || (readStat & TSTAT_EOT))
    { // hit a blank; quit
      Uprintf( "Blank/Erased tape or EOT hit\n");
      break;
    }

//	Check for tapemark.	

    if ( readStat & TSTAT_TAPEMARK)
    {
      tapeMarkSeen++;
      fileCount++;
      readCount = 0;
    }
    else
      tapeMarkSeen = 0;		// not a tapemark, start all over
     
//  Simply note corrected errors     
     
     if ( readStat & TSTAT_CORRERR)
       Uprintf( "At block %d, an error was auto-corrected.\n", TapePosition); 
      
//  Also note length error; set error flag.

    if( readStat & TSTAT_LENGTH)
    {
      Uprintf( "Block too long at %d; truncated and flagged.\n", TapePosition);
      tapeHeader |= TAP_ERROR_FLAG;
    }
    
    if ( readStat & TSTAT_HARDERR)
    {
      Uprintf( "At block %d, an un-corrected error was hit.\n", TapePosition);
      tapeHeader |= TAP_ERROR_FLAG;
    }
    TapePosition++;
    
//  Finally, it's time to write out a record.

    f_write( &tf, &tapeHeader, sizeof( tapeHeader), &wc);          

//  If it's a filemark or other 0-length error, don't write the trailer.

    if ( readCount)
    {
      f_write( &tf, TapeBuffer, readCount, &wc);
      f_write( &tf, &tapeHeader, sizeof( tapeHeader), &wc);          
    }
    
    AddRecordCount( readCount);
    if ( tapeMarkSeen == StopTapemarks)  
    {
      fileCount -= (StopTapemarks -1);
      Uprintf( "%d consecutive tape marks--ending.\n", StopTapemarks);
      break;
    } // if tapemark hit
  } // read the tape
  
// Write an EOM record, rewind and close.

  tapeHeader = TAP_EOM;
  f_write( &tf, &tapeHeader, sizeof( tapeHeader), &wc);          
  if ( abort)
  {
    Uprintf( "Operation terminated by operator.\n");
  }
  f_close( &tf);

  FlushRecordCount();
  Uprintf( "\nFile %s written.\n", args[0]);
  Uprintf( "\n%d blocks read.\n", TapePosition);
  Uprintf( "%d files; %d bytes copied.\n", fileCount, BytesCopied);
  if (!abort)
    GetComment( args[0]);		// get a comment
  if ( !noRewind)
  {
    Uprintf( "Rewinding...\n");
    TapeRewind();
    TapePosition = 0;		// we rewound the tape
  } // rewind if requested
  ShowRTCTime();
  return;
} // CmdCreateImage

//*	CmdWriteImage - Read tape and write an image file.
//	------------------------------------------------
//
//	Currently, the only required argument is the image file name.
//	We may add others later.
//

void CmdWriteImage( char *args[])
{

  FRESULT
    fres;               // file result codes

  FIL 
    tf;                 // our file structure

  int
    fileCount;          // how many files?

  unsigned int
    status;		// tape driver return status

  bool
    noRewind,		// nonzero if skip rewind
    abort;		// nonzero if ESC hit

  noRewind = false;	// assume rewinding

  if ( !args[0])
  {
    Uprintf( "This command requires an image file name!\n");
    return;
  } // if no arguments
  
  if ( args[1])
  {
    if ( toupper( *args[1]) == 'N')
      noRewind = true;			// don't rewind before or after
  } // see if no rewinding
  
//  Open the file for reading.

  if ( (fres = f_open( &tf, args[0], FA_READ)) != FR_OK)
  {
    Uprintf( "\nCan't find file %s. Error = %d\n", args[0], fres);
    return;
  } // if open error         
 
//  Rewind the tape if necessary.  If offline, quit.

  if ( !IsTapeOnline())
  {
    Uprintf( "\nTape is offline.\n");
    return;
  } // tape isn't online
    
  if ( !noRewind)
    TapeRewind();

// Note that if not rewinding, our block count is relative to the last
// known position of the tape.
//
//  Make sure that the tape isn't write-protected.

  if (IsTapeProtected())
  {
    Uprintf( "\nTape is protected (no ring, no write).\n");
    return;
  }  // if tape write protected

//  Now copy things.

  abort = 0;
  LastRecordCount = 0;	// how many records of the same size
  TapePosition = 0;	// block counter
  BytesCopied = 0;	// data counter
  fileCount = 0;	// file count
  status = TSTAT_NOERR;	// assume no tape errors yet

  while( true)
  {
  
    uint32_t
      header1,		// leading header
      header2;		// trailing header

    UINT 
      bcount;

    UINT
      bytesRead;	// how many bytes read
    
    fres = f_read( &tf, &header1, sizeof(header1), &bytesRead);
    if ((bytesRead == 0) && (fres == FR_OK))
      break;			// we hit eof
      
//	See if something's wrong      
      
    if ( (fres != FR_OK) || (bytesRead != sizeof(header1)) )
    {
      Uprintf( "File read error--aborted\n");
      break;
    } // read error
    
//	Check out the header--if zero, it's a file mark. If it's EOM, we're done.
//	Otherwise, the lower 24 bits has the record length; make sure that it's
//	less than our buffer size.  If it checks out, read up the record, read
//	the trailer and compare it to the header--they should be the same.

    if ( (abort = CheckForEscape()) )  // Check for ESC key
      break;
    if ( header1 == TAP_EOM)
      break;			// finished
    TapePosition++;			// bump block number
    
    if ( header1 != 0)
    { // if not tapemark
      
      bool corrupt;
      
      corrupt = true;			// assume things will go south
      
      bcount = header1 & TAP_LENGTH_MASK;
      if (bcount < TAPE_BUFFER_SIZE)
      { // block size is in range
        fres = f_read( &tf, TapeBuffer, bcount, &bytesRead);
        if ((bytesRead == bcount) && (fres == FR_OK))
        { // file read is okay
          fres = f_read( &tf, &header2, sizeof(header2), &bytesRead);
          if ((bytesRead == sizeof(header2)) && (fres == FR_OK))
          {
            if ( header1 == header2)
            {
              corrupt = false;		// say the record looks good
              status = TapeWrite( TapeBuffer, bcount);
            } // header and trailer check out
          } // read of trailer looks good
        } // if read of data looks good
      } // block size is in range      
      if ( corrupt)
      {
        Uprintf( "\nImage file corrupt at block %d.\n", TapePosition);
        break;                               
      }  // show corruption
    } // appears to be a data block
    else 
    { // we have a tapemark 
      bcount = 0;
      status = TapeWrite( TapeBuffer, 0);
      fileCount++;
    } // write a tapemark
    AddRecordCount( bcount);		// sum it up
  } // while  we have data
  
  if ( status != TSTAT_NOERR)
  { // diagnose any media errors
    Uprintf( "Tape error - %s\n", TranslateError( status));  
  } // if media errors
  
// close up and give a summary.

  f_close(&tf);

  if ( abort)
  {
    Uprintf( "Operation terminated by operator.\n");
  }

  FlushRecordCount();
  Uprintf( "\nFile %s written to tape.\n", args[0]);
  Uprintf( "\n%d blocks read.\n", TapePosition);
  Uprintf( "%d files; %d bytes copied.\n", fileCount, BytesCopied);

  if ( !noRewind)
  {
    Uprintf( "Rewinding...\n");
    TapeRewind();
    TapePosition = 0;		// we rewound the tape
  } // rewind if requested

  return;
} // CmdWriteImage

//  CmdSet6250 - Set 6250 GCR density.
//  ----------------------------------
//

//	Tape must be at BOT.
//

void CmdSet6250( char *args[])
{

  (void) args;

  if ((TapeStatus() & PS1_ILDP) == 0)
    Uprintf( "Error - Tape must be at BOT for this command.\n");
  else
    Set6250();
  return;
} // CmdSet6250

//  CmdSet1600 - Set 1600 PE density.
//  ----------------------------------
//
//	Tape must be at BOT.
//

void CmdSet1600( char *args[])
{

  (void) args;
  
  if ((TapeStatus() & PS1_ILDP) == 0)
    Uprintf( "Error - Tape must be at BOT for this command.\n");
  else
    Set1600();
  return;
} // CmdSet1600


//*	Local utility routines.
//	=======================

//*	GetComment - Get a one line comment and create a file with it.
//
//	We append a ".txt" to the base file name and put the
//	comment there if desired.
//

static void GetComment( char *Filename)
{

  char
    *noteFile,		// our new file name
    *inBuffer;		// where the line input goes

  FRESULT
    fres;               // file result codes

  FIL 
    tf;                 // our file structure

  UINT
    wc;			// write count

    
  noteFile = (char *)TapeBuffer;
  strcpy(noteFile, Filename);
  strcat(noteFile, ".txt");
  inBuffer = strlen( noteFile)+noteFile+1;	
  
  Uprintf( "\nEnter a comment for this tape, or <Enter> for none:\n");
  Ugets( inBuffer, 132);		// long enough
  if ( *inBuffer == 0)
    return;				// no comment  
  if ( (fres = f_open( &tf, noteFile, FA_CREATE_ALWAYS | FA_WRITE)) != FR_OK)
  {
    Uprintf( "\nError in creating file. Error = %d\n", fres);
    return;
  } // if open error         
  
//	Append a cr-lf to the comment.

  strcat( inBuffer,"\r\n");
  f_write( &tf, inBuffer, strlen( inBuffer), &wc);          
  f_close( &tf);     
  Uprintf( "\n");		// done!
  return;
} // GetComment


//*  	Check for Escape hit.
//  	---------------------
//
//  Returns 0 if no keypress, otherwise returns TRUE if ESC hit after
//  issuing a message.
//

static bool CheckForEscape( void)
{

  if ( Ucharavail())
  {
    if ( Ugetchar() == '\e')
    {
      Uprintf( "ESC pressed--aborting\n");
      return true;
    } // if it was an ESC
  }     // if a key was hit    
  return false;
} //  CheckForEscape

//  AddRecordCount - Accumulate file size and block size stats.
//  ------------------------------------------------------------
//
//    When the block size changes, calls FlushRecordCount.
//
//  Uses - 
//
//  LastRecordLength,                   // length of last record
//  LastRecordCount,                    // last record count
//  RecordLength,                       // length of current record
//  TapePosition,                       // what record
//  BytesCopied;                        // number of bytes copies   

static void AddRecordCount( uint32_t RecordLength)
{

  BytesCopied += RecordLength;
  if (LastRecordCount)
  {
    if (LastRecordLength != RecordLength)
    {
      FlushRecordCount();
      LastRecordLength = RecordLength;
    }
    LastRecordCount++;    
    if ( !RecordLength)
       Uprintf( "Filenark hit at %d\n", TapePosition); 
  }  // if we've seen records
  else
  {
    LastRecordLength = RecordLength;
    LastRecordCount = 1;          // starting a new total
  }
  return;
} // AddRecordCount

//  FlushRecordCount - Print possible summary of records encountered.
//  -----------------------------------------------------------------
//
//    Summarizes and resets the counter.
//

static void FlushRecordCount( void)
{

  if ( (LastRecordCount != 0)  && (LastRecordLength != 0))
  { // got some; report on it

    Uprintf( "%d x %d bytes\n", LastRecordCount, LastRecordLength);
    LastRecordCount = 0;
    LastRecordLength = 0xffff;          // clear the counters
  }
  return;
} // FlushRecordCount
