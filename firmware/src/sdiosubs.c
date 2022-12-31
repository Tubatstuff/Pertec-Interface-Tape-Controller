#include <stdbool.h>
#include <stdint.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/sdio.h>
#include <libopencm3/cm3/nvic.h>
#include "comm.h"
#include "sdiosubs.h"
#include "sdcard.h"

//*	Routines for STM32F4 SDIO Support
//	----------------------------------


//  These two macros help avoid a mess of nested "if" statements.  
//  Basically, they define a once-through sequence of code that 
//  can be exited with "break" statements.

#define BEGIN do
#define END while(false);

//  Timeouts.   The SD standard says to allow 5msec for read and 250 for write.
//  We're using a 24 MHz SDIO clock, so we figure it according to the number
//  of SDIO cycles. There are 24000 cycles per millisecond, so, for each block
//  transferred:

#define DATA_READ_TIMEOUT  (24000*5)
#define DATA_WRITE_TIMEOUT (24000*250)

//  The following macro is a no-op currently, but should eventually
//  be a timeout check.

#define NULLOP {}               // null operation for stall loops
#define INIT_RETRIES 100        // how many times to retry initialization
#define RETRY_COUNT  10         // how many times to repeat a command
#define STOP_RETRIES 10         // how many times to get status?

//  Since DMA is used here, we check buffers for alignment.
//  The following macro returns nonzero if the address argument
//  is not on a 32-bit boundary.

#define IS_MISALIGNED(x) (((uint32_t)(x)) & 0x03)

//  We set some sort of limit on the maximum transfer size.  For now,
//  let it be 64K.

#define MAX_TRANSFER_SIZE 128         // maximum transfer size in blocks


//  SDIO errors - these are internal to SD_Command results.

typedef enum
{
  SDIO_ERR_SUCCESS = 0,     // no error
  SDIO_ERR_CMD_CRC,         // command CRC error
  SDIO_ERR_CTIMEOUT,        // command timeout
  SDIO_ERR_NORESP,          // no response after command
  SDIO_ERR_BAD_CARD,        // card failure  
  SDIO_ERR_UNKNOWN          // don't know how to categorize
} SDIO_ERROR;

//  The following defines all active SDIO status register bits.

#define SDIO_STA_MASK (SDIO_ICR_CEATAENDC | SDIO_ICR_SDIOITC | \
                       SDIO_ICR_DBCKENDC  | SDIO_ICR_STBITERRC | \
                       SDIO_ICR_DATAENDC  | SDIO_ICR_CMDSENTC | \
                       SDIO_ICR_CMDRENDC  | SDIO_ICR_RXOVERRC | \
                       SDIO_ICR_TXUNDERRC | SDIO_ICR_DTIMEOUTC | \
                       SDIO_ICR_CTIMEOUTC | SDIO_ICR_DCRCFAILC | \
                       SDIO_ICR_CCRCFAILC)

//  Local storage
//  -------------

static volatile SD_ERROR
  ErrorStatus;            // current error status
static volatile bool
  DMADone;                // if set true, DMA is complete

//  Card types

static enum
{
  SD_TYPE_STANDARD_CAPACITY,
  SD_TYPE_HIGH_CAPACITY
} SDType;

//  DMA transfer direction.

typedef enum _dma_tranfer
{
  DMA_MEMORY_TO_SD,           // move memory data to SD (i.e. WRITE)
  DMA_SD_TO_MEMORY            // move SD data to memory (i.e. READ)
} DMA_DIRECTION;

static uint32_t
  RCA,                      // Card address
  CardSize = 0;             // Card size in sectors, zero if no card.
 
static bool
  GPIOInitialized = false;  // used to check if SD_LowLevel_Init has been called

static enum 
{
  XFER_INACTIVE = 0,        // no transfer
  XFER_READING_SINGLE,      // read single block
  XFER_READING_MULTI,       // read multiple block
  XFER_WRITING_SINGLE,      // write single block
  XFER_WRITING_MULTI        // write multiple block
} TransferPending;

