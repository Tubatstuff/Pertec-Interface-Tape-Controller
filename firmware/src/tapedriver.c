//*	Pertec interface low-level routines.
//	------------------------------------

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

#include "license.h"

// Local definitions.

#include "comm.h"
#include "gpiodef.h"
#include "miscsubs.h"
#include "globals.h"
#include "tapedriver.h"
#include "pertbits.h"
#include "filedef.h"

//  Static variables used here.

uint16_t
  LastCommand;			// last written contents of command register.

//  Prototypes.

static unsigned int TapeMotion( uint16_t Command);
static void AckTapeTransfer( void);

//	TapeStatus - Read 16 bit status.
//	--------------------------------
//
//	Status 1 in bits 15-8
//	Status 0 in bits 7-0
//
//	Returns the positive tape status;
//

uint16_t TapeStatus( void)
{

  uint16_t res;		// result
  uint8_t ss0, ss1;

//  Do things twice here to delay a bit.

  gpio_clear( PCTRL_GPIO, PCTRL_SSEL);
  gpio_clear( PCTRL_GPIO, PCTRL_SSEL);
  ss0 = (gpio_port_read( PSTAT_GPIO) >> 8);
  ss0 = (gpio_port_read( PSTAT_GPIO) >> 8);
  gpio_set( PCTRL_GPIO, PCTRL_SSEL); 
  gpio_set( PCTRL_GPIO, PCTRL_SSEL); 
  ss1 = (gpio_port_read( PSTAT_GPIO) >> 8);
  ss1 = (gpio_port_read( PSTAT_GPIO) >> 8);

  res = ss0 | (ss1 << 8);
   
  return res ^ 0xffff;
} // TapeStatus

//	Initialzie tape interface.
//	---------------------------
//

void TapeInit( void)
{

// issue a transfer acknowledge

  gpio_clear( PCTRL_GPIO, PCTRL_TACK);
  Delay(1);
  gpio_set( PCTRL_GPIO, PCTRL_TACK);            // ack the transfer
  TapeAddress = 0;				// default address
  StopTapemarks = 2;				// default tape mark stop
  StopAfterError = false;			// if stop after error
  
//  Set the command bits high.

  gpio_set( PCMD_GPIO, PCMD_BIT);	// set all bits to one
  gpio_clear( PCTRL_GPIO, PCTRL_CSEL0);
  Delay(1);
  gpio_set( PCTRL_GPIO, PCTRL_CSEL0);	// latch it in
  Delay(1);  
  gpio_clear( PCTRL_GPIO, PCTRL_CSEL1);
  Delay(1);
  gpio_set( PCTRL_GPIO, PCTRL_CSEL1);	// latch it in
  LastCommand = 0;			// remember this

//  Now enable the command output.

  gpio_clear( PCTRL_GPIO, PCTRL_ENA);	// and there we go...

} // TapeInit

//*	Rewind and Unload - No data phase.
//	==================================

//	TapeRewind - Rewind tape.
//	------------------------
//

unsigned int TapeRewind( void)
{

  uint16_t
    stat;

  
  if (!IsTapeOnline())
    return TSTAT_OFFLINE;		// not online

  stat = TapeStatus();		// See what gives
  if ( stat & PS1_ILDP)
    return TSTAT_NOERR;			// already at loadpoint

//	Issue rewind command.

  IssueTapeCommand( PC_IREW);		// assert rewind
  IssueTapeCommand( 0);			// null commnad
  
  while( TapeStatus() & PS1_IRWD);	// let tape rewind
  return TSTAT_NOERR;
} // TapeRewind

//*	TapeUnload - Unload tape.
//	--------------------------
//

unsigned int TapeUnload( void)
{
  
  if (!IsTapeOnline())
    return TSTAT_OFFLINE;		// not online

//	Issue rewind command.

  IssueTapeCommand( PC_IRWU);		// assert rewind
  IssueTapeCommand( 0);			// null commnad
  
  return TSTAT_NOERR;
} // TapeUnload

//**	Motion functions--no data transfer.
//	===================================


