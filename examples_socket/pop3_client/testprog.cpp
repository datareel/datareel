// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/23/2001
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

Test program for the embedded POP3 client class.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "gxspop3.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

int CheckError(gxsPOP3Client *pop3)
{
  if(pop3->GetSocketError() != gxSOCKET_NO_ERROR) {
    cout << pop3->SocketExceptionMessage() << "\n" << flush;
    if(pop3->reply_buf[0] != 0) { // Check the reply buffer
      cout << pop3->reply_buf << flush;
    }
    pop3->Close();
    return 0;
  }
  return 1; // No errors reported
}


void ClearInputStream(istream &s)
// Used to clear istream
{
  char c;
  s.clear();
  while(s.get(c) && c != '\n') { ; }
}

void Menu()
{
  cout << "\n";
  cout << "(c, C)    Reset connection" << "\n";
  cout << "(d, D)    Delete a message" << "\n";
  cout << "(h, H, ?) Display this menu" << "\n";
  cout << "(l, L)    List message numbers" << "\n";
  cout << "(r, R)    Retrieve a message" << "\n";
  cout << "(t, T)    Retrieve a message header" << "\n";   
  cout << "(q, Q)    Quit this program" << "\n";
  cout << "\n";
}

void Delete(gxsPOP3Client *pop3)
{
  int message_number = 1;
  cout << "Enter the message number> ";
  cin >> message_number;
  if(!cin) {
    cout << "Invalid input" << "\n";
    return;
  }

  const int sbuf_size = 8192;
  char sbuf[sbuf_size];

  if(pop3->POP3Delete(message_number, sbuf, sbuf_size) != gxSOCKET_NO_ERROR) {
    cout << pop3->SocketExceptionMessage() << "\n" << flush;
    return;
  }

  sbuf[pop3->BytesRead()] = 0;
  cout << sbuf << flush;
}

void List(gxsPOP3Client *pop3)
{
  const int sbuf_size = 8192;
  char sbuf[sbuf_size];
  if(pop3->POP3List(sbuf, sbuf_size) != gxSOCKET_NO_ERROR) {
    cout << pop3->SocketExceptionMessage() << "\n" << flush;
    sbuf[pop3->BytesRead()] = 0;
    cout << sbuf << flush;
    return;
  }
  sbuf[pop3->BytesRead()] = 0;
  cout << sbuf << flush;
}

void Reset(gxsPOP3Client *pop3)
{
  if(pop3->POP3RSet() != gxSOCKET_NO_ERROR) {
    cout << pop3->SocketExceptionMessage() << "\n" << flush;
    cout << pop3->reply_buf << flush;
    return;
  }
  cout << pop3->reply_buf << flush;
}

void Retrieve(gxsPOP3Client *pop3)
{
  int message_number = 1;
  cout << "Enter the message number> ";
  cin >> message_number;
  if(!cin) {
    cout << "Invalid input" << "\n";
    return;
  }

  const int sbuf_size = 8192;
  char sbuf[sbuf_size];
  if(pop3->POP3Retr(message_number, sbuf, sbuf_size) != gxSOCKET_NO_ERROR) {
    cout << pop3->SocketExceptionMessage() << "\n" << flush;
    return;
  }
  sbuf[pop3->BytesRead()] = 0;
  cout << sbuf << flush;
}

void Top(gxsPOP3Client *pop3)
{
  int message_number = 1;
  cout << "Enter the message number> ";
  cin >> message_number;
  if(!cin) {
    cout << "Invalid input" << "\n";
    return;
  }

  const int sbuf_size = 8192;
  char sbuf[sbuf_size];
  if(pop3->POP3Top(message_number, sbuf, sbuf_size) != gxSOCKET_NO_ERROR) {
    cout << pop3->SocketExceptionMessage() << "\n" << flush;
    sbuf[pop3->BytesRead()] = 0;
    cout << sbuf << flush;
    return;
  }
  sbuf[pop3->BytesRead()] = 0;
  cout << sbuf << flush;
}

int main(int argc, char **argv)
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  if(argc < 4) {
    cout << "Usage: " << argv[0] << " server username password" << "\n";
    cout << "server   = Your POP3 mail server" << "\n";
    cout << "username = Your POP3 account user name" << "\n";
    cout << "username = Your POP3 account password" << "\n";
    cout << "\n";
    cout << "Example: " << argv[0] 
	 << " popserver.xyz.com myname mypassword" << "\n";
    return 1;
  }
  char *server = argv[1];
  char *username = argv[2];
  char *password = argv[3];
 
  gxsPOP3Client pop3;

  cout << "Connecting to the POP3 server" << "\n";
  pop3.ConnectClient((const char *)server);
  if(!CheckError(&pop3)) return 1;
  cout << pop3.reply_buf << flush;

  pop3.POP3Login((const char *)username, (const char *)password);
  if(!CheckError(&pop3)) return 1;
  cout << pop3.reply_buf << flush;

  char key;
  Menu();
  int rv = 1;

  while(rv) {
    if (!cin) { 
      ClearInputStream(cin); 
      if (!cin) {  
	cout << "Input stream error" << "\n";
	return 0;
      }
    }
    cout << '>';
    cin >> key;
    if (!cin) continue;
    switch(key) {
      case '?' : Menu(); break;
      case 'h' : case 'H' : Menu(); break;
      case 'q' : case 'Q' : rv = 0; break;
      case 'C' : case 'c' : ClearInputStream(cin); Reset(&pop3); break;
      case 'd' : case 'D' : ClearInputStream(cin); Delete(&pop3); break;
      case 'l' : case 'L' : ClearInputStream(cin); List(&pop3); break;
      case 'r' : case 'R' : ClearInputStream(cin); Retrieve(&pop3); break;
      case 't' : case 'T' : ClearInputStream(cin); Top(&pop3); break;
      default:
        cout << "Unrecognized command" << "\n";
	break;
    }
  }

  pop3.POP3Logout();
  if(!CheckError(&pop3)) return 1;
  cout << pop3.reply_buf << flush;

  pop3.Close();
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
