// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxspop3.h 
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/23/2001
// Date Last Modified: 11/01/2026
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

11/01/2026: Multi-line replies stop at SetMaxSize() bytes (64 MB by
default) and are parsed by ParseMultiline(), which can be given a
buffer. Stat() checks its numbers instead of using atoi().
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_EMBEDDED_POP3_LIB_HPP__
#define __GX_EMBEDDED_POP3_LIB_HPP__

#include "gxdlcode.h"

#include <cstddef>
#include <string>
#include <string_view>

#include "gxsmtp.h" // gxsMailSecurity
#include "gxstls.h"

namespace datareel {


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
  // 11/01/2026: Largest multi-line reply (List, Retr, Top) kept in 
  // memory
  void SetMaxSize(std::size_t n) { max_size = n; }

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

public: // 11/01/2026: Parser, used on the connection and in tests
  // Parse the lines of a multi-line reply that follow "+OK", up to
  // the "." line, from the start of buf. Dot stuffing is removed and
  // lines end in CR LF. Returns 0, or -1 if buf does not hold a
  // complete reply or it is over max_size bytes.
  static int ParseMultiline(std::string_view buf, std::string &data,
			    std::size_t max_size = 64*1024*1024,
			    std::size_t *consumed = 0);

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
  std::size_t max_size = 64*1024*1024; // 11/01/2026: See SetMaxSize()
};

} // namespace datareel

#endif // __GX_EMBEDDED_POP3_LIB_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
