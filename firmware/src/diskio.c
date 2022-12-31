/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2016        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "license.h"

#include "ff.h"
#include "diskio.h"	// FatFs lower layer API
#include "sdiosubs.h"	// SDIO routines
#include "rtcsubs.h"
#include "globals.h"
#include "comm.h"

// Definitions of physical drive number for each drive 

#define DEV_RAM		0	/* Example: Map Ramdisk to physical drive 0 */
#define DEV_MMC		1	/* Example: Map MMC/SD card to physical drive 1 */
#define DEV_USB		2	/* Example: Map USB MSD to physical drive 2 */

#define BLOCK_SIZE 512		// size of sector

//	We use DMA here, so we need to handle misaligned I/O.

static uint8_t  __attribute__ ((aligned(4)))
  DiskBuf[ BLOCK_SIZE];

#define IS_MISALIGNED(x) (((uint32_t)(x)) & 0x03)	// nonzero if not on 32-bit boundary

//* disk_status - Get Disk Status.
//  ------------------------------
//
//

DSTATUS disk_status (BYTE pdrv)
{

  (void) pdrv;

  return RES_OK;
}	// disk status

//* disk_initialize - Initialize a disk.
//  ------------------------------------
//

DSTATUS disk_initialize (BYTE pdrv)
{

  (void) pdrv;

  if ( SD_GetCardSize())
    return RES_OK;        // say the card's already initialized.


  if ( !SD_Init() )
    return RES_OK;
  else
    return STA_NOINIT;
} // disk_initialize

//* disk_read - Read sector(s)
//  --------------------------
//
//

DRESULT disk_read (BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{

  SD_ERROR sdstat;

  (void) pdrv;
  
//  Misaligned block reads are performed a sector at a time.
//
//  Aligned block reads are performed a block at a time.

  SD_WaitComplete();		// clear any pending writes

  if ( IS_MISALIGNED( buff))
  {
    while( count--)
    {
      if ( (sdstat = SD_ReadBlocks( DiskBuf, sector, 1)))
        break;
      SD_WaitComplete();
      memcpy( buff, DiskBuf, BLOCK_SIZE);
      sector++;
      buff += BLOCK_SIZE;
    } // do every sector
  } // handle problems
  else
  {  // aligned buffers, no problem
    sdstat = SD_ReadBlocks( buff, sector, count);
    SD_WaitComplete();
  } //  aligned buffers
  
  if (sdstat != SD_ERR_SUCCESS )
    return RES_ERROR;
  else
    return RES_OK;
} // disk_read

//* disk_write - Write Sectors.
//  ---------------------------
//
//

DRESULT  disk_write (BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{

  SD_ERROR 
    sdstat;               // status

  (void) pdrv;

//	Misaligned buffers are write+wait single sectors.
//	Aligned buffers post the write and return immediately.  An
//	SD_WaitComplete is issued before any other disk operation is
//	performed.


  SD_WaitComplete();
  if ( IS_MISALIGNED( buff))
  {
    while( count--)
    {  
      memcpy( DiskBuf, buff, BLOCK_SIZE);
      if ( (sdstat = SD_WriteBlocks( DiskBuf, sector, 1)) )
        break;
      SD_WaitComplete();
      sector++;
      buff += BLOCK_SIZE;
    } // do every sector
  } // handle problems
  else
  {  // aligned buffers, no problem
    sdstat = SD_WriteBlocks( (void *) buff, sector, count);
    SD_WaitComplete();
  } // aligned buffers
  
  if (sdstat != SD_ERR_SUCCESS)
    return RES_ERROR;
  else
    return RES_OK;
} // disk_write

//* disk_ioctl - Disk control functions.
//  ------------------------------------
//

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void *buff)
{

  (void) pdrv;

  switch (cmd)
  {
    case GET_SECTOR_COUNT:
      *((uint32_t *) buff) = SD_GetCardSize();
      return RES_OK;
          
    case CTRL_SYNC:
      SD_WaitComplete();
      return RES_OK;

    case GET_SECTOR_SIZE:
      *((WORD *) buff) = BLOCK_SIZE;
      return RES_OK;

    case GET_BLOCK_SIZE:
      *((WORD *) buff) = BLOCK_SIZE;
      return RES_OK;

    default:
      break;
  }				// switch
  return RES_PARERR;
} // disk_ioctl

//  get_fattime - Get RTC in DOS format.
//  ------------------------------------
//
//	Reads the real-time clock to get the date and time.
//

DWORD get_fattime (void)
{

  return GetRTCDOSTime ();

}	// get_fattime
