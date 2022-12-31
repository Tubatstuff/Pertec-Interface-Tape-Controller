#ifndef _TAPEUTIL_INC
#define _TAPEUTIL_INC

//	Prototypes.

void ShowBriefStatus (void);
void CmdShowStatus( char *args[]);
void CmdSetAddr( char *args[]);
void CmdSetRetries( char *args[]);
void CmdSetStop( char *args[]);
void CmdInitTape( char *args[]);
void CmdRewindTape( char *args[]);
void CmdUnloadTape( char *args[]);
void CmdReadForward( char *args[]);
void CmdSkip( char *args[]);
void CmdSpace( char *args[]);
void CmdTapeDebug( char *args[]);
void CmdCreateImage( char *args[]);
void CmdWriteImage( char *args[]);
void CmdSet1600( char *args[]);
void CmdSet6250( char *args[]);
#endif