//  Prototypes.

static SD_ERROR TestSDStatus( void);
static void SD_GPIO_Init(void);
static int SD_Command( uint8_t Command, uint32_t Arg);
static void SD_BeginTransfer(void *Buf, uint32_t Count, DMA_DIRECTION Dir);
static void SD_StopMultiWrite( void); 

//  TestSDStatus - Preliminary check for SD Status.
//  -----------------------------------------------
//
//    Basically checks for SD interface initialized and
//    card present.
//
//    Returns status.
//

static SD_ERROR TestSDStatus( void)
{

  if ( !GPIOInitialized)
    return SD_ERR_NOT_INITIALIZED;    // whoops, the GPIO isn't done

  if (!CardSize)
    return SD_ERR_NO_CARD;    // initialized, but no card

  return SD_ERR_SUCCESS;      // okay, we're ready
} // TestSDStatus

//  SD_GPIO_Init - Initialize GPIOs and DMA
//  ----------------------------------------
//
//  Note that PD2 = SDIO CMD
//  and PC8-PC11  = SDIO D0-D3
//  and PC12      = SDIO CLK
//
//  We start out with a 400KHz clock and 1-bit mode.
//

static void SD_GPIO_Init(void)
{

//  Enable clocks for SDIO and DMA2

  rcc_periph_clock_enable( RCC_DMA2);
  rcc_periph_clock_enable( RCC_SDIO);
  
//  Setup GPIO pins.  Note that PC8-PC11 are DIO 0-3, PC12 = SD clock
//  PD2 is command.

  rcc_periph_clock_enable( RCC_GPIOC);

  gpio_set_output_options (GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ, GPIO12);
  gpio_set_output_options (GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ,
                           GPIO8 | GPIO9 | GPIO10 | GPIO11);
  gpio_mode_setup (GPIOC, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
                   GPIO8 | GPIO9 | GPIO10 | GPIO11);
  gpio_mode_setup (GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO12);
  gpio_set_af (GPIOC, GPIO_AF12, GPIO8 | GPIO9 | GPIO10 | GPIO11 | GPIO12);
  gpio_set_af (GPIOD, GPIO_AF12, GPIO2);

  gpio_set_output_options (GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ, GPIO2);
  gpio_mode_setup (GPIOD, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO2);
  GPIOInitialized = true;     // say we did it
  TransferPending = false;    // kill any pending transfer
  return;
} // SD_GPIO_Init

//  SD_Reset - Reset Card and Power.
//  --------------------------------
//
//  If Power == false, unpower the SDIO card, so that it can reset.
//

void SD_Reset(bool Power)
{

//  Turn power off.

  SDIO_POWER = SDIO_POWER_PWRCTRL_PWROFF;

//  Reset the SDIO interface.

  rcc_peripheral_reset(&RCC_APB2RSTR, RCC_APB2RSTR_SDIORST);
  rcc_peripheral_clear_reset(&RCC_APB2RSTR, RCC_APB2RSTR_SDIORST);
  TransferPending = XFER_INACTIVE;     // cancel any transfers not finished

//  We re-apply power to the card only if requested.

  if (Power)
  { // power on
    SDIO_POWER = SDIO_POWER_PWRCTRL_PWRON;
    SDIO_CLKCR = (SDIO_CLKCR_CLKEN | 120);  // 1 bit, 400 Khz
  } // if powering on
} // SD_Reset

//  SD_Command - Issue command, return error status.
//  ------------------------------------------------
//
//    On entry, Command = SD card commnad
//              Arg = 32-bit argument
//
//    returns SDIO_ERR_xxx status (see above)
//

