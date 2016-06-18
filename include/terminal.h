// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: terminal.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 03/21/1997 
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

This is a terminal interface designed to be portable between
DOS and UNIX systems. On UNIX systems the "curses" library is
used to create terminal independent code. On MSDOS/Windows95
systems the ANSI.SYS driver is used to simulate the basic
functions of the "curses" library.

Changes:
==============================================================
07/19/2001: Added HPUX 11 preprocessor directives to this 
include file to avoid conflicts when building HPUX 11 static
libraries.

09/30/2005: This code will only be enabled if the __USE_TERMINAL_IO__
macro is defined during the library build. 
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_TERMINAL_HPP__
#define __GX_TERMINAL_HPP__

#include "gxdlcode.h"

// This directive must be defined to use the terminal I/O class
#if defined (__USE_TERMINAL_IO__)

#include <stdio.h>
#include <stdlib.h>

// HPUX 11 pre-processor directives
#if defined (__HPUX11__)
#ifndef _XOPEN_SOURCE_EXTENDED 
#define _XOPEN_SOURCE_EXTENDED
#endif
#ifndef __HP_CURSES
#define __HP_CURSES
#endif
#endif

#if defined (__DOS__) || defined (__WIN32__)
#include <conio.h> // Console I/O library

#elif defined (__UNIX__)
// NOTE: Redhat Linux 5.2-7.2 requires ncurses-devel-4.2-10.i386.rpm
#include <curses.h> // Terminal independent I/O library

#else
#error You must define a target platform: __DOS__ __WIN32__ or __UNIX__
#endif

// Key definitions
const int termEscapeKey = 27;

#define CONTROL(c) ((c) & 037) 

// Screen (C)oordinates class
class GXDLCODE_API Coords
{
public:
  Coords() { xpos = ypos = 0; }
  Coords(int x, int y) { xpos = x; ypos = y; }
  ~Coords() { }
  Coords(const Coords &ob) { xpos = ob.xpos ; ypos = ob.ypos; }
  Coords &operator=(const Coords &ob) {
    xpos = ob.xpos ; ypos = ob.ypos; return *this;
  } 
  
public:
  int XPos() { return xpos; } // Current x coordinate
  int YPos() { return ypos; } // Current y coordinate 
  int XNext() { int x = xpos + 1; return x; } 
  int YNext() { int y = ypos + 1; return y; }
  int XPrev() { int x = xpos - 1; return x; }
  int YPrev() { int y = ypos - 1; return y; }
  int XOffset(int x) { xpos = xpos + x; return xpos; } 
  int YOffset(int y) { ypos = ypos + y; return ypos; }
  void SetXY(int x, int y) { xpos = x; ypos = y; }
  void SetX(int x) { xpos = x; }
  void SetY(int y) { ypos = y; }

private:
  int xpos;
  int ypos;
};

// ANSI (T)erminal class
class GXDLCODE_API Terminal
{
public:
  Terminal() { putback = 0; }
  ~Terminal() { }

public: // Standard dialogs
  int YesNo(int x = -1, int y = -1);
  int YesNo(const char *s, int x = -1, int y = -1);
  int GetYesNo();
  void AnyKey(int x = -1, int y = -1);
  void AnyKey(const char *s, int x = -1, int y = -1);
  
public: // Terminal setup and screen operations
  void init();
  void finish();
  char *GetTerm() { return getenv("TERM"); }
  int MaxLines() { return maxlines; } 
  int MaxLines() const { return maxlines; }
  int MaxCols() { return maxcols; }
  int MaxCols() const { return maxcols; }
  void SetMaxLines(int lines);
  void SetMaxCols(int cols);
  void MoveCursor(int x, int y) const;
  void MoveCursor(int x, int y);
  void ClearScreen() const;
  void ClearScreen();
  void ClearLine(int x = -1, int y = -1) const;
  void ClearLine(int x = -1, int y = -1);
  int Center(const char *s) const; // Return center coordinate for a string
  int Center(char *s);
  int Right(const char *s) const;  // Return right justifed coordinate
  int Right(char *s);  
  int ScreenCenter(const int offset) const; // Return screen's center
  int ScreenCenter(int offset); 
  
public: // Output functions
  void Write(const char c, int x = -1, int y = -1) const;
  void Write(char c, int x = -1, int y = -1);
  void Write(const unsigned char c, int x = -1, int y = -1) const;
  void Write(unsigned char c, int x = -1, int y = -1);
  void Write(const char *s, int x = -1, int y = -1) const;
  void Write(char *s, int x = -1, int y = -1);
  void Write(const long val, int x = -1, int y = -1) const;
  void Write(long val, int x = -1, int y = -1);
  void Write(const int val, int x = -1, int y = -1) const;
  void Write(int val, int x = -1, int y = -1);
  void Write(double val, int x = -1, int y = -1); 
  void Write(const double val, int x = -1, int y = -1) const; 
  void Write(float val, int x = -1, int y = -1); 
  void Write(const float val, int x = -1, int y = -1) const; 
  void StatusLine(const char *s) const;
  void StatusLine(char *s);

