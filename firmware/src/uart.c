#ifdef USE_UART

//*  	UART Driver
//  	-----------
//
//	If the USE_UART symbol is define at compilation, UART 1 will be
//	used for the interface rather than USB CDC.   Note that serial
//	debugging must not be used if this option is selected--there's
//	too much conflict between the two.
//

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <miscsubs.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "license.h"

#include "usbserial.h"

#define USART_SPEED 115200    // bit/baud rate of connection.
#define UART_PORT   USART1    // what port

int _write( int Fd, char *What, int Count);

//  Initialize UART
//  ---------------
//
//

int USInit( void)
{
 
// Enable GPIOD clock for  UART1 pins. 

  rcc_periph_clock_enable(RCC_GPIOA);
  rcc_periph_clock_enable(RCC_USART1);    // change this if you move poarts

// Setup GPIO pins for UART_PORT transmit. 

  gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9);
  gpio_set_af(GPIOA, GPIO_AF7, GPIO9);

//  Setup GPIO pins for UART_PORT receive.

  gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO10);
  gpio_set_output_options(GPIOA, GPIO_OTYPE_OD, GPIO_OSPEED_25MHZ, GPIO10);

// ...And connect to the AF1 bus.

  gpio_set_af(GPIOA, GPIO_AF7, GPIO9);
  gpio_set_af(GPIOA, GPIO_AF7, GPIO10);

//  Now, set up the UART characteristics and speed.

  usart_set_baudrate(UART_PORT, USART_SPEED);
  usart_set_databits(UART_PORT, 8);
  usart_set_stopbits(UART_PORT, USART_STOPBITS_1);
  usart_set_mode(UART_PORT, USART_MODE_TX_RX);
  usart_set_parity(UART_PORT, USART_PARITY_NONE);
  usart_set_flow_control(UART_PORT, USART_FLOWCONTROL_NONE);
  usart_enable( UART_PORT);

  return 0;
} // USInit

//* USClear - Clear input buffer contents.
//  --------------------------------------
//
//	No buffer, so no-op.
//

void USClear( void)
{
  return;
} // USClear


//*  USGetchar - Get a character from input.
//   ---------------------------------------
//
//  Serves as an alias to the USB entry.
//

int USGetchar( void)
{

  while ((USART_SR(UART_PORT) & USART_SR_RXNE) == 0);
  
  return usart_recv(UART_PORT);
} // USGetchar

//  USCharReady - See if a character is ready for input.
//  ---------------------------------------------------
//
//  Serves as an alias to the USB entry.
//

int USCharReady( void)
{

  return ((USART_SR(UART_PORT) & USART_SR_RXNE) != 0);
} // USCharReady


//* USPutchar - Write a single character to output.
//  ----------------------------------------------
//
//    Prepends a CR to every LF.
//

int USPutchar( char What)
{

  if ( What == '\n')
    usart_send_blocking(UART_PORT, '\r');
  while ((USART_SR(UART_PORT) & USART_SR_TXE) == 0);
  usart_send(UART_PORT, (uint8_t) What);
  return What;
} // USPutchar

//* USWritechar - Write "raw" character with no lf filtering.
//  ---------------------------------------------------------
//
//	Writes the character with no processing.
//

int USWritechar( char What)
{

  while ((USART_SR(UART_PORT) & USART_SR_TXE) == 0);
  usart_send(UART_PORT, (uint8_t) What);
  return What;
} // USWritechar

//* USPuts - Put a string to UART.
//  ------------------------------
//
//  Ends with a null.  If a newline is present, adds a CR.
//

void USPuts( char *What)
{

  char
    c;

  while( (c = *What++))
    USPutchar( c);
} // USPuts

//*  USWriteBlock - Write a block of characters without "cooking"
//   ------------------------------------------------------------
//
//      This is a bit more efficient than single-character writes.
//
//      Always returns zero.
//

int USWriteBlock( uint8_t *What, int Count)
{

  while( Count)
  {

    while ((USART_SR(UART_PORT) & USART_SR_TXE) == 0);
    usart_send(UART_PORT, (uint8_t) *What++);
    Count--;
  } // while
  
  return 0;

} // USWriteBlock

//*  Write hooked to stdio routines.
//   ------------------------------
//

int _write( int Fd, char *What, int Count)
{

  int written = Count;
  
  (void) Fd;
  
  while ( Count > 0)
  {
    USPutchar( *What++);
    Count--;
  }
  return written;
} // _write


#endif