static int SD_Command( uint8_t Command, uint32_t Arg)
{

  uint32_t
    tmp;
  int
    error;

  error = 0;
  tmp = (SDIO_CMD & 
          (SDIO_CMD_ATACMD | SDIO_CMD_NIEN |
          SDIO_CMD_ENCMDCOMPL | SDIO_CMD_SDIOSUSPEND)) |
          (Command & SDIO_CMD_CMDINDEX_MASK) |  // Put the Command in
          SDIO_CMD_CPSMEN;                      // We'll be running CPSM

//  Figure response times for various commands.

  switch( Command)
  {

    case SD_CMD_RESET:          // reset just forces card to idle
      tmp |= SDIO_CMD_WAITRESP_NO_0;
      break;
 
    case SD_CMD_ALL_SEND_CID:
    case SD_CMD_SEND_CSD:
      tmp |= SDIO_CMD_WAITRESP_LONG;
      break;
 
    default:
      tmp |= SDIO_CMD_WAITRESP_SHORT; // the common case
      break;
  } // switch

//  Reset status bits by writing 1s into the ICR:

  SDIO_ICR = SDIO_STA_MASK;

//  Now issue the command and argument.

  SDIO_ARG = Arg;
  SDIO_CMD = tmp;
 
//  Wait for command to be accepted.

  tmp = 0;
  do
  {
    tmp |= ( SDIO_STA & SDIO_STA_MASK);   // collect status
  } while ((SDIO_STA & SDIO_STA_CMDACT) || (!tmp) );
 
  SDIO_ICR = tmp;     // clear any flags

//  Figure out any errors.

  if (!tmp)
    error = SDIO_ERR_NORESP;
  else if (tmp & SDIO_STA_CCRCFAIL)
    error = SDIO_ERR_CMD_CRC;
  else if (tmp & (SDIO_STA_CMDREND | SDIO_STA_CMDSENT))
    error = SDIO_ERR_SUCCESS;
  else if (tmp & SDIO_STA_CTIMEOUT)
    error = SDIO_ERR_CTIMEOUT;
  else
    error = SDIO_ERR_UNKNOWN;
  return error;
} // SD_Command

//  SD_Init - Initialize card
//  -------------------------
//
//  When we complete, we'll have a 4 bit, 25MHz clock set up.
//  Returns status value.
//

