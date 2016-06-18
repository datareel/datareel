// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: ehandler.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/14/1996  
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ---------- Include File Description and Details  ---------- // 
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
*/
// ----------------------------------------------------------- //   
#ifndef __GX_EHANDLER_HPP__
#define __GX_EHANDLER_HPP__

#include "gxdlcode.h"

// Macros used to select display types for error messaging
// NOTE: These preprocessor directives are usually defined during 
// compile time using the -D compiler flag. Alternately they may 
// be defined in the "gxdlcode.h" include file.
// ===========================================================
// #ifndef __wxWIN168B__ 
// #define __wxWIN168B__ // wxWindows 1.68B GUI 
// #endif

// wxWindows 2.X.X GUI 
// #ifndef __wxWIN201__
// #define __wxWIN201__ 
// #endif
// #ifndef __wxWIN291__
// #define __wxWIN291__
// #endif

// wxWindows 3.X.X GUI 
// #ifndef __wxWIN302__
// #define __wxWIN302__ 
// #endif

// #ifndef __CONSOLE__ 
// #define __CONSOLE__   // Console (stdout) display
// #endif

// #ifndef __CURSES__
// #define __CURSES__    // Curses port for terminal sessions
// #endif

// #ifndef __WIN32_ERROR_MSG__ 
// #define __WIN32_ERROR_MSG__ // WIN32 API error messages
// #endif
// ===========================================================

// Constants
const int FatalErrorLevel = 1; // Return value to Operating System

// Type definitions
typedef void (*AF)(); // Function pointer for action function

class GXDLCODE_API EHandler
{
  enum { // Exception Handler error levels
    FATAL = 0x0010,  // Fatal error, terminate program immediately
    DISPLAY = 0x0020 // Dummy handler that does nothing
  };
    
public:
  EHandler() { }
  ~EHandler() { };

public:
  // Display exception, exit the program, do nothing, or handle exception
  void SignalException(const char *mesg, int Level = EHandler::DISPLAY);
  void DisplayException(const char *mesg);

  // Display a program message
  void Message(const char *mesg1=" ", const char *mesg2=" ", 
	       const char *mesg3=" ");
  
  // Exception handler used to handle user defined routines to trap
  // a program error 
  void TrapException(AF ActionFunction);
  
  // Terminate the program
  void Terminate();
};

extern GXDLCODE_API EHandler ExceptionHandler;
extern GXDLCODE_API EHandler *ProgramError; // Global exception handler pointer

#endif  // __GX_EHANDLER_HPP__ 
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