static unsigned int TapeMotion( uint16_t Command)
{

  unsigned int
    retStatus;
  uint16_t  
    status;

  if ( !IsTapeOnline())
    return TSTAT_OFFLINE;	// return if offline

  IssueTapeCommand( PC_IGO | Command);	// assert go+command
  Delay(2);
  IssueTapeCommand( Command);		// release it

//  Wait for IFBY to go active, then active IDBY.

  while( true)
  {
    status = TapeStatus();
    if ( status & PS1_IFBY)
      break;
  }  // wait for formatter busy

//	Okay, we have the formatter acknowledging the command, now wait
//	for the data phase.  If formatter busy drope while waiting, we
//	bombed.
  
  do
  {
    status = TapeStatus();
    if ( !(status & PS1_IFBY))
    {
      return TSTAT_OFFLINE;		// tape dropped ready
    }

  } while( (status & PS0_IDBY) == 0);	// wait for data phase

//	Kill time while data busy is set.  

  do
  {
    status = TapeStatus();
  } while ( status & PS0_IDBY);

//	Wait for formatter busy to drop.

  do
  {
    status = TapeStatus();
  } while ( status & PS1_IFBY);

  retStatus = TSTAT_NOERR;

  if ( (status & PS0_IFMK))
    retStatus |= TSTAT_TAPEMARK;		// say we have a tapemark

  if ( (status & PS0_IHER))
    retStatus |= TSTAT_HARDERR;		// signal hard error
  
  if ( status & PS1_EOT)
    retStatus |= TSTAT_EOT;		// say end of tape
    
  return retStatus;			// all done  
} // TapeMotion

//*	SkipBlock - Skip one block forward or backward.
//	-----------------------------------------------
//
//	Sign of argument determines direction.
//
//	Returns the usual TSTAT status
//

unsigned int SkipBlock( int Dir)
{

  unsigned int
    status;
  uint16_t  			// tape status
    tCommand;			// command we're sending

  tCommand = PC_IERASE;
  if ( Dir < 0)
    tCommand |= PC_IREV;
    
  status = TapeMotion( tCommand);	// issue it
  return status;			// all done--successful
} // SkipBlock


//*	SpaceFile - Space filemarks forware or backward.
//	------------------------------------------------
//
//	The argument is + for forward, - for backward.
//

unsigned int SpaceFile( int Dir)
{

  unsigned int
    status;

  uint16_t
    tCommand;			// command we're sending

  tCommand = PC_IWFM;		// space with data
  if ( Dir < 0)
    tCommand |= PC_IREV;
    
  status = TapeMotion( tCommand);	// issue it
  return status;			// all done
} // SpaceFile

//*	Read/Write Functions.
//	=====================


//*	TapeRead - Read a tape block.
//	-----------------------------
//
//	Reads (forward) a block from a tape into a buffer.
//	Returns the size of the block read and the status.
//
//	Status can be a combination of any of these:
//		TSTAT_OFFLINE 	- Drive is offline
//		TSTAT_HARDERR 	- Formatter detected a hard error
//		TSTAT_CORRERR 	- Formatter corrected an error
//		TSTAT_TAPEMARK 	- Record is a tape mark
//		TSTAT_EOT	- End of tape encountered
//		TSTAT_BLANK	- Blank tape read
//		TSTAT_LENGTH	- Block longer than buffer
//		TSTAT_NOERR	- No error detected
//
//	Some conditions such as read past EOT, offline, blank or tapemark
//	return a data count of zero.
//

