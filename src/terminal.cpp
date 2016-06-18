// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: terminal.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 03/21/1997 
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

This is a terminal interface designed to be portable between
DOS and UNIX systems. On UNIX systems the "curses" library is
used to create terminal independent code. On MSDOS/Windows95
systems the ANSI.SYS driver is used to simulate the basic
functions of the "curses" library.

Changes:
==============================================================
03/24/2002: The DOS and Windows emulation no longer uses the 
C++ iostream to write to the console. The stdio library is now 
used to write to the console. 

07/05/2003: The DOS and Windows emulation modified to perform
an fflush(stdout) following all printf() calls.

09/30/2005: This code will only be enabled if the __USE_TERMINAL_IO__
macro is defined during the library build. 
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

// This directive must be defined to use the terminal I/O class
#if defined (__USE_TERMINAL_IO__)

#include <ctype.h>
#include <string.h>
#include "terminal.h"

#if defined (__UNIX__)
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#endif

#ifdef __BCC32__
#pragma warn -8071
#endif

GXDLCODE_API Terminal I_TERM;              // Independent Terminal type object
GXDLCODE_API Terminal *terminal = &I_TERM; // Global terminal pointer

void Terminal::GetString (char *string, int x, int y)
// This function allows the user to enter a single string.
// It was added to echo each character without having to
// to turn the echo on. This will allow the user to edit
// the entry using the backspace or other defined keys.
{
  char ch;
  int charcount = 0;
  
  if (x != -1) move (y, x);

  while (1) {
    // Ignore BCC32 conversion warning
    ch = getch();
    if (ch == '\n' || ch == '\r' || ch == CONTROL('c')) {
      *string = 0;
      return;
    }

    if (ch == '\b' || ch == CONTROL('d')) { // Delete keys 
      if (charcount > 0) {
	string--;
	charcount--;
	Write('\b');
	Write(' ');
	Write('\b');
      }
    }
    else {
      *string++ = ch;
      charcount++;
      Write(ch);
    }
    refresh();
  }
}

void Terminal::GetPassword (char *string, int x, int y)
// This function allows the user to enter a password
// and echo an asterisk for each character typed.
{
  char ch;
  int charcount = 0;
  
  if (x != -1) move (y, x);

  while (1) {
    // Ignore BCC32 conversion warning
    ch = getch();
    if (ch == '\n' || ch == '\r' || ch == CONTROL('c')) {
      *string = 0;
      return;
    }

    if (ch == '\b' || ch == CONTROL('d')) { // Delete keys 
      if (charcount > 0) {
	string--;
	charcount--;
	Write('\b');
	Write(' ');
	Write('\b');
      }
    }
    else {
      *string++ = ch;
      charcount++;
      Write('*');
    }
    refresh();
  }
}

int Terminal::GetInt(int x, int y)
// Get a signed int value.
{
  char buf[100];
  GetString(buf, x, y);
  // PC-lint 09/14/2005: Function is considered dangerous
  return atoi(buf);
}

long Terminal::GetLong(int x, int y)
// Get a signed long value.
{
  char buf[100];
  GetString(buf, x, y);
  // PC-lint 09/14/2005: Function is considered dangerous
  return atol(buf);
}

double Terminal::GetFloat(int x, int y)
// Get a floating point value
{
  char buf[100];
  GetString(buf, x, y);
  // PC-lint 09/14/2005: Function is considered dangerous
  return atof(buf);
}

int Terminal::YesNo(int x, int y)
{
  if(x == -1) 
    Write((char *)" <Do you wish to continue (y/n)> "); // Same line message
  else
    Write((char *)"Do you wish to continue (y/n)...", x, y);

  char c = ' ';
  while (c != 'y' && c != 'n')	{
    c = GetChar();
    // Ignore BCC32 conversion warning
    c = tolower(c);
  }
  return c == 'y' ? 1: 0; // return true if users answers yes
}

int Terminal::YesNo(const char *s, int x, int y)
{
  if(x == -1) 
    Write(s); // Same line message
  else
    Write(s, x, y);

  char c = ' ';
  while (c != 'y' && c != 'n')	{
    c = GetChar();
    // Ignore BCC32 conversion warning
    c = tolower(c);
  }
  return c == 'y' ? 1: 0; // return true if users answers yes
}

int Terminal::GetYesNo()
// Wait for y/n reply with no prompt.
{
  char c = ' ';
  while (c != 'y' && c != 'n')	{
    c = GetChar();
      // Ignore BCC32 conversion warning
    c = tolower(c);
  }
  return c == 'y' ? 1: 0; // return true if users answers yes
}

void Terminal::PutBack(char c)
// Put back a keyboard character
{
  putback = c;
}

