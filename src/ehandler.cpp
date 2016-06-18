// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: ehandler.cpp 
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/14/1996  
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description and Details ------------- // 
// ----------------------------------------------------------- // 
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.
 
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  
USA

The Ehandler class is used to display exceptions that
occur at run-time. 

Changes:
==============================================================
07/17/2001: Corrected BCC32 syntax error in EHandler::Message()
function. 

05/27/2002: Added break to terminating case switches in the 
EHandler::SignalException() function to eliminate flow control 
warnings.

10/10/2002: Modified the EHandler::Message() function to flush
the console buffers with each write operation.

10/10/2002: Modified the EHandler::DisplayException() function to 
flush the console buffers with each write operation.

01/03/2003: Modified the display type preprocessor directives
to allow the library to compile without a display type.

01/11/2003: Added support for WIN32 API error messages via
the __WIN32_ERROR_MSG__ preprocessor directive. The change
affects the EHandler::DisplayException(), EHandler::Message(), 
and EHandler::Terminate() functions.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <string.h>
#include "ehandler.h"

#if defined (__wxWIN168B__)
#include "wxincs.h"

#elif defined (__wxWIN201__) || (__wxWIN291__)
#include "wx2incs.h"

#elif defined (__wxWIN302__)
#include "wx3incs.h"

#elif defined (__CONSOLE__)
#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include <stdlib.h>

#elif defined (__CURSES__)
#include <stdlib.h>
#include "terminal.h"

#elif defined (__WIN32_ERROR_MSG__)
#include <windows.h>

#else
// Allow the library to be compiled without a display type
#endif

const char *MessageBoxCaption = "Program Error";

// Global exception handler pointer initialization 
GXDLCODE_API EHandler ExceptionHandler;
GXDLCODE_API EHandler *ProgramError = &ExceptionHandler;

void EHandler::DisplayException(const char *mesg)
{
#if defined (__wxWIN168B__)
  wxMessageBox((char *)mesg, (char *)MessageBoxCaption, wxOK|wxCENTRE);

#elif defined (__wxWIN201__) || (__wxWIN291__)
  wxString message(mesg);
  wxString caption(MessageBoxCaption);
  wxMessageBox(message, caption, wxOK|wxCENTRE);

#elif defined (__wxWIN302__)
  wxString message(mesg);
  wxString caption(MessageBoxCaption);
  wxMessageBox(message, caption, wxOK|wxCENTRE);

#elif defined (__CURSES__)
  terminal->ClearScreen();
  terminal->Write(mesg, 0, 0);
  terminal->AnyKey(0, 2);

#elif defined (__CONSOLE__)
  GXSTD::cout << "\n" << mesg << "\n" << GXSTD::flush;

#elif defined (__WIN32_ERROR_MSG__)
  MessageBox( NULL, (char *)mesg, (char *)MessageBoxCaption, MB_OK );

#else
// Allow the library to be compiled without a display type
#endif
}

void EHandler::SignalException(const char *mesg, int Level)
{
  // By default a message will be displayed
  DisplayException(mesg); 

  switch(Level) {
    case FATAL:    // Fatal error, terminate program immediately
      Terminate();
      break;

    case DISPLAY: // Dummy handler that does nothing
      return;
      
    default:
      Terminate();
      break;
  }
}

void EHandler::Message(const char *mesg1, const char *mesg2, 
		       const char *mesg3)
{
#if defined (__wxWIN168B__)
  int len = strlen(mesg1) + strlen(mesg2) + strlen(mesg3);
  char *comp = new char[len+1];
  comp[len] = '\0';
  strcpy(comp, mesg1);
  strcat(comp, mesg2);
  strcat(comp, mesg3);
  wxMessageBox(comp, "Program Message", wxOK|wxCENTRE);

  // PC-lint 05/27/2002: Avoid inappropriate de-allocation error,
  // but comp is not a user defined type so there is no destructor
  // to call for the array members.
  delete[] comp;

#elif defined (__wxWIN201__) || (__wxWIN291__)
  wxString message(mesg1);
  wxString caption("Program Message");
  message += mesg2;
  message += mesg3;
  wxMessageBox(message, caption, wxOK|wxCENTRE|wxICON_EXCLAMATION);

#elif defined (__wxWIN302__)
  wxString message(mesg1);
  wxString caption("Program Message");
  message += mesg2;
  message += mesg3;
  wxMessageBox(message, caption, wxOK|wxCENTRE|wxICON_EXCLAMATION);

#elif defined (__CONSOLE__)
  GXSTD::cout << "\n" << GXSTD::flush;
  if(mesg1) GXSTD::cout << mesg1 << GXSTD::flush;
  if(mesg2) GXSTD::cout << mesg2 << GXSTD::flush;
  if(mesg3) GXSTD::cout << mesg3 << GXSTD::flush;
  GXSTD::cout << "\n" << GXSTD::flush;
  
#elif defined (__WIN32_ERROR_MSG__)
  int len = lstrlen(mesg1) + lstrlen(mesg2) + lstrlen(mesg3);
  char *comp = new char[len+1];
  comp[len] = '\0';
  strcpy(comp, mesg1);
  strcat(comp, mesg2);
  strcat(comp, mesg3);
  MessageBox( NULL, comp, "Program Message", MB_OK ) ;
  delete comp;

#else
// Allow the library to be compiled without a display type
#endif
}

void EHandler::Terminate()
{
#if defined (__wxWIN168B__)
  // Terminate the program using the wxExit() function.
  wxExit();
  // Exits application after calling wxApp::OnExit. Should only
  // be used in an emergency: normally the top-level frame should
  // be deleted (after deleting all other frames) to terminate the
  // application. 

#elif defined (__wxWIN201__) || (__wxWIN291__)
  // Terminate the program using the wxExit() function.
  wxExit();

#elif defined (__wxWIN302__)
  // Terminate the program using the wxExit() function.
  wxExit();

#elif defined (__CURSES__)
  // Terminate the program abnormally
  terminal->finish();
  exit(FatalErrorLevel);

#elif defined (__CONSOLE__)
  // Terminate the program abnormally
  exit(FatalErrorLevel);

#elif defined (__WIN32_ERROR_MSG__)
  // Terminate the program abnormally
  exit(FatalErrorLevel);

#else
// Allow the library to be compiled without a display type
#endif
}

void EHandler::TrapException(AF ActionFunction)
{
  // Call the user defined routine
  (*ActionFunction)();
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