unsigned int TapeRead( uint8_t *Buf, int Buflen, int *BytesRead)
{

  unsigned int
    retStatus;			// cumulative return status
  uint8_t 
    *bptr;			// buffer pointer
  uint16_t 
    status;			// 16 bit status registers
  int
    bcount;			// current byte count
  uint8_t
    stat;			// SR0 value

  *BytesRead = 0;		// say nothing yet
  retStatus = 0;		// clear return status  

  if ( !IsTapeOnline())
    return TSTAT_OFFLINE;	// return if offline
    
  bcount = Buflen;		// byte count
  bptr = Buf;			// where we store things

  G_INPUT( PDATA);		// enforce input mode on data
  gpio_clear( PCTRL_GPIO, PCTRL_DDIR);	// set direction

  AckTapeTransfer();		// clear transfer flags
  IssueTapeCommand( PC_IGO);	// assert go
  Delay(2);
  IssueTapeCommand( 0);		// release it

//  Wait for IFBY to go active, then active IDBY.

  while( true)
  {
    status = TapeStatus();
    if ( status & PS1_IFBY)
      break;
  }  // wait for formatter busy

//  Uprintf( "Start status = %04x\n", status);

//	Okay, we have the formatter acknowledging the command, now wait
//	for the data phase.  If formatter busy drope while waiting, we
//	bombed.
  
  do
  {
    status = TapeStatus();
    if ( !(status & PS1_IFBY))
    {
      return TSTAT_OFFLINE;		// tape dropped ready
    }

  } while( (status & PS0_IDBY) == 0);	// wait for data phase

//	During the duration of the read, we use status register 0.
//	Note that direct reading of the status is negative-true.
//	Status reg 1 bits are checked at the conclusion.

  gpio_clear( PCTRL_GPIO, PCTRL_SSEL);	// start with the first status reg

//	Read loop.

  do
  { // data transfer loop

    stat = gpio_port_read( PSTAT_GPIO) >> 8;	// normalize status
    
    if ( (stat & PS0_RDAVAIL) == 0)		// note negative logic
    { // read data
      gpio_clear( PCTRL_GPIO, PCTRL_TACK);	// start transfer ACK
      *bptr++ = ~gpio_port_read( PDATA_GPIO);	// get a byte
      gpio_set( PCTRL_GPIO, PCTRL_TACK);        // ack the transfer
      bcount--;
      continue;
    } // if we have a byte
    else if  (stat & PS0_IDBY)
     break;				// data busy drops? 
  } while (bcount);			// while

//	If bcount is zero, then the block was longer than our buffer.

  if ( bcount == 0)
    retStatus = TSTAT_LENGTH;		// say we have an overrun

// 	Now, figure the amount actually read.

  bcount = Buflen - bcount;		

  if ( (stat & PS0_IFMK) == 0)
  {
    retStatus |= TSTAT_TAPEMARK;	// say we have a tapemark
    bcount = 0;				// say nothing transferred
  }

  if ( (stat & PS0_IHER) == 0)
    retStatus |= TSTAT_HARDERR;		// signal hard error
  if ( (stat & PS0_ICER) == 0)
    retStatus |= TSTAT_CORRERR;		// signal corrected error
  
// 	Look at full 16-bit status.

  status = TapeStatus();		// get all 16 bits of status
  if ( status & PS1_EOT)
    retStatus |= TSTAT_EOT;		// say end of tape
    
//	If we don't see any data, but haven't thrown an error, we call the
//	tape blank.

  if ( (bcount == 0) && (retStatus== 0))
    retStatus |= TSTAT_BLANK;		// say we have a blank tape

  *BytesRead = bcount;
  return retStatus;			// all done  
} // TapeRead

//*	TapeWrite - Write a tape block.
//	-------------------------------
//
//	Returns completion status.
//
//	Status can be a combination of any of these:
//		TSTAT_OFFLINE 	- Drive is offline
//		TSTAT_HARDERR 	- Formatter detected a hard error
//		TSTAT_EOT	- End of tape encountered
//		TSTAT_BLANK	- Blank tape read
//		TSTAT_NOERR	- No error detected
//
//	If Buflen == 0, we write a tapemark.
//
//	Some implementation notes:
//	* The write-buffer-empty signal is generated on the *rising*
//	edge of the write-strobe pulse.  In other words, it indicates
//	that the previous character has been accepted by the drive.
//	* The "Last Word" signal must be asserted *prior* to the 
//	write-strobe pulse of last character.  Upon receiving the
//	last character, the "data busy" condition will terminate.
//
	


