//  UART prototypes.


#ifndef COMM_DEFINED

void InitACM( int BaudRate);
int Ucharavail( void);
unsigned char Ugetchar( void);
void Uputchar( unsigned char What);
void Uputs( char *What);
void Uprintf( char *Form,...);
char *Ugets( char *buf, int len);
char *Hexin( unsigned int *RetVal, unsigned int *Digits, char *Buf);

#define COMM_DEFINED 1
#endif
