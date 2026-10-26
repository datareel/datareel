// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxspop3.h 
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/23/2001
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
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

Post Office Protocol 3 (POP3) classes used with applications
that require use of embedded POP3 client/server functions.

Changes:
==============================================================
10/26/2026: DataReel 7 rewrites gxsPOP3Client on gxsTLSConnection. 
Mail is only read over TLS: implicit TLS (port 995) or STLS (port 
110). A server that does not accept STLS is refused. Multi-line 
replies are read to the terminating dot and un-stuffed, and the 
results are returned as std::string values.
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_EMBEDDED_POP3_LIB_HPP__
#define __GX_EMBEDDED_POP3_LIB_HPP__

#include "gxdlcode.h"

#include <string>

#include "gxsmtp.h" // gxsMailSecurity
#include "gxstls.h"

// POP3 client (RFC 1939, STLS from RFC 2595) that only uses TLS
class GXDLCODE_API gxsPOP3Client
{
public:
  gxsPOP3Client() { }
  ~gxsPOP3Client() { Close(); }

public: // Settings used before Connect()
  void SetTimeout(int seconds) { conn.SetTimeout(seconds); }
  void SetCAFile(const std::string &fname) { conn.SetCAFile(fname); }
  void SetVerifyPeer(bool on) { conn.SetVerifyPeer(on); }

public: // Each returns 0 if the server replied +OK
  int Connect(const std::string &host, int port = gxSOCKET_SPOP3_PORT,
	      gxsMailSecurity security = gxsMAIL_IMPLICIT_TLS);
  int Login(const std::string &user, const std::string &passwd);
  int Stat(int &num_messages, long long &total_bytes);
  int List(std::string &listing);  // "number size" lines
  int Retr(int message_number, std::string &message);
  int Top(int message_number, int lines, std::string &message);
  int Delete(int message_number);
  int Reset();
  int Quit();
  void Close() { conn.Close(); }

  // Send a command and read a one-line reply
  int Command(const std::string &cmd);

public:
  const std::string &LastReply() const { return last_reply; }
  const std::string &Error() const { return error; }
  gxsTLSConnection &Connection() { return conn; }

private:
  int read_status();
  int read_multiline(std::string &data);
  int fail(const std::string &msg) { error = msg; return -1; }

private:
  gxsTLSConnection conn;
  std::string last_reply;
  std::string error;
};

#endif // __GX_EMBEDDED_POP3_LIB_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