int Terminal::Center(const char *s) const
// Returns the center coordinate for a string.
{
  int len = strlen(s);
  if(len > MaxCols()-1) len = MaxCols()-1;
  int mid = MaxCols()/2 - 1;
  int half = len/2;
  return mid - half;
}

int Terminal::Center(char *s)
// Returns the center coordinate for a string.
{
  int len = strlen(s);
  if(len > MaxCols()-1) len = MaxCols()-1;
  int mid = MaxCols()/2 - 1;
  int half = len/2;
  return mid - half;
}

int Terminal::ScreenCenter(const int offset) const
// Returns the center coordinate for the screen.
{
  int buf = offset;
  if(buf > MaxLines()-1) buf = MaxLines()-1;
  int mid = MaxLines()/2 - 1;
  int half = buf/2;
  return mid - half;
}

int Terminal::ScreenCenter(int offset)
// Returns the center coordinate for the screen.
{
  if(offset > MaxLines()-1) offset = MaxLines()-1;
  int mid = MaxLines()/2 - 1;
  int half = offset/2;
  return mid - half;
}

int Terminal::Right(const char *s) const
// Returns the right justified coordinate for a string.
{
  int len = strlen(s);
  if(len > MaxCols()-1) len = MaxCols()-1;
  int end = MaxCols()-1;
  return end - len;
}

int Terminal::Right(char *s)
// Returns the right justified coordinate for a string.
{
  int len = strlen(s);
  if(len > MaxCols()-1) len = MaxCols()-1;
  int end = MaxCols()-1;
  return end - len;
}

void Terminal::StatusLine(const char *s) const
// Display a status line in normal text mode.
{
  Write(s, 0, MaxLines()-1);
  int len = strlen(s);
  while (len++ < MaxCols()-1) Write(' ');
}

void Terminal::StatusLine(char *s) 
// Display a status line in normal text mode.
{
  Write(s, 0, MaxLines()-1);
  int len = strlen(s);
  while (len++ < MaxCols()-1) Write(' ');
}

void Terminal::ClearLine(int x, int y) const
{
  int i = 0;
  if(x == -1) 
    while (i++ < MaxCols()-1) Write(' ');
  else {
    MoveCursor(x, y);
    while (i++ < MaxCols()-1) Write(' ');
  }
}

void Terminal::ClearLine(int x, int y)
{
  int i = 0;
  if(x == -1) 
    while (i++ < MaxCols()-1) Write(' ');
  else {
    MoveCursor(x, y);
    while (i++ < MaxCols()-1) Write(' ');
  }
}

void Terminal::MoveCursor(int x, int y) const
{
  move(y, x);
  refresh();
}

void Terminal::MoveCursor(int x, int y)
{
  move(y, x);
  refresh();
}

void Terminal::ClearScreen() const
{
  clear();
}

void Terminal::ClearScreen() 
{
  clear();
}

void Terminal::AnyKey(int x, int y) 
// Prompt the user for any key to continue
{
  if(x == -1) {
    Write((char *)" <Press any key to continue> "); // Same line message
    GetChar();
    return;
  }
  Write((char *)"Press any key to continue...", x, y);
  GetChar();
}

void Terminal::AnyKey(const char *s, int x, int y) 
// Prompt the user for any key to continue
{
  if(x == -1) {
    Write(s); // Same line message
    GetChar();
    return;
  }
  Write(s, x, y);
  GetChar();
}

void Terminal::Write(const unsigned char c, int x, int y) const
// Write a unsigned character to the screen.
{
  if (x != -1) move (y, x);
#if defined ( __BCC32__)
  addch(c);
  refresh();
#else
  char s[2];
  s[0] = c;    // Copy char into a string buffer
  s[1] = '\0'; // Null terminate the string
  addstr(s);
  refresh();
#endif
}

void Terminal::Write(unsigned char c, int x, int y) 
// Write a unsigned character to the screen.
{
  if (x != -1) move (y, x);

#if defined (__BCC32__)
  addch(c);
  refresh();
#else
  char s[2];
  s[0] = c;    // Copy char into a string buffer
  s[1] = '\0'; // Null terminate the string
  addstr(s);
  refresh();
#endif
}

void Terminal::Write(const char c, int x, int y) const
// Write a signed character to the screen.
{
  if (x != -1) move (y, x);
#if defined (__BCC32__)
  addch(c);
  refresh();
#else
  char s[2];
  s[0] = c;    // Copy char into a string buffer
  s[1] = '\0'; // Null terminate the string
  addstr(s);
  refresh();
#endif
}

