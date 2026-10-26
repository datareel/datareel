// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxsmtp.h
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

Simple Mail Transfer Protocol (SMTP) classes used with applications
that require use of embedded SMTP client/server functions.

Changes:
==============================================================
09/03/2002: The return type on the gxsSMTPClient::GetSMTPTimeStamp()
was changed from void to char * to allow this function to be 
returned from another calling function.

03/10/2003: Changed the default blocking timeout value from 5
seconds to 60 seconds to allow for slower mail server to respond
to SMTP commands. 

06/09/2003: Added the gxsSMTPClient::ESMTPLogin() and 
gxsSMTPClient::ESMTPAuthLogin() functions to allow mail clients 
the use SMTP authentication as require by ESMTP servers when 
a client is accessing the mail server from a different domain.

10/26/2026: DataReel 7 rewrites gxsSMTPClient on gxsTLSConnection. 
Mail is only sent over TLS: implicit TLS (port 465) or STARTTLS 
(port 587). A server that does not offer STARTTLS is refused, 
never used in plain text. Login() uses AUTH PLAIN or AUTH LOGIN, 
multi-line replies are read in full, and message bodies are dot 
stuffed. SendMessage() writes RFC 5322 headers.
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_EMBEDDED_SMTP_LIB_HPP__
#define __GX_EMBEDDED_SMTP_LIB_HPP__

#include "gxdlcode.h"

#include <string>
#include <vector>

#include "gxstls.h"

namespace datareel {


enum gxsMailSecurity {
  gxsMAIL_IMPLICIT_TLS, // TLS from the start (SMTP 465, POP3 995, FTPS 990)
  gxsMAIL_STARTTLS      // Plain greeting, then STARTTLS/STLS/AUTH TLS
};

// SMTP client (RFC 5321) that only sends mail over TLS
class GXDLCODE_API gxsSMTPClient
{
public:
  gxsSMTPClient() { }
  ~gxsSMTPClient() { Close(); }

public: // Settings used before Connect()
  void SetTimeout(int seconds) { conn.SetTimeout(seconds); }
  void SetCAFile(const std::string &fname) { conn.SetCAFile(fname); }
  void SetVerifyPeer(bool on) { conn.SetVerifyPeer(on); }
  void SetClientName(const std::string &s) { client_name = s; }

public:
  // Connect, read the greeting, send EHLO and start TLS. Returns 0 if
  // successful.
  int Connect(const std::string &host, int port = gxSOCKET_SMTPS_PORT,
	      gxsMailSecurity security = gxsMAIL_IMPLICIT_TLS);
  int Login(const std::string &user, const std::string &passwd);
  int MailFrom(const std::string &address);
  int RcptTo(const std::string &address);
  int Data(const std::string &message); // Headers and body, CR LF or LF
  int Reset();
  int Quit();
  void Close() { conn.Close(); }

  // Send one plain text message. Connect() (and Login() if the server
  // needs it) must be called first. Returns 0 if successful.
  int SendMessage(const std::string &from, 
		  const std::vector<std::string> &to,
		  const std::string &subject, const std::string &body);

  // Send a command and read the reply. Returns 0 if the reply code
  // starts with expect, such as "250" or "3".
  int Command(const std::string &cmd, const std::string &expect);

public:
  bool HasExtension(const std::string &name) const; // From EHLO, such as "AUTH"
  int LastCode() const { return last_code; }
  const std::string &LastReply() const { return last_reply; }
  const std::string &Error() const { return error; }
  gxsTLSConnection &Connection() { return conn; }

private:
  int read_reply();
  int ehlo();
  int fail(const std::string &msg);

private:
  gxsTLSConnection conn;
  std::string client_name;
  std::vector<std::string> extensions;
  int last_code = 0;
  std::string last_reply;
  std::string error;
};

} // namespace datareel

#endif // __GX_EMBEDDED_SMTP_LIB_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