SD_ERROR SD_Init(void)
{

  int
    error,
    retry;        // retry counter

  uint32_t
    temp;

  if ( !GPIOInitialized)
    SD_GPIO_Init();
  CardSize = 0;                         // say card is uninitialized

//  Initialize the SDIO (with ~400Khz clock in 1 b-t mode) (48MHz / 120)
  
  SD_Reset( true);              // turn the card on
 
  BEGIN
  {
 
    if ( (error = SD_Command(SD_CMD_RESET, 0)) )
      break;                    // go to idle
 
//  Send interface conditions request.  Essentially will tell us that
//  we have a 3.3 volt card.   This applies to V2 cards only.  Earlier
//  cards will cause problems.

    if ( (error =  SD_Command(SD_CMD_IF_COND, 0x1AA)) )
    {
      error = SDIO_ERR_BAD_CARD;  // say it's a bad V1 card
      break;                    // we support only V2 cards
    }
 
// V2 cards will return AA; earlier cards will return an error.
//
//  Next, make sure this is a card, and not some other SDIO device.

    if ( !(error = SD_Command( SD_CMD_IO_SEND_OP_CMD, 0)) )
    {
      error = SDIO_ERR_BAD_CARD;
      break;        // fake an error
    } // if an SDIO device, but not a card
 
    for ( retry = 0; retry < INIT_RETRIES; retry++)
    {
 
//  Send out an application command  to initialize card.  We wait a bit for
//  the response--it can take several iterations.
 
      if ( SD_Command( SD_CMD_APP_CMD, 0))
        break;                // this should always be accepted
 
      error = SD_Command( SD_ACMD_INIT,  0xC0100000);
      if ( error != SDIO_ERR_CMD_CRC)
        break;                 // we expect a bad command CRC error
      
      if ( !(SDIO_RESP1 & SD_OCR_READY) )   // bit 31 is set low if not ready
        continue;               // keep trying
      else
      {
        if ( SDIO_RESP1 & SD_OCR_HCS)       // check for high capacity
          SDType = SD_TYPE_HIGH_CAPACITY;
        else
           SDType = SD_TYPE_STANDARD_CAPACITY;
        break;
      } // when card goes ready
    } // Keep trying
    if ( retry == INIT_RETRIES)
       return SD_ERR_NO_CARD;             // bad or no card

//  Card is in ready state.  Get CID to put card in identification mode.

    if ( (error = SD_Command( SD_CMD_ALL_SEND_CID, 0)) )
      break;              // if get CID failed

//  Get the card's RCA (Relative card address)

    if ( (error = SD_Command(SD_CMD_SEND_REL_ADDR, 0)) )
      break;
    temp = SDIO_RESP1;
    RCA = (temp >> 16);  // get upper 16 bits for card address

//  If the card returns 0, we tell it to pick.
    
    if ( !RCA)
    { // try again
      SD_Command( SD_CMD_SEND_REL_ADDR, 0);
      RCA = (SDIO_RESP1 >> 16);  // get upper 16 bits for card address again
    }
 
//  Now that we have the card address, we can switch into 4 bit mode at
//  25 MHz.
 
    if ( (error = SD_Command(SD_CMD_SEND_CSD, (RCA << 16))) )
      break;          // we have to get the CSD

//  Dig out CSD information.

    if (SDType == SD_TYPE_STANDARD_CAPACITY)
    { // this is a V1 card--extraction is a bit messy
 
      uint32_t
        read_bl_len,      // parameters returned from info
        c_size,
        c_size_mult,
        mult,
        blocknr,
        block_len;
 
      read_bl_len = (SDIO_RESP2 >> 16) & 0xF;
      c_size = ((SDIO_RESP2 & 0x3FF) << 2) | (SDIO_RESP3 >> 30);
      c_size_mult = (SDIO_RESP3 >> 15) & 0x7;
      mult = 1 << (c_size_mult + 2);
      blocknr = (c_size + 1) * mult;
      block_len = 1 << read_bl_len;
      CardSize = (block_len * blocknr) >> 9;    // size in sectors
    } else { // this is a V2 card

      uint32_t
        c_size;
 
      c_size = ((SDIO_RESP2 & 0x3F) << 16) | (SDIO_RESP3 >> 16);
      CardSize = (c_size + 1) << 10;          // size in sectors
    } // if V2 card

//  Next we need to change the bus width and card speed.  Select the card
//  and set it up.

    SD_Command(SD_CMD_SELECT_CARD, (RCA << 16));
 
//  First, set the new card bus width.

    SD_Command(SD_CMD_APP_CMD, (RCA << 16));
 
    SD_Command(SD_ACMD_SET_WIDTH, 0x02);
 
//  Set the SDIO controller to 24 MHz (as close as we can get to 25).
 
     SDIO_CLKCR = (SDIO_CLKCR_WIDBUS_4 | SDIO_CLKCR_CLKEN);  // 4 bit Bus Width;
  }
  END

  if ( error)
  {
    CardSize = 0;               // say no card
    return SD_ERR_CARD;
  }
 
  return SD_ERR_SUCCESS;       // say we're good
} // SD_Init

//  SD_GetCardSize - Return card size
//  ---------------------------------
//
//    Returns card size in sectors.  If 0, no card is present or initialized.
//

uint32_t SD_GetCardSize( void)
{

  return CardSize;

} // SD_GetCardSize

//  SD_WriteBlocks - Write blocks to SD.
//  ------------------------------------
//
//  Arguments are buffer address and sector address.
//