void Terminal::Write(char c, int x, int y) 
// Write a signed character to the screen.
{
  if (x != -1) move (y, x);
#if defined (__BCC32__)
  addch(c);
  refresh();
#else
  char s[2];
  s[0] = c;    // Copy char into a string buffer
  s[1] = '\0'; // Null terminate the string
  addstr(s);
  refresh();
#endif
}

void Terminal::Write(const char *s, int x, int y) const
// Write a single string to the screen. 
{
  if (x != -1) move (y, x);

#ifdef __SOLARIS__
  addstr((char *)s);
#else
  addstr(s);
#endif
  
  refresh();
}

void Terminal::Write(char *s, int x, int y) 
// Write a single string to the screen. 
{
  if (x != -1) move (y, x);
  addstr(s);
  refresh();
}

void Terminal::Write(const long val, int x, int y) const
// Write a signed long int to the screen.
{
  char buf[100];
  // 05/30/2002: Using type cast to int type to eliminate
  // warning: int format, long int arg (arg 3) when compiled
  // under Linux gcc.
  sprintf(buf, "%d", (int)val);
  Write((const char*)buf, x, y);
}

void Terminal::Write(long val, int x, int y)
// Write a signed long int to the screen.
{
  char buf[100];

  // 05/30/2002: Using type cast to int type to eliminate
  // warning: int format, long int arg (arg 3) when compiled
  // under Linux gcc.
  sprintf(buf, "%d", (int)val);
  Write(buf, x, y);
}

void Terminal::Write(const int val, int x, int y) const
// Write a signed int to the screen.
{
  char buf[100];
  sprintf(buf, "%d", val);
  Write((const char*)buf, x, y);
}

void Terminal::Write(int val, int x, int y)
// Write a signed int to the screen.
{
  char buf[100];
  sprintf(buf, "%d", val);
  Write(buf, x, y);
}

void Terminal::Write(const double val, int x, int y) const 
// Write a floating point value to the screen.
{
  char buf[100];
  sprintf(buf, "%.2f", val);
  Write((const char*)buf, x, y);
}

void Terminal::Write(double val, int x, int y) 
// Write a floating point value to the screen.
{
  char buf[100];
  sprintf(buf, "%.2f", val);
  Write(buf, x, y);
}

void Terminal::Write(const float val, int x, int y) const 
// Write a floating point value to the screen.
{
  char buf[100];
  sprintf(buf, "%.2f", val);
  Write((const char*)buf, x, y);
}

void Terminal::Write(float val, int x, int y) 
// Write a floating point value to the screen.
{
  char buf[100];
  sprintf(buf, "%.2f", val);
  Write(buf, x, y);
}

void Terminal::StandOut(const char *s, int x, int y) const
// Write text in stand out mode.
{
  if (x != -1) move (y, x);
  standout();

#ifdef __SOLARIS__
  addstr((char *)s);
#else
  addstr(s);
#endif
  
  standend();
  refresh();
}

void Terminal::StandOut(char *s, int x, int y) 
// Write text in stand out mode.
{
  if (x != -1) move (y, x);
  standout();
  addstr(s);
  standend();
  refresh();
}

unsigned char Terminal::GetChar() 
// Input a keyboard character (unbuffered) with out waiting
// for the RETURN key.
{
  int c;
  if(putback) {
    c = putback;
    putback = 0;
  }
  else {
    c = getch();
    if (c == 0) // Convert function key
      c = getch() | 0x80;
    else // Strip scan code
      c &= 0xff;
  }
  return (unsigned char)(c);
}

int Terminal::KBWait() const
// Test for a keyboard character waiting. Continue processing until
// the user interrupts by hitting a key.
{
#if defined (__DOS__) || defined (__WIN32__)
  return kbhit();

#elif defined (__UNIX__)
  fd_set readfds, writefds, exceptfds;
  struct timeval timeout;
  static struct termios otty, ntty;
  int ret;

  // Create proper environment for select() 
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);
  FD_SET(fileno(stdin), &readfds);

  // We shall specify 0.5 sec as the waiting time 
  timeout.tv_sec  = 0;	 // 0 seconds 
  timeout.tv_usec = 500; // 500 microseconds 

  int fd = 0;
  
  // Put tty in raw mode 
  tcgetattr(fd, &otty); // Get the current options for the port
  tcgetattr(fd, &ntty);
  ntty.c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL);
  ntty.c_lflag &= ~ICANON;
  ntty.c_lflag |= ISIG;
  ntty.c_cflag &= ~(CSIZE|PARENB);
  ntty.c_cflag |= CS8;
  ntty.c_iflag &= (ICRNL|ISTRIP);
  ntty.c_cc[VMIN] = 1;
  ntty.c_cc[VTIME] = 1;
  tcsetattr(fd, TCSANOW, &ntty);

  // Do a select 
  ret = select(1, &readfds, &writefds, &exceptfds, &timeout);

  // Reset the tty back to its original mode 
  tcsetattr(fd, TCSANOW, &otty);
  
  return ret;

