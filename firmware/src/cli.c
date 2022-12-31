#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "license.h"

#include "filedef.h"
#include "globals.h"
#include "rtcsubs.h"
#include "comm.h"
#include "sdiosubs.h"
#include "cli.h"
#include "gpiodef.h"
#include "ymodem.h"
#include "filesub.h"
#include "tapeutil.h"

#define MAX_ARGS 5		// maximum number of arguments to command

typedef struct _command_list_
{
  char *Name;			// Command name
  char *Description;		// for HELP
  void (*Func)( char *args[]);
} COMMAND_LIST;

//  Prototypes

static void SetDateTime( char *args[]);
static void ShowHelp( char *args[]);

static const COMMAND_LIST Commands[] =
{
 { "HELP", 	
  "Show all commands or HELP cmd for specifc command", 	ShowHelp},  // here
 { "DIR",	"Show files on SD card",	ShowFiles  	},  // filesub
 { "DEL",	"Delete (file name)",		DeleteFile 	},  // filesub
 { "TIME",	"Set date and Time",		SetDateTime	},  // here
 { "MOUNT",	"(Re-)initialize SD interface", MountSD		},  // filesub
 { "CD",	"Change/show directory",	ChangeDir	},  // filesub
 { "MKDIR",	"Make a directory",		MakeDir		},  // filesub
 { "PUT",	"Send YMODEM (file name)",	SendFile	},  // filesub
 { "GET",	"Get a remote file",		GetFile		},  // filesub
 { "STATUS",	"Show detailed tape status",	CmdShowStatus  	},  // tapeutil
 { "REWIND",	"Rewind tape",			CmdRewindTape	},  // tapeutil
 { "READ",    	
   "Read tape to image <file> [N] = no rewind",	CmdCreateImage  },  // tapeutil

 { "WRITE",	
   "Write tape from <file> [N] = no rewind", 	CmdWriteImage	},  // tapeutil
 { "DUMP",	"Read and display tape block",	CmdReadForward  },  // tapeutil
 { "INIT",	"Initialize tape interace",	CmdInitTape	},  // tapeutil
 { "ADDRESS",	
   "Set tape drive address 0-7",		CmdSetAddr	},  // tapeutil
 { "SKIP",	
   "Skip n blocks (+=forward, -=backward)",	CmdSkip		},  // tapeutil
 { "SPACE", 
    "Space tape n files (+=forward, -=backward)", CmdSpace	},  // tapeutil
 { "UNLOAD",	"Unload tape and go offline",	CmdUnloadTape   },  // tapeutil
 { "STOP",	
   "Set stop: # of filemarks [E if error] ", 	CmdSetStop	},  // tapeutil
 { "DEBUG",	"Set command register [value]",	CmdTapeDebug	},  // tapeutil

// { "SETPE",	"Set 1600 PE mode",		CmdSet1600	},  // tapeutil
// { "SETGCR",	"Set 6250 GCR mode",		CmdSet6250	},  // tapeutil
 
// { "RETRIES",	"Set number of retries",	CmdSetRetries	},  // tapeutil
 { 0, 0, NULL}
};

//*	ProcessCommand - Get and process command input
//	----------------------------------------------
//
//	Get input, scan for command and return arguments.
//

bool ProcessCommand( void)
{

#define CMD_DELIMS " ,;\t\r\n"

  static char 
    *arglist[ MAX_ARGS];
  char 
    *cmd, *ch;			// command and scratch
  int i;
 
  while ( true)
  {    
    ShowBriefStatus();		// Show brief drive status

    Uprintf( "? ");		// prompt     
    Ugets( (char *) Buffer, 256);
    cmd = strtok( (char *) Buffer, CMD_DELIMS);	// get command name
    if (! *cmd)
      continue;			// ignore null entries
  
//  Fold the command to uppercase.

    for ( ch = cmd; *ch; ch++)
      *ch = toupper( *ch);  
 
 //  Tokenize the rest of the string. 
  
    for (i = 0; i < MAX_ARGS; i++)
    {
      arglist[i] = strtok(NULL, CMD_DELIMS);	// get the arguments
    } // grab arguments  

//  Now classify the commands.  Fold them to uppercase.

    for ( i = 0; Commands[i].Name; i++)
    {
      if (!strcmp( Commands[i].Name, cmd)) 
      {	// got a hit, call the processing function
        (*Commands[i].Func)(arglist);	// process it
        break;
      }	// got a hit
    } // look through the list
   
    if ( !Commands[i].Name)
    {
      Uprintf( "\nERROR - Don\'t understand %s\n", cmd);
    }
  } // do forever   
} // ProcessCommand

//  	Help command - Display help context..
//	------------------------------------
//
//	If there's an argument, display the help context,
//	otherwise, display the list.
//

#define HELP_TAB_LENGTH 10		// space between commands

static void ShowHelp( char *args[])
{

  int i;
  char *ch;

  if (args[0])
  { // display help for a given command.

//  Fold the command to uppercase.

    for ( ch = args[0]; *ch; ch++)
      *ch = toupper( *ch);  
  
    for ( i = 0; Commands[i].Name; i++)
    {
      if ( !strcmp( args[0], Commands[i].Name))
        break;
     } // for
     if ( Commands[i].Name)
       Uprintf( "\n %10s %s\n", Commands[i].Name, Commands[i].Description);
     else
       Uprintf( "\n Command %s not found!\n", args[0]);
  } else {
    Uprintf( "\nLegal commands are:\n\n");
    for ( i = 0; Commands[i].Name; i++)
    {
      if ( (i%5) == 0)
        Uprintf( "\n");
      Uprintf( "%15s", Commands[i].Name);
     }
     Uprintf("\n");    
     Uprintf( "Enter HELP <command name> for specific informantion.\n");
  } // print a summary
} // ShowHelp

//	SetDateTime - Set date and time.
//	--------------------------------
//

static void SetDateTime( char *args[])
{

  (void) args;
  
  SetRTCTime();
  return;
} // SetDateTime

  