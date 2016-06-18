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

Test program for the embedded SMTP client class.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include <string.h>
#include "gxsmtp.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

int CheckError(gxsSMTPClient *smtp)
{
  if(smtp->GetSocketError() != gxSOCKET_NO_ERROR) {
    cout << smtp->SocketExceptionMessage() << "\n" << flush;
    if(smtp->reply_buf[0] != 0) { // Check the reply buffer
      cout << smtp->reply_buf << flush;
    }
    smtp->Close();
    return 0;
  }
  return 1; // No errors reported
}

int main(int argc, char **argv)
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  if(argc < 3) {
    cout << "Usage: " << argv[0] << " server from" << "\n";
    cout << "server = Your outgoing mail server" << "\n";
    cout << "from   = Your email address" << "\n";
    cout << "\n";
    cout << "Example: " << argv[0] 
	 << " mailserver.xyz.com myname@xyz.com" << "\n";
    return 1;
  }
  char *server = argv[1];
  char *from = argv[2];
  char *to = "example@example.com";
  char *subject = "GXS SMTP Test Message";
  char *body = "Test test test...\n";

  gxsSMTPClient smtp;

  cout << "\n";
  cout << "Connecting to mail server" << "\n";
  smtp.ConnectClient((const char *)server);
  if(!CheckError(&smtp)) return 1;
  cout << smtp.reply_buf << flush;

  // Get my domain name, as required by the server
  char sbuf[255];
  smtp.GetDomainName(sbuf);

  smtp.SMTPLogin(sbuf);
  if(!CheckError(&smtp)) return 1;
  cout << smtp.reply_buf << flush;

  cout << "\n";
  cout << "Sending a test message without a message header..." << "\n";
  smtp.SMTPRSet();
  if(!CheckError(&smtp)) return 1;
  cout << smtp.reply_buf << flush;

  smtp.SMTPMailFrom((const char *)from);
  if(!CheckError(&smtp)) return 1;
  cout << smtp.reply_buf << flush;

  smtp.SMTPRcptTo((const char *)to);
  if(!CheckError(&smtp)) return 1;
  cout << smtp.reply_buf << flush;

  smtp.SMTPData((const char *)body, (int)strlen(body));
  if(!CheckError(&smtp)) return 1;
  cout << smtp.reply_buf << flush;
  
  cout << "\n";
  cout << "Testing the send message function..." << "\n";
  smtp.SMTPSendMessage(to, from, subject, body);
  if(!CheckError(&smtp)) return 1;
  cout << smtp.reply_buf << flush;

  cout << "\n";
  cout << "Disconnecting from the server" << "\n";
  smtp.SMTPLogout();
  if(!CheckError(&smtp)) return 1;
  cout << smtp.reply_buf << flush;

  cout << "\n";
  smtp.Close();
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