#else
#error You must define a target platform: __DOS__ __WIN32__ or __UNIX__
#endif
}

void Terminal::init()
{
#if defined (__DOS__) || defined (__WIN32__)
  // Simulate an initscr() call 
  printf("\033[m"); // Normal text attributes
  fflush(stdout);
  clear();
  maxcols = COLS;
  maxlines = LINES;
  move(0, 0);
  refresh();

#elif defined (__UNIX__)
// Removed: causes core dump if term variable is set incorrectly (ie: dec-vt320)
// newterm(getenv("TERM"), stdout, stdin);

  initscr();            // initialize the curses library 
  keypad(stdscr, TRUE); // enable keyboard mapping 
  idlok(stdscr, TRUE);  // enable terminal insert and delete line features 
  nonl();               // tell curses not to do NL->CR/NL on output 
  noecho();             // don't echo input 
  raw();                // Allows the program to mask Ctrl-C

  // Curses input modes
  // cbreak();   // Send all characters to program except Ctrl-C,S,Q, and Z
  // nocbreak(); // Nothing is sent to the program until the users hits return
  // raw();      // All characters are sent immediately to the program 
  // noraw();    // Turn off raw mode

  maxcols = COLS;
  maxlines = LINES;

#else
#error You must define a target platform: __DOS__ __WIN32__ or __UNIX__
#endif
}

void Terminal::finish()
{
  clear();
  endwin();
}

void Terminal::SetMaxLines(int lines)
{
  if(lines > LINES)
    maxlines = LINES;
  else
    maxlines = lines;
}

void Terminal::SetMaxCols(int cols)
{
  if(cols > COLS)
    maxcols = COLS;
  else
    maxcols = cols;
}

#if defined (__DOS__) || defined (__WIN32__)
// ===============================================================
// Stand alone functions used to port this code from UNIX to DOS,
// Windows 3.11, and Windows 95. These functions are used to
// simulate the basic functionality of the vast CURSES library.
// All of these functions rely on the ANSI.SYS driver installed
// in the "config.sys" file: DEVICEHIGH=C:\WINDOWS\COMMAND\ANSI.SYS
// in Windows 95 or DEVICEHIGH=C:\DOS\ANSI.SYS in DOS/Windows 3.11.
// ===============================================================
GXDLCODE_API int LINES = 25; // PC terminal the maximum screen height 
GXDLCODE_API int COLS  = 80; // PC terminal the maximum screen width

GXDLCODE_API int addch(const chtype c)
{
  printf("%c", c);
  fflush(stdout);
  return OK;
}

GXDLCODE_API int addstr(const char *s)
{
  if(s) printf("%s", s);;
  fflush(stdout);
  return OK;
}

GXDLCODE_API int beep()
{
  printf("%c", '\a');
  return OK;
}

GXDLCODE_API int clear()
{
  printf("\033[2J");
  fflush(stdout);
  return OK;
}

GXDLCODE_API int endwin()
{
  printf("\033[m"); // Normal text attributes
  fflush(stdout);
  clear();
  move(0, 0);
  return OK;
}

GXDLCODE_API int move(int y, int x)
{
  printf("\033[%d;%d%c", (y+1), (x+1), 'H');
  fflush(stdout);
  return OK;
}

GXDLCODE_API int refresh()
{
  return OK;
}

GXDLCODE_API int standend()
{
  printf("\033[m"); // Normal text attributes
  fflush(stdout);
  return OK;
}

GXDLCODE_API int standout()
{
  printf("\033[7m"); // Reverse video text attribute
  fflush(stdout);
  return OK;
}
// ===============================================================
// The following code is used for PC enhancements, used to emulate
// the effect of the video attribute functions in the curses library.
// All of the escape codes are compatible with vt100 terminals.

GXDLCODE_API void UnderlineText()
{
  printf("\033[4m"); 
  fflush(stdout);
}

GXDLCODE_API void BoldText()
{
  printf("\033[1m"); 
  fflush(stdout);
}

GXDLCODE_API void ReverseVideo()
{
  printf("\033[7m"); 
  fflush(stdout);
}

GXDLCODE_API void NormalText()
{
  printf("\033[m"); // Normal text attributes
  fflush(stdout);
}
// ===============================================================
#endif // __DOS__ || __WIN32__

#ifdef __BCC32__
#pragma warn .8071
#endif

#endif // __USE_TERMINAL_IO__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