unsigned int TapeWrite( uint8_t *Buf, int Buflen)
{

  unsigned int
    retStatus;                  // cumulative return status
  uint8_t 
    *bptr;                      // buffer pointer
  uint16_t 
    driveCmd,			// drive command
    status;                     // 16 bit status registers
  int
    bcount;                     // current byte count
  uint8_t
    stat;                       // SR0 value

//	First off, check to make sure the drive is online 
//	and not write-protected.

  if ( !IsTapeOnline())
    return TSTAT_OFFLINE;       // return if offline
  if ( IsTapeProtected())
     return TSTAT_PROTECT;	// can't write to a write-protected tape
     
  retStatus = TSTAT_NOERR;	// assume no status
  bptr = Buf;			// buffer
  bcount = Buflen;		// set up some locals
  
  G_OUTPUT( PDATA);              // enforce output mode on data
  gpio_set( PCTRL_GPIO, PCTRL_DDIR | PCTRL_LBUF);  // set direction+strobe

  if (bcount == 0)
    driveCmd = PC_IWRT + PC_IWFM;	// write file mark
  else
    driveCmd = PC_IWRT;			// write data  
  
  IssueTapeCommand( PC_IGO + driveCmd);	
  Delay(2);
  IssueTapeCommand( driveCmd);	// issue command

//	Formatter will go busy.

  do
  {
    status = TapeStatus();		// grab current status
  } while( !(status & PS1_IFBY));   	// wait for formatter finished

//  If writing a filemark, there's no data phase.
//  If writing data, prime the buffer.

  if ( bcount != 0)
  {
    gpio_clear( PCTRL_GPIO, PCTRL_TACK | PCTRL_LBUF);      // start transfer ACK
    gpio_port_write( PDATA_GPIO, ~*bptr++);   // get a byte
    gpio_set( PCTRL_GPIO, PCTRL_TACK | PCTRL_LBUF);        // ack the transfer
    bcount--;

//	Okay, at this point we transfer the rest of the buffer

    do
    {
      status = TapeStatus();
    } while( (status & PS0_IDBY) == 0);   // wait for data phase

//      During the duration of the write, we use status register 0.
//      Note that direct reading of the status is negative-true.
//      Status reg 1 bits are checked at the conclusion.

    gpio_clear( PCTRL_GPIO, PCTRL_SSEL);  // start with the first status reg

//	Perform the write transfer.  

    while (bcount)
    { // data transfer loop

      stat = gpio_port_read( PSTAT_GPIO) >> 8;	// normalize status
    
      if ( (stat & PS0_WREMPTY) == 0)		// note negative logic
      { // need to refill buffer

 //	Load next byte and ack the empty buffer.

        gpio_clear( PCTRL_GPIO, PCTRL_TACK | PCTRL_LBUF);	// start transfer ACK
        gpio_port_write( PDATA_GPIO, ~*bptr++);	// load next byte
        gpio_set( PCTRL_GPIO, PCTRL_TACK | PCTRL_LBUF);  // ack the transfer

//	If we're at the second-to-last word, set "last word" flag.  

        if (bcount == 2)
        {
          gpio_set( PCMD_GPIO, PCMD_BIT);	// set all bits to one
          gpio_clear( PCMD_GPIO, PC_ILWD);       // assert last word
          gpio_clear( PCTRL_GPIO, PCTRL_CSEL1);  // latch it in
          gpio_set( PCTRL_GPIO, PCTRL_CSEL1);   // strobe to latch bits
        } // if last word
        bcount--;      
      } else
      {
        if( (stat & PS0_IDBY))		// if IDBY has dropped prematurely
          break;
      } // if not buffer empty
    };	// while data to transfer

//	Wait for IDBY to drop

    do
    {
      stat = gpio_port_read( PSTAT_GPIO) >> 8;	// normalize status
    } while (!(stat & PS0_IDBY));	// wait for IDBY to drop
  } // if transferring data  

//  De-assert commands and wait for "formatter busy" to drop

  IssueTapeCommand( 0);			// clear it

  do
  {
    status = TapeStatus();		// grab current status
  } while( (status & PS1_IFBY));   	// wait for formatter finished

  if ( status & PS0_IHER)
    retStatus |= TSTAT_HARDERR;		// signal hard error
  if ( status & PS0_ICER)
    retStatus |= TSTAT_CORRERR;		// signal corrected error
  
//  Check completion status.

  return retStatus;			// done.  
} // TapeWrite

//*	Status Testing Routines.
//	========================


//	IsTapeOnline - Test tape drive online.
//	--------------------------------------
//
//	Returns true if online, false otherwise.
//

bool IsTapeOnline( void)
{

  uint16_t stat;

  stat = TapeStatus();
  
  if ( stat & PS1_IRDY)
    return true;
  else
    return false;
} //  IsTapeOnline

