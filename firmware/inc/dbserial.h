//  Debug UART prototypes.

#ifndef DBSERIAL_H_DEFINED

#ifdef SERIAL_DEBUG
int DBinit( void);
int DBgetchar( void);
int DBcharReady( void);
int DBputchar( char What);
void DBputs( char *What);
void DBprintf( char *Form,...);
char *DBgets( char *Buf, int Len);
char *DBhexin( unsigned int *RetVal, unsigned int *Digits, char *Buf);
#else
#define DBinit(...) 0
#define DBgetchar(...) 0
#define DBprintf(...) 0
#define DBcharReady(...) 0
#define DBputchar(x) 0
#define DBputs(x) 
#define DBgets(x,y) 0
#define DBhexin(x,y,z) 0
#endif
#endif