SD_ERROR SD_WriteBlocks( void *Buf, uint32_t Sector, uint32_t Count)
{

  SD_ERROR
    errStat;              // error status
 
  if ( (errStat = TestSDStatus()) != SD_ERR_SUCCESS)
    return errStat;       // if not ready for transfer
 
  if ( IS_MISALIGNED( Buf))
    return SD_ERR_MISALIGNED;   // whoops, misaligned buffer.

  if ( Count == 0)
    return SD_ERR_SUCCESS;      // read nothing always succeeds

  if ( Count > MAX_TRANSFER_SIZE)
    return SD_ERR_PARAMETER;    // argument out of range
 
  if (SDType != SD_TYPE_HIGH_CAPACITY)
    Sector *= SD_SECTOR_SIZE;   // V1 cards use byte offset

//  If we have a transfer pending, clear it.

  if (TransferPending != XFER_INACTIVE)
    SD_WaitComplete();

//  Set block length.

  SD_Command( SD_CMD_BLKLEN, SD_SECTOR_SIZE);

//  And number of blocks, if doing a multi-write.
    
  if ( Count > 1)
  {    
    SD_Command( SD_CMD_APP_CMD, RCA << 16);  
    SD_Command( SD_ACMD_NUM_BLOCKS, Count);     // set number of blocks
  }
  
  if ( SD_Command( ((Count == 1) ? SD_CMD_WRITE_SNGL : SD_CMD_WRITE_MULTI), 
                     Sector))
    return SD_ERR_NO_CARD;

  SD_BeginTransfer(Buf, Count, DMA_MEMORY_TO_SD);
 
//  Return - the caller must wait for the transmission to end.

  return SD_ERR_SUCCESS;
} // SD_WriteBlocks

//  SD_ReadBlocks - Initiate read of blocks.
//  ----------------------------------------
//
//    Initiate a read of a single block.
//

SD_ERROR SD_ReadBlocks( void *Buf, uint32_t Sector, uint32_t Count)
{

  SD_ERROR
    errStat;              // error status

  if ( (errStat = TestSDStatus()) != SD_ERR_SUCCESS)
    return errStat;       // if not ready for transfer
 
  if ( IS_MISALIGNED( Buf))
    return SD_ERR_MISALIGNED;   // whoops, misaligned buffer.

  if ( Count == 0)
    return SD_ERR_SUCCESS;      // read nothing always succeeds

  if ( Count > MAX_TRANSFER_SIZE)
    return SD_ERR_PARAMETER;    // argument out of rangea%

  if (SDType != SD_TYPE_HIGH_CAPACITY)
    Sector *= SD_SECTOR_SIZE;   // convert everything to byte offset

//  If we have a transfer pending, clear it.

  if (TransferPending)
    SD_WaitComplete();

//  Inform the card of our sector size.

  SD_Command( SD_CMD_BLKLEN, SD_SECTOR_SIZE);
    
//  Begin DMA transfer from card.
 
  SD_BeginTransfer(Buf,Count,DMA_SD_TO_MEMORY);

// Issue READ MULTIPLE BLOCKS

  if ( SD_Command( ((Count == 1) ? SD_CMD_READ_SNGL :SD_CMD_READ_MULTI), 
                    Sector))
    return SD_ERR_CARD;

  return SD_ERR_SUCCESS;
 
} //  SD_ReadBlocks
 
//  SD_BeginTransfer - Set up DMA transfer.
//  ---------------------------------------
//
//   This just initiaztes ("primes") DMA transfer
//
//  The buffer must be aligned to a 32-bit boundary.
//  The count is the number of blocks transfered.
//