  // The standout mode does not work the same on every terminal type.
  // In order to maintain portability avoid using these functions in
  // code that needs to maintain a universal appearance when executed
  // across multiple platforms.
  void StandOut(const char *s, int x = -1, int y = -1) const;
  void StandOut(char *s, int x = -1, int y = -1);
  
public: // Input functions
  int KBWait() const;
  unsigned char GetChar();
  void GetString(char *string, int x = -1, int y = -1);
  void GetPassword(char *string, int x = -1, int y = -1);
  void PutBack(char c);
  int GetInt(int x = -1, int y = -1);
  long GetLong(int x = -1, int y = -1);
  double GetFloat(int x = -1, int y = -1);

private:
  char putback;
  int maxlines;
  int maxcols;
};

extern GXDLCODE_API Terminal I_TERM;    // Independent Terminal type object
extern GXDLCODE_API Terminal *terminal; // Global terminal pointer

#if defined (__DOS__) || defined (__WIN32__)
// ===============================================================
// Stand alone functions used to port this code from UNIX to DOS,
// Windows 3.11, and Windows 95. These functions are used to
// simulate the basic functionality of the vast CURSES library.
// All of these functions rely on the ANSI.SYS driver installed
// in the "config.sys" file: DEVICEHIGH=C:\WINDOWS\COMMAND\ANSI.SYS
// in Windows 95 or DEVICEHIGH=C:\DOS\ANSI.SYS in DOS/Windows 3.11.
// ===============================================================
// chtype is the type used to store a character together with attributes.
// It can be set to "char" to save space, or "long" to get more attributes.
#ifdef	CHTYPE
typedef	CHTYPE chtype;
#else
typedef unsigned long chtype;
#endif // CHTYPE 

extern GXDLCODE_API int LINES;
extern GXDLCODE_API int COLS;

// TRUE and FALSE get defined so many times, let's not get in the 
// way of other definitions.
#ifndef TRUE
#define	TRUE	1
#endif
#ifndef FALSE
#define	FALSE	0
#endif
#ifndef ERR
#define	ERR	-1
#endif
#ifndef OK 
#define	OK	0
#endif

GXDLCODE_API int addch(const chtype c);
GXDLCODE_API int addstr(const char *s);
GXDLCODE_API int beep();
GXDLCODE_API int clear();
GXDLCODE_API int endwin();
GXDLCODE_API int move(int y, int x);
GXDLCODE_API int refresh();
GXDLCODE_API int standend();
GXDLCODE_API int standout();

// ===============================================================
// The following code is used for PC enhancements, used to emulate
// the effect of the video attribute functions in the curses library.
// All of the escape codes are compatible with vt100 terminals.
GXDLCODE_API void UnderlineText();
GXDLCODE_API void BoldText();
GXDLCODE_API void ReverseVideo();
GXDLCODE_API void NormalText();
// ===============================================================
#endif // __DOS__

#endif // __USE_TERMINAL_IO__

#endif  // __GX_TERMINAL_HPP__ 
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //



