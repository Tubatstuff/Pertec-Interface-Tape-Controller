//
//    SDCard/MMC definitions.
//

#ifndef _SDCARD_DEFINED
#define _SDCARD_DEFINED

//  SDCard commands.

enum
{
  SD_CMD_RESET=0,             // reset card
  SD_CMD_INIT=1,              // old init commnad
  SD_CMD_ALL_SEND_CID=2,      // not supported in SDIO mode
  SD_CMD_SEND_REL_ADDR=3,     // Send relative address
  SD_CMD_SET_DSR=4,           // set DSR of all cards
  SD_CMD_IO_SEND_OP_CMD=5,    // Used for SDIO devices only
  SD_CMD_SWITCH_FUNC=6,       // switch card function mode
  SD_CMD_SELECT_CARD=7,       // Select/deselect card
  SD_CMD_IF_COND=8,           // Check voltage range (V2 only)
  SD_CMD_SEND_CSD=9,          // Get CSD data
  SD_CMD_SEND_CID=10,         // Get card ID
  SD_CMD_STOP_TRANS=12,       // Stop transmission
  SD_CMD_SEND_STATUS=13,      // send card status
  SD_CMD_BLKLEN=16,           // set block lengthS
  SD_CMD_READ_SNGL=17,        // read single
  SD_CMD_READ_MULTI=18,       // read multiple blocks
  SD_CMD_WRITE_SNGL=24,       // write single
  SD_CMD_WRITE_MULTI=25,      // write multiple blocks
  SD_CMD_APP_CMD=55,          // prefix for ACMD commands
  SD_CMD_READ_OCR=58          // read operating conditions register
};

//  Application commands; must be peceded by a 55 command.

enum
{
  SD_ACMD_SET_WIDTH=6,        // Set Bus width
  SD_ACMD_SD_STATUS=13,       // Get card status
  SD_ACMD_NUM_BLOCKS=23,      // Send number of write blocks
  SD_ACMD_INIT=41,            // initialize card
  SD_ACMD_CARD_DETECT=42,     // Set/Clear card detect
  SD_ACMD_SEND_SCR=51         // Send SCR
};

//  OCR bits for SD_ACMD_INIT

#define SD_OCR_HCS (1<<30)        // high capacity for ACMD_INIT status
#define SD_OCR_READY (1<<31)      // Ready (i.e. not busy) in ACMD_INIT 

//  Status bits - type R1

#define SD_R1_PARAM 0x40          // parameter error
#define SD_R1_ADDR  0x20          // address error
#define SD_R1_ERASE 0x10          // erase sequence error
#define SD_R1_CRC   0x08          // CRC error
#define SD_R1_ILLEGAL 0x04        // illegal command
#define SD_R1_ERESET 0x02         // erase reset
#define SD_R1_IDLE  0x01          // idle

//  SD transfer and error status token.

#define SD_TOKEN_BLOCK_START 0xfe // start of block token
#define SD_TOKEN_ERROR 1          // Error encountered
#define SD_TOKEN_CCERR 2          // CRC error
#define SD_TOKEN_ECC   4          // ECC error
#define SD_TOKEN_RANGE 8          // Range error
#define SD_TOKEN_LOCKED 16        // Card is locked
#define SD_TOKEN_MASK  0xe0       // if these bits are 000, there's an error

//  SDType bits that describe capabilities.

#define SD_TYPE_V1 1              // Card accepts V1 commands
#define SD_TYPE_V2 2              // card accepts V2 commands
#define SD_TYPE_MMC 4             // card accepts MMC commands
#define SD_TYPE_SDHC 8            // SD HD card commands

#define SD_SECTOR_SIZE 512        // length of a sector

#endif