static void SD_BeginTransfer(void *Buf, uint32_t Count, DMA_DIRECTION Dir)
{

  bool
    readingSD;              // true if reading from SD

//  Start by clearing SDIO status.

  SDIO_ICR = SDIO_STA_MASK;
  readingSD = (Dir == DMA_SD_TO_MEMORY);    // otherwise, we're writing

//  Reset the control register (0x00 is the default value. this also disables the dma.

  DMA2_S3CR = 0; 

//  Clear DMA flags.

  DMA2_LIFCR =  DMA_LIFCR_CTCIF3 | 
                DMA_LIFCR_CTEIF3 | 
                DMA_LIFCR_CDMEIF3 | 
                DMA_LIFCR_CFEIF3 | 
                DMA_LIFCR_CHTIF3;

//  Set memory and peripheral sizes and increments.

  dma_set_memory_address( DMA2, DMA_STREAM3, (uint32_t) Buf);
  dma_set_peripheral_address( DMA2, DMA_STREAM3, (uint32_t) &SDIO_FIFO);

  dma_channel_select( DMA2, DMA_STREAM3,  DMA_SxCR_CHSEL_4);

  dma_set_memory_burst( DMA2, DMA_STREAM3, DMA_SxCR_MBURST_INCR4);
  dma_set_peripheral_burst( DMA2, DMA_STREAM3, DMA_SxCR_PBURST_INCR4);

  dma_set_priority( DMA2, DMA_STREAM3, DMA_SxCR_PL_VERY_HIGH);

  dma_set_memory_size( DMA2, DMA_STREAM3, DMA_SxCR_MSIZE_32BIT);
  dma_set_peripheral_size( DMA2, DMA_STREAM3, DMA_SxCR_PSIZE_32BIT);

  dma_enable_memory_increment_mode(DMA2, DMA_STREAM3);  
  dma_disable_peripheral_increment_mode( DMA2, DMA_STREAM3);
  dma_set_peripheral_flow_control( DMA2, DMA_STREAM3);

//  Set up to use full FIFO.

  dma_set_fifo_threshold( DMA2, DMA_STREAM3, DMA_SxFCR_FTH_4_4_FULL); 
  dma_enable_fifo_mode( DMA2, DMA_STREAM3);

//  Set direction.

  if ( readingSD)
    dma_set_transfer_mode( DMA2, DMA_STREAM3, DMA_SxCR_DIR_PERIPHERAL_TO_MEM);
  else
    dma_set_transfer_mode( DMA2, DMA_STREAM3, DMA_SxCR_DIR_MEM_TO_PERIPHERAL);

//  Enable DMA TC interrupt.

  DMADone = false;
  dma_enable_transfer_complete_interrupt( DMA2, DMA_STREAM3);

//  Next, tend to the SDIO side.

// Set the timer to 500 msec..

  SDIO_DTIMER = 12000000;    
    
//  Set the total number of bytes to be moved.

  SDIO_DLEN = Count * SD_SECTOR_SIZE;

//  Start the operation with the DCTRL register.

  SDIO_DCTRL =
              SDIO_DCTRL_DBLOCKSIZE_9   |     // 512 byte block
              SDIO_DCTRL_DTEN   |             // data transfer enabled
              (readingSD ? SDIO_DCTRL_DTDIR : 0) | // direction
              SDIO_DCTRL_DMAEN;             // enable DMA

//  Set up the SDIO interrupt.

  SDIO_MASK = SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT | SDIO_STA_DATAEND | 
              SDIO_STA_RXOVERR | SDIO_STA_STBITERR;

  ErrorStatus = SD_ERR_BUSY;
  nvic_enable_irq( NVIC_SDIO_IRQ);
  nvic_enable_irq( NVIC_DMA2_STREAM3_IRQ);
  
//  Say we have a transfer pending.

  if ( readingSD)
    TransferPending = (Count==1) ? XFER_READING_SINGLE : XFER_READING_MULTI;
  else
    TransferPending = (Count==1) ? XFER_WRITING_SINGLE : XFER_WRITING_MULTI;  

//  Enable the DMA

  dma_enable_stream( DMA2, DMA_STREAM3);

  return;
} // SD_BeginTransfer

//**	Routines Dealing With End-Of-Operation.
//	=======================================


//  SD_WaitComplete - Wait for DMA to finish.
//  -----------------------------------------
//
//  Of course, the data transfer may not have even been initiated, so
//  take that into account.  If DMA is stil going on, wait until it's
//  finished.
//
//  In any case, pick up the error flags and pass them on.
//

