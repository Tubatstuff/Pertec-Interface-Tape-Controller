#ifndef __SDIOSUBS_H
#define __SDIOSUBS_H

#include "stdint.h"

//  Error return values

typedef enum _sdioerrors_
{
  SD_ERR_SUCCESS = 0,           // success
  SD_ERR_NO_CARD,               // cannot detect card
  SD_ERR_BAD_CARD,              // card signals an error
  SD_ERR_TIMEOUT,               // operation hung, need restart
  SD_ERR_DMA,                   // DMA under/overrun
  SD_ERR_CRC,                   // data or command CRC error
  SD_ERR_PROTOCOL,              // protocol error
  SD_ERR_MISALIGNED,            // Buffer not on 32-bit boundary
  SD_ERR_LENGTH,                // transfer length must be a multiple of 512
  SD_ERR_CARD,                  // internal card error
  SD_ERR_PARAMETER,             // parameter error
  SD_ERR_NOT_INITIALIZED,       // Card has not been initialized
  SD_ERR_BUSY
} SD_ERROR;

void SD_Reset(bool Power);
SD_ERROR SD_Init(void);
uint32_t SD_GetCardSize( void);
SD_ERROR SD_ReadBlocks( void *Buf, uint32_t Sector, uint32_t Count);
SD_ERROR SD_WriteBlocks( void *Buf, uint32_t Sector, uint32_t Count);
SD_ERROR SD_WaitComplete( void);


#endif //__SDIOSUBS_H

