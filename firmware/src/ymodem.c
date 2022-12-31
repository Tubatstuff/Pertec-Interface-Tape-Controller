//*	Quick and Dirty Ymodem-batch code.
//	----------------------------------
//
//	For interface details, see Chuck Forsberg's document on the web
//	entitled "XMODEM/YMODEM Protocol Reference".   Some shortcuts
//	have been taken--CRC16 is generated on sending, but not checked
//	on receiving.  There is little transfer retry cdoe here, as it's
//	expected that a hard-wired connection to the host will be used.
//

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "license.h"

#include "filedef.h"
#include "comm.h"
#include "globals.h"
#include "usbserial.h"
#include "dbserial.h"

#include "crc16.h"
#include "ymodem.h"

//	ASCII character constants.

#define ASCII_SOH 0x01
#define ASCII_STX 0x02
#define ASCII_ACK 0x06
#define ASCII_NAK 0x15
#define ASCII_EOT 0x04
#define ASCII_CAN 0x18
#define ASCII_C   0x43

//	Timeout in milliseconds for a response.

#define RECEIVE_TIMEOUT 16000	

//	Timeout for ymodem response

#define POLL_TIMEOUT 5000
#define POLL_RETRIES 5

//	Data payload sizes.

#define LARGE_BLOCK 1024
#define SMALL_BLOCK 128
#define MAX_PACKET_SIZE LARGE_BLOCK	// size of the largest packet

//	How much space allocated for the file list?

#define NAME_BUF_SIZE (TAPE_BUFFER_SIZE - MAX_PACKET_SIZE)

//	Prototypes.

static int BuildFileList( char *Pattern, uint8_t *RetBuf, int RetSize);
static int SendPacket( uint8_t BlockNo, bool BigBlock, uint8_t *Payload);
static int GetByte( uint32_t TimeOut);
static int WaitChar( uint32_t TimeOut);
static void PutBlock( uint8_t *What, int Count);
static void PutChar( uint8_t What);
static char *IntToString (uint32_t What);

//*     SendYmodem - Routine to send a file via Ymodem.
//      ------------------------------------------------
//
//	Just takes the file name.  Uses FATFS to mount and read the file.
//	Returns an XERR_ code (see definitions in ymodem.h).  
//
        
XERR_CODE SendYmodem( char *FileSpec)
{

 FRESULT
    fres;               // file result codes
  FIL 
    fHandle;            // our file structure
  FSIZE_t 
    fLength;            // file length
    
  uint16_t
    blockNo;            // block number
  UINT
    bytesRead;          // how many bytes read

  char
    *fileName;		// the file we're working on

  int 
    ch; 
    
  uint8_t
    *buffer = TapeBuffer;     	// data buffer
  uint8_t
     *nameBuf = TapeBuffer+MAX_PACKET_SIZE;
  
//	Build list of files to transfer.  If none, exit.     

  if ( BuildFileList( FileSpec, nameBuf, NAME_BUF_SIZE) == 0)
    return XERR_NO_FILE;		// say file not found  
  
  fileName = (char *) nameBuf;		// first name

  while (true)
  {  // transfer a file at a time.

    if ( (fres = f_open( &fHandle, fileName, FA_READ) != FR_OK)) 
      return XERR_NO_FILE;		// something is really wrong!
  
    fLength = f_size( &fHandle);          // get length

//  Wait for a 'C' from the recevier.  If nothing, exit.

    while (true)
    {
      if ( (ch = GetByte(RECEIVE_TIMEOUT)) < 0)
        return XERR_ABORT;
      else
      {
        if ( ch == 'C')
          break;			// we got a go-ahead; discard others
      }
    } // wait for receiver

//  We have a C from the receiver, so construct a 128-byte packet.  Since we 
//  allocate 128 bytes total and our file length can be 10 characters, the
//  file name can't be more than 100 or so characters long.

    memset( buffer, 0, SMALL_BLOCK);		// clear garbage out
    strncpy( (char *) buffer, fileName, 100);	// copy in the file name

//	Add the file length after a null.  
  
    strcpy((char *) buffer+strlen( (char *) buffer)+1, IntToString( fLength));	  

//	This is block 0, so we send it.

    SendPacket( 0, false, buffer);		

//	The receiver will acknowledge with an ACK, then a C.

    ch = GetByte( RECEIVE_TIMEOUT);

    if (ch < 0)
      return XERR_TIMEOUT;	// no good
    if ( ch != ASCII_ACK)  
      return XERR_ABORT;  	// didn't get what we wanted
    
//	Now, we get in a "C" loop as we send packets.

    blockNo = 1;			// we start here
    while ( true)
    {
      ch = GetByte( RECEIVE_TIMEOUT);

      if ( ch < 0)
        return XERR_TIMEOUT;		// dead receiver
      if ( ch != ASCII_ACK)
      {  // should be an ACK, but could be a "C" at the start
        if ( ch != 'C')
          return XERR_ABORT;		// got the wrong response
      }
      bytesRead = 0;
      fres = f_read( &fHandle, buffer, LARGE_BLOCK, &bytesRead);
      if ( bytesRead == 0)
        break;				// call it eof
      if ( bytesRead < LARGE_BLOCK)
         memset( buffer+bytesRead, 0, LARGE_BLOCK-bytesRead); // fill with zero

//	okay, send the packet.     
       
      SendPacket( blockNo, true, buffer);
      blockNo++;                  // advance
    } // keep going
  
//	At end of file here.   Send some EOTs.

    PutChar(ASCII_EOT);				// signal EOF
  
//	We'll get a NAK.

    ch = GetByte( RECEIVE_TIMEOUT);		// we expect NAK
    PutChar( ASCII_EOT);
    ch = GetByte( RECEIVE_TIMEOUT);		// shouild be an ACK
    ch = GetByte( RECEIVE_TIMEOUT);
    if ( ch != ASCII_C)
    {
      break;
    }
    fileName = fileName + strlen(fileName)+1;	// to next file 
    if ( !*fileName)				// if end of list
      break;
  } // while sending files

  memset( buffer, 0, LARGE_BLOCK);		// clear the buffer
  SendPacket( 0, false, buffer);		// send a null filename
  f_close( &fHandle);
  PutChar (ASCII_CAN);
  PutChar (ASCII_CAN);
  return XERR_SUCCESS;
} // YmodemSend

