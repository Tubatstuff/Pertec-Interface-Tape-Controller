//  UART prototypes.

#ifndef UART_H_DEFINED

void InitUART( void);
int Ucharavail( void);
unsigned char Ugetchar( void);
void Uputchar( unsigned char What);
void Uputs( char *What);
void Uprintf( char *Form,...);
char *Ugets( char *buf, int len);
char *Hexin( unsigned int *RetVal, unsigned int *Digits, char *Buf);

#define UART_H_DEFINED 1
#endif