//	IsTapeEOT - Test for End of Tape status.
//	----------------------------------------
//
//	Returns true if EOT status present.
//

bool IsTapeEOT( void)
{

  uint16_t stat;

  stat = TapeStatus();
  
  if ( stat & PS1_EOT)
    return true;
  else
    return false;
} // IsTapeEOT

//	IsTapeProtected - Test for write protect asserted.
//	--------------------------------------------------
//	
//	Returns true if write-protected.
//

bool IsTapeProtected( void)
{

  uint16_t stat;

  stat = TapeStatus();
  
  if ( stat & PS1_IFPT)
    return true;
  else
    return false;
} // IsTapeProtected

//*	Local utility routines.
//	=======================

//	IssueTapeCommand - Issue 16 bit tape command.
//	---------------------------------------------
//
//	Command 1 in bits 15-8
//	Command 0 in bits 7-0
//
//	Includes the tape address automatically.
//
//	Note that most signals on the control interface are edge-sensitive,
//	so that tristating (ENA bit) shouldn't harm a thing.
//

void IssueTapeCommand( uint16_t What)
{

  uint8_t
    cmd1,			// lower command bits
    cmd2;			// upper command bits

  What |= TapeAddress;		// insert address
  cmd1 = What & 0xff;		// get low byte
  cmd2 = What >> 8;		// get high byte

//	Start with the high-order 16 bits.  If it already matches with the
//	last one issued, skip it.

  if ( cmd2 != (uint8_t) (LastCommand >>8))
  { // need to set high order  

    gpio_set( PCTRL_GPIO, PCTRL_ENA);	// float the command register
    gpio_set( PCMD_GPIO, PCMD_BIT);	// set all bits to one
    if ( cmd2)
      gpio_clear( PCMD_GPIO, cmd2);	// assert the ones
    gpio_clear( PCTRL_GPIO, PCTRL_CSEL0);
    Delay(1);
    gpio_set( PCTRL_GPIO, PCTRL_CSEL0);	// latch it in
  } // high order bits

  if ( cmd1 != (uint8_t) LastCommand)
  { // need to set low order

//	cmd1 has the IGO status.
    
    gpio_set( PCMD_GPIO, PCMD_BIT);	// set all bits to one
    if ( cmd1)
      gpio_clear( PCMD_GPIO, cmd1);	// assert the ones
  
    gpio_clear( PCTRL_GPIO, PCTRL_CSEL1);
    Delay(1);				// let the latch settle in
    gpio_set( PCTRL_GPIO, PCTRL_CSEL1);	// strobe to latch bits
  } // low-order bits
  gpio_clear( PCTRL_GPIO, PCTRL_ENA);	// enable command register
  LastCommand = What;			// remember it
  return;
} // IssueTapeCommand

//	AckTapeTransfer - Reset the data latch status.
//	----------------------------------------------
//

static void AckTapeTransfer( void)
{

  gpio_clear( PCTRL_GPIO, PCTRL_TACK);
  gpio_set( PCTRL_GPIO, PCTRL_TACK);            // ack the transfer

} // AckTapeTransfer

//  SetTapeAddress - Set drive/formatter address.
//  ---------------------------------------------
//
//	3 bits - FAA - F =Formatter, AA = Drive address 
//

void SetTapeAddress( uint16_t What)
{  
  TapeAddress = 0;
  if (What & 1)
    TapeAddress = PC_ITAD0;
  if (What & 2)
    TapeAddress |= PC_ITAD1;
  if (What & 4)
    TapeAddress |= PC_IFAD;
  return;
} // SetTapeAddress


//  Set1600 - Set 1600 PE mode.
//  ---------------------------
//
//	Valid only at BOT for writing; not supported on all drives.
//

void Set1600( void)
{

  IssueTapeCommand( PC_IEDIT | PC_IWFM | PC_IERASE);
  Delay(4);
  IssueTapeCommand( 0);
  return;  
} // Set1600

//  Set6250 - Set 6250 GCR mode.
//  ---------------------------
//
//	Valid only at BOT for writing; not supported on all drives.
//

void Set6250( void)
{

  IssueTapeCommand( PC_IEDIT | PC_IWFM | PC_IERASE | PC_IREV);
  Delay(4);
  IssueTapeCommand( 0);
  return;  
} // Set6250