//*    	SendPacket--Send Ymodem Payload.
//	--------------------------------
//
//	Sends a packet to host.  Depending on file size, a mixture of
//	1024 and 128 byte packets may be sent.
//

static int SendPacket( uint8_t BlockNo, bool BigBlock, uint8_t *Payload)
{

  int 
    payloadSize;		// how many bytes to send

  uint16_t
    crcWord;  			// CRC

  uint8_t
    cBuf[3];			// comm buffer--scratch

//  Determine payload and CRC;

  payloadSize = BigBlock ? LARGE_BLOCK : SMALL_BLOCK;
  crcWord = CRC16( Payload, payloadSize);
  
// If we're doing large packets, issue a SOH; otherwise, issue an STX;

  cBuf[0] = BigBlock ? ASCII_STX : ASCII_SOH;
  cBuf[1] = BlockNo;
  cBuf[2] = ~BlockNo;		// add block number and complement
  PutBlock( cBuf, 3);		// 3 header bytes

//  while (payloadSize--)
//  {
//    PutByte( *Payload++);  
//  } // output the payload

  PutBlock( Payload, payloadSize);
  
  cBuf[0] = (uint8_t) (crcWord >> 8);	// high byte of CRC
  cBuf[1] = (uint8_t) (crcWord & 0xff);	// low byte of CRC
  PutBlock( cBuf, 2);		// CRC trailer
  return 0;
} // SendPacket

//*	ReceiveYmodem - Get a remote file.
//	----------------------------------
//
//	Creates or overwrites a file whose name is sent in the zero block.
//

