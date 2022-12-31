//  Serial Debug Driver
//  -------------------

#ifdef SERIAL_DEBUG

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "license.h"
#include "dbserial.h"

#define USART_SPEED 115200    // bit/baud rate of connection.
#define UART_PORT   USART1    // what port

static void Numout( unsigned Num, int Dig, int Radix, int bwz);


//  Initialize UART
//  ---------------
//
//	Note that if UART instead of USB comms is used, this will
//	cause conflicts.
//

int DBinit( void)
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
} // DBinit


//*  DBgetchar - Get a character from input.
//   ---------------------------------------
//
//  Serves as an alias to the USB entry.
//

int DBgetchar( void)
{

  while ((USART_SR(UART_PORT) & USART_SR_RXNE) == 0);
  
  return usart_recv(UART_PORT);
} // DBgetchar

//  DBcharReady - See if a character is ready for input.
//  ---------------------------------------------------
//
//  Returns true if there's a character ready
//

int DBcharReady( void)
{

  return ((USART_SR(UART_PORT) & USART_SR_RXNE) != 0);
} // DBcharReady


//* DBputchar - Write a single character to output.
//  ----------------------------------------------
//
//    Prepends a CR to every LF.
//

int DBputchar( char What)
{

  if ( What == '\n')
    usart_send_blocking(UART_PORT, '\r');
  while ((USART_SR(UART_PORT) & USART_SR_TXE) == 0);
  usart_send(UART_PORT, (uint8_t) What);
  return What;
} // DBputchar

//* DBputs - Put a string to UART.
//  ------------------------------
//
//  Ends with a null.  If a newline is present, adds a CR.
//

void DBputs( char *What)
{

  char
    c;

  while( (c = *What++))
    DBputchar( c);
} // DBputs

//* DBprintf - Simple printf function to UART.
//  -----------------------------------------
//
//    We understand %s and %nx, where n is either null or 1-8
//

void DBprintf( char *Form,...)
{

  const char *p;
  va_list argp;
  int width;
  int i, bwz;
  unsigned u;
  char *s;
  
  va_start(argp, Form);
  
  for ( p = Form; *p; p++)
  {  // go down the format string
    if ( *p != '%')
    { // if this is just a character, echo it
      DBputchar( *p);    
      continue;
    } // ordinary character
    
    ++p;                  // advance
    width = 0;            // assume no fixed width
    bwz = 1;              // say blank when zero
    
    if ( *p == '0')
    {
      p++;
      bwz = 0;            // don't blank when zero
    } // say don't suppress lead zeros
    
    while ( (*p >= '0') && (*p <= '9'))
      width = (width*10) + (*p++ - '0'); // get width of field

    switch (*p)
    {  

      case 'd':
        u = va_arg( argp, unsigned int);
        Numout( u, width, 10, bwz);    // output decimal string
        break;
              
      case 'x':
        u = va_arg( argp, unsigned int);
        Numout( u, width, 16, bwz);     // output hex string
        break;      

      case 'c':
        i = va_arg( argp, int);
        DBputchar(i);
        break;
        
      case 's':
        s = va_arg( argp, char *);
        
//  We have to handle lengths here, so eventually space-pad or truncate.

        if ( !width)
          DBputs(s);            // no width specified, just put it out
        else
        {  
          i = strlen(s) - width;
          if ( width >= 0)
            DBputs( s+i);       // truncate
          else
          {  // if pad
            for ( ; i; i++)
              DBputchar(' ');   // pad  
            DBputs(s);
          }
        } // we have a width specifier
        break;
      
      default:
        break;   
    } // switch
  } // for each character
  va_end( argp);
  return;
} // DBprintf

//  DBgets - Get a string.
//  -----------------------
//
//  Reads a string from input; ends with CR or LF on input
//  Strips the terminal CR or LF; terminal null appended.
//
//  Echo and backspace are also processed.
//
//  On input, address of string, length of string buffer.
//  Returns the address of the null.
//

char *DBgets( char *Buf, int Len)
{

  char c;
  int pos;
 
  for ( pos = 0; Len;)
  {
    c = DBgetchar();
    if ( c == '\r' || c == '\n')
      break;
    if ( c == '\b')
    {	// handle backspace.
      if (pos)
      {
      	DBputs( "\b \b");	// backspace-space-backspace
        pos--;
        Buf--;
        Len++;
      } // if not at the start of line
      continue;			// don't store anything
    } // if backspace
    if ( c >= ' ')
    {
      DBputchar( c);		// echo the character;
      *Buf++ = c;		// store character
      pos++;
      Len--;
      if ( !Len)
      	break;
    } // if printable character
  } // for
  *Buf = 0;
  DBputs( "\r\n");		// end with a newline
  return Buf;
} // DBgets


//  Numout - Output a number in any radix.
//  --------------------------------------
//
//  On entry, Num = number to display, Dig = digits, Radix = radix up to 16.
//  bwz = nonzero if blank when zero.
//  Nothing on return.
//

static void Numout( unsigned Num, int Dig, int Radix, int bwz)
{

  int i;
  const char hexDigit[] = "0123456789abcdef";
  char  outBuf[10];
  
  memset( outBuf, 0, sizeof outBuf);  // zero it all
  if (Radix  == 0 || Radix > 16)
    return;
  
// Use Chinese remainder theorem to develop conversion.

  i = (sizeof( outBuf) - 2); 
  do
  {  
    outBuf[i] = (char) (Num % Radix);
    Num = Num / Radix;
    i--;
   } while (Num);
   
//  If the number of digits is zero, just print the significant number.

  if ( Dig == 0)
    Dig = sizeof( outBuf) - 2 - i;
  for ( i = sizeof( outBuf) - Dig - 1; i < (int) sizeof( outBuf)-1; i++)
  {
    if ( bwz && !outBuf[i] && (i != (sizeof(outBuf)-2)) )
      DBputchar( ' ');
    else
    {
      bwz = 0;  
      DBputchar( hexDigit[ (int) outBuf[i]] );
    }  // not blanking lead zeroes
  }  
} // Numout


//  DBhexin - Read a hex number until a non-hex.
//  --------------------------------------------
//
//    On exit, the hex value is stored in RetVal.
//    and the address of the next non-hex digit is returned.
//
//    If no valid digits are encountered, the return value is 0xffffffff;
//
//    Leading spaces are permitted.
//

char *DBhexin( unsigned int *RetVal, unsigned int *Digits, char *Buf)
{

  unsigned int
    accum;
  char
    c;
  int
    digCount;

  digCount = 0;				// no digits yet
  accum = 0;				// set null accumulator

//  First, strip off any leading spaces.

  while( *Buf == ' ')
    Buf++;				// skip until non-space

//  Now, pick up digits.

  do
  {
    c = *Buf++;
    c = toupper(c);
    if ( c >= '0' && c <= '9')
      accum = (accum << 4) | (c - '0');
    else if (c >= 'A' && c <= 'F')
      accum = (accum << 4) | (c - 'A' +10);
    else
    {
      Buf--;
      *Digits = digCount;
      *RetVal = accum;
      return Buf;
    }
    digCount++;
  } while(1);
} // DBhexin

#endif