SD_ERROR SD_WaitComplete( void)
{

  if ( TransferPending == XFER_INACTIVE)
    return SD_ERR_SUCCESS;		// if nothing happening just quti

//  Wait for the transfer to be complete.

  while( SDIO_STA & (SDIO_STA_TXACT | SDIO_STA_RXACT)) NULLOP;
  
//  Wait for the SDIO interrupt.

  while( ErrorStatus == SD_ERR_BUSY) NULLOP;

// and then wait for the card to idle.

  if (TransferPending == XFER_WRITING_MULTI)
    SD_StopMultiWrite();		// stop the operation.
  else
    SD_Command( SD_CMD_STOP_TRANS, 0);
  TransferPending = XFER_INACTIVE;
  return ErrorStatus;
} // SD_WaitComplete

//*	SD_StopMultiWrite - End of multiblock writing.
//	----------------------------------------------
//
//

static void SD_StopMultiWrite( void) 
{

//  uint32_t 
//    response;
  
// Issue CMD12 - STOP TRANSMISSION.  

  SD_Command( SD_CMD_STOP_TRANS, 0);
  while (!((SDIO_RESP1 >> 8) & SD_R1_IDLE) ) 
  { // wait for things to settle down.

//  Issue SEND_STATUS

     SD_Command(SD_CMD_SEND_STATUS, RCA << 16);
  }  // Wait for a response
} // SD_StopMultiWrite

//**	SD and DMA Interrupt Servicing.
//	===============================


//  SD_ServiceSDIOInterrupt - SDIO ISR
//  ----------------------------------
//
//    Clears SDIO_IT flags and sets status.
//

static void SD_ServiceSDIOInterrupt( void)
{ 

  uint32_t
    status;


  status = SDIO_STA;          // pick up status register
  BEGIN
  {

    if (status & SDIO_STA_DATAEND)
    {
      ErrorStatus = SD_ERR_SUCCESS;
      break; 
    } // successful end of operation

    if (status & SDIO_STA_DCRCFAIL)
    {
      ErrorStatus = SD_ERR_CRC;
      break; 
    } // Data CRC failure

    if (status & SDIO_STA_DTIMEOUT)
    {
      ErrorStatus = SD_ERR_TIMEOUT;
      break; 
    } // Data timeout

    if (status & SDIO_STA_RXOVERR)
    {
      ErrorStatus = SD_ERR_DMA;
      break; 
    } // DMA receive overrun

    if (status & SDIO_STA_TXUNDERR)
    {
      ErrorStatus = SD_ERR_DMA;
      break; 
    } // DMA transmit underrun 

    if (status & SDIO_STA_STBITERR)
    {
      ErrorStatus = SD_ERR_PROTOCOL;
      break; 
    } // successful end of operation

    if (status & SDIO_STA_DCRCFAIL)
    {
      ErrorStatus = SD_ERR_CRC;
      break; 
    } // successful end of operation

    ErrorStatus = SD_ERR_NOT_INITIALIZED;     // this should never happen
    break;
  } END;

//  Clear the interrupt mask.

  SDIO_MASK = 0;        // all interrupts get disabled.
  return;
} // SD_ServiceSDIOInterrupt


//  SD_ServiceDMAInterrupt - DMA ISR.
//  ---------------------------------
//
//  Clears error flags and sets the DMADone flag.
//

static void SD_ServiceDMAInterrupt( void)
{
  if (dma_get_interrupt_flag( DMA2, DMA_STREAM3, DMA_TCIF)) 
  {
    DMADone = true;
    dma_clear_interrupt_flags( DMA2, DMA_STREAM3, 
      DMA_TCIF | DMA_FEIF | DMA_TEIF);
  }
  return;
} // ServiceDMAInterrupt


//  DMA2 Stream 3, ISR.
//  -------------------
//
//

void dma2_stream3_isr (void)
{
  SD_ServiceDMAInterrupt();
} // dma2_stream3_isr

//  SDIO Interrupt handler.
//  -----------------------
//

void sdio_isr	(void)
{
  SD_ServiceSDIOInterrupt();
} // sdio_isr