XERR_CODE ReceiveYmodem( void)
{

  FRESULT
    fres;               	// file result codes

  FIL 
    tf;                 	// our file structure

  UINT
    wc;				// word count for file write

  char
    filename[64];		// seems like a reasonable amount
    
  uint8_t
    *buffer = TapeBuffer,	// data buffer - uses unused tape buffer
    blockno[2],			// block and complement block
    crcval[2];			// crc bytes

  uint32_t
    bytesWritten,		// how many bytes written
    fileSize;			// file size as read in header

  int
    nextBlock,    		// next anticipated block
    i,				// general index
    rerror,			// error flag
    currChar,			// current character
    dPos,			// position in buffer
    blockLength;		// block length

  
  typedef enum   
  {
    GET_TYPE,
    GET_FIRST_BLOCK,
    GET_SECOND_BLOCK,
    GET_DATA,
    GET_CRC1,
    GET_CRC2,
    GET_EOT1,
    GET_EOT2
  } RECEIVE_STATE;

  RECEIVE_STATE
    state;


  rerror = XERR_SUCCESS;
  bytesWritten = 0;			// initialize counters
  nextBlock = 0;			// keep count
  
//	First, flush any characters lying around.

  while( Ucharavail() )
    Ugetchar();
    
  for ( i = POLL_RETRIES; i; i--)
  {
    PutChar( ASCII_C);		// put out a C

    if ( WaitChar( POLL_TIMEOUT) > 0)
      break;  
  } // wait for a response
  if ( i == 0)
    return XERR_TIMEOUT;    
  state = GET_TYPE;		// start here.

//  	Character ready, join main loop

  while( true)
  {
      
//	Start of next (or first block here.

    rerror = XERR_SUCCESS;			// assume no error

    currChar =  GetByte( RECEIVE_TIMEOUT);
    if ( currChar < 0)
    {
      rerror =  XERR_TIMEOUT;			// timed out, go away
      break;
     } 

//	Figure out what to do next.

    switch ( state)
    {
 
      case GET_TYPE:				// beginning of record
        state = GET_FIRST_BLOCK;
        rerror = XERR_SUCCESS;			// assume success
        if ( currChar == ASCII_STX)
          blockLength = LARGE_BLOCK;			// long block
        else if ( currChar == ASCII_SOH)
          blockLength = SMALL_BLOCK;			// short block
        else if ( currChar == ASCII_EOT)
        {
          if ( bytesWritten)
            f_close( &tf);
          PutChar( ASCII_NAK);			// ready for next file.
          state = GET_EOT2;		// here we go...
        }  // got an EOT
        break;

//	Fetch the block number and its complement.

      case GET_FIRST_BLOCK:
        blockno[0] = (uint8_t) currChar;	// pick up positive block
        state = GET_SECOND_BLOCK;
        break;
 
      case GET_SECOND_BLOCK:
        blockno[1] = ~(uint8_t) currChar;	// get complemet, complement it
        if ( blockno[0] != blockno[1])
        {
          rerror = XERR_CORRUPT;		// bad block
          break;
        }
        dPos  = 0;				// set the counter
        state = GET_DATA;
        break;

//	Read the block in.

      case GET_DATA:				// fill the block buffer
        buffer[ dPos++] = (uint8_t) currChar;	// store the character	
        if ( dPos >= blockLength)
          state = GET_CRC1;			// got our buffer, CRC1 next
        break;

//	Get 2 bytes of CRC

      case GET_CRC1:
        crcval[0] = (uint8_t) currChar;		// first byte of CRC
        state = GET_CRC2;
        break;
      
      case GET_CRC2:
        crcval[1] = (uint8_t) currChar;		// second byte of CRC
        (void) crcval;				// gets rid of warning

//	Block 0 is a special case.  It has the file name and length.

        if ( nextBlock == 0)
        { // this is the file name, etc. block
          bytesWritten = 0;		// say no bytes written
          if ( buffer[0] == 0)
          { // if null record
            PutChar( ASCII_ACK);	// we're done
            rerror = XERR_ENDFILE;	// say we're done
            break;			// no more             
          }
          strcpy(filename, (char *)buffer);	// get the file name
          fileSize = strtoul( (char *) buffer+strlen(filename)+1, NULL, 0);
          if ( fileSize == 0)
            fileSize = 0x40000000;	// upper limit of 1GB

//	Now create a file.

         if ( (fres = f_open( &tf, filename, FA_CREATE_ALWAYS | FA_WRITE)) 
               != FR_OK)
         {
           rerror = XERR_NO_FILE;
           break;				// couldn't create 
          } // if open error   

        } // if block zero
        else
        { // need to write this one
        
          int thisPass = fileSize > (uint32_t)blockLength ? blockLength : (int) fileSize;
        
//	See how much we need to write.
          
          f_write( &tf, buffer, thisPass, &wc);  
          fileSize -= thisPass;
          bytesWritten += thisPass;	// bump the write
        } // we're dumping a block

//	Respond with an ACK or ACK-C if block 0.

        PutChar( ASCII_ACK);
        rerror = XERR_SUCCESS;		// still good
        if ( nextBlock == 0)
          PutChar( ASCII_C);
        state = GET_TYPE;
        nextBlock++;
        break;

      case GET_EOT2:
        if ( currChar != ASCII_EOT)
        {
          rerror = XERR_ENDFILE;	// say we have a good one
          break;			// didn't get what expected
        }
        
        PutChar( ASCII_ACK);		// respond with NAK
        PutChar( ASCII_C);
        nextBlock = 0;			// get ready for next file
        rerror = XERR_SUCCESS;
        state = GET_TYPE;      		// gets us out of the loop
        break;
              
      default:
          break;
    } // switch

//	Loop exits on errors.  XERR_ENDFILE is not really an error.

    if ( rerror != XERR_SUCCESS)
    {
      PutChar( ASCII_CAN);		// abort the transfer
      break;
    }
  }; // while

  if  ( rerror == XERR_ENDFILE)
    rerror = XERR_SUCCESS;		// if we came to the end gracefully

  if ( bytesWritten)
  {
    f_close( &tf);
  }
  return rerror;
} // ReceiveYmodem

//	BuildFileList - Construct list of files to transmit.
//	----------------------------------------------------
//
//	Creates a list of null-terminated file names according to
//	the search criterion in "Pattern".   Returns the number of
//	matches found, as well as filling the buffer.  Last name
//	is terminated by a double-null.
//

static int BuildFileList( char *Pattern, uint8_t *RetBuf, int RetSize)
{

  int 
    fileCount = 0;
  uint8_t
    *listPos = RetBuf;
  FRESULT
    fres;
  FILINFO
    finfo;
  DIR
    dirObj;

  if (*Pattern)
  { // skip if null pattern
    fres = f_findfirst( &dirObj, &finfo, CurrentPath, Pattern); // start search
    if ( (fres != FR_OK) || (*finfo.fname == 0))
    { // if the first find fails
      Uprintf( "No files found.\n");
      *RetBuf = 0;	
      return 0;				// signal no files
    } // first find
    
    while( (fres == FR_OK) && finfo.fname[0])
    { // all files   
      if ( !(finfo.fattrib & AM_DIR))
      { // exclude directories
        if ( (int) (strlen(finfo.fname)+(listPos-RetBuf)) >= RetSize)
          break;			// no more room!
        strcpy((char *)listPos, finfo.fname);	// copy the name
        fileCount++;      
        listPos += (strlen( finfo.fname) +1);    
      } // if not a directory
      fres = f_findnext( &dirObj, &finfo);	// next item
    } // while
    f_closedir( &dirObj);	// close the search
  } // if there's something to find
  *listPos = 0;
  return fileCount;
}  // Buildfilelist


//  	GetByte - Timed get byte.
//  	-------------------------
//
//      Argument is timeout in milliseconds.
//      Return is character or -1 if timed out.
//

static int GetByte( uint32_t TimeOut)
{

  uint32_t 
    start;

  start = Milliseconds;            // get starting time
  do
  {
    if ( USCharReady())
    { // if input waiting
      return USGetchar();        // return input read
    }
  }
  while ( (Milliseconds - start) <= (uint32_t) TimeOut);
  return -1;                    // say we're timed out  
} // GetByte

//*  	WaitChar - Timed wait for input.
//  	---------------------------------
//
//      Argument is timeout in milliseconds.
//      Return is 0 if character ready or -1 if timed out.
//

static int WaitChar( uint32_t TimeOut)
{

  uint32_t 
    start;

  start = Milliseconds;            // get starting time
  do
  {
    if ( USCharReady())
    { // if input waiting
      return 1;        // Say we've got one
    }
  }
  while ( (Milliseconds - start) <= (uint32_t) TimeOut);
  return -1;                    // say we're timed out  
} // WaitChar

//*	PutBlock - Output Data Block.
//	-----------------------------
//
//	Arguments are pointer and count.
//

static void PutBlock( uint8_t *What, int Count)
{
  USWriteBlock( What, Count);
  return;
} //  PutBlock

//*	PutChar - write a single uncooked character.
//	--------------------------------------------
//
//	No filtering or additions.
//


static void PutChar( uint8_t What)
{
  USWritechar( What);
  return;
} // PutChar

//*     IntoToString -  Convert 32-bit unsigned integer to decimal string.
//      ------------------------------------------------------------------
//
//      Maximum number of digits is 10.  Note that the return
//      value is static and so will be overwritten with every call.
//

static char *IntToString (uint32_t What)
{

  static char 
    retval[11];         // return string
    
  int 
    pos = 10;           // digit count

  retval[10] = '\0';    // be sure of a terminator

  if (What == 0)
  {
    pos--;
    retval[pos] = '0';          // just return a single zero
  } else
  {

//      Use the Chinese remainder theorem.

    while ((What != 0) && (pos > 0))
    {
      retval[--pos] = (What % 10) + '0';  // stash reminder
      What /= 10;                         // divide by 10 for next
    } // while
  } // if not zero
  return retval+pos;                            // return value
} // IntToString 
