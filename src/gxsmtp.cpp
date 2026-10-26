// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxsmtp.cpp
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/23/2001
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
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

Simple Mail Transfer Protocol (SMTP) classes used with applications
that require use of embedded SMTP client/server functions.

Changes:
==============================================================
09/03/2002: The return type on gxsSMTPClient::GetSMTPTimeStamp()
was changed from void to char * to allow this function to be 
returned from another calling function.

09/03/2002: gxsSMTPClient::GetSMTPTimeStamp() now uses the 
SysTime class to format SMTP time/date strings.

09/19/2003: Modified gxsSMTPClient::RecvResponse() to return 
immediately if numeric response codes do not match and the entire 
SMTP response has been received.

09/22/2005: Modified gxsSMTPClient::RecvResponse() to signal 
a graceful socket disconnect.

10/26/2026: DataReel 7 rewrites gxsSMTPClient on gxsTLSConnection. 
Mail is only sent over TLS: implicit TLS (port 465) or STARTTLS 
(port 587). A server that does not offer STARTTLS is refused, 
never used in plain text. Login() uses AUTH PLAIN or AUTH LOGIN, 
multi-line replies are read in full, and message bodies are dot 
stuffed. SendMessage() writes RFC 5322 headers.
==============================================================
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cstdlib>
#include <ctime>
#include "gxsmtp.h"
#include "gxs_b64.h"
#include "strutil.h"
#include "systime.h"

using namespace datareel;

int gxsSMTPClient::fail(const std::string &msg)
{
  error = msg;
  return -1;
}

int gxsSMTPClient::read_reply()
// Read a reply, which may span several "250-" lines. Returns the
// reply code or -1 if the connection failed.
{
  last_reply.clear();
  last_code = 0;
  std::string line;
  for(int lines = 0; lines < 1000; lines++) {
    if(!conn.ReadLine(line, 4096)) {
      fail("SMTP exception: No reply from server" + 
	   (conn.Error().empty() ? std::string() : ": " + conn.Error()));
      return -1;
    }
    if(line.size() < 3) { fail("SMTP exception: Bad reply: " + line); return -1; }
    if(!last_reply.empty()) last_reply += "\n";
    last_reply += line;
    if(line.size() == 3 || line[3] != '-') {
      last_code = std::atoi(line.substr(0, 3).c_str());
      return last_code;
    }
  }
  fail("SMTP exception: Reply is too long");
  return -1;
}

int gxsSMTPClient::Command(const std::string &cmd, const std::string &expect)
{
  if(cmd.find('\r') != std::string::npos || cmd.find('\n') != std::string::npos) {
    return fail("SMTP exception: Command contains a line break");
  }
  if(conn.SendLine(cmd) < 0) return fail("SMTP exception: " + conn.Error());
  if(read_reply() < 0) return -1;
  if(!starts_with(std::to_string(last_code), expect)) {
    return fail("SMTP exception: " + last_reply);
  }
  return 0;
}

int gxsSMTPClient::ehlo()
{
  std::string name = client_name.empty() ? gxSocket::HostName() : client_name;
  if(name.empty()) name = "localhost";
  if(Command("EHLO " + name, "250") != 0) return -1;
  extensions.clear();
  std::vector<std::string> lines = split(last_reply, '\n');
  for(std::size_t i = 1; i < lines.size(); i++) {
    if(lines[i].size() > 4) extensions.push_back(to_upper(lines[i].substr(4)));
  }
  return 0;
}

bool gxsSMTPClient::HasExtension(const std::string &name) const
{
  std::string n = to_upper(name);
  for(const auto &e : extensions) {
    if(e == n || starts_with(e, n + " ")) return true;
  }
  return false;
}

int gxsSMTPClient::Connect(const std::string &host, int port,
			   gxsMailSecurity security)
{
  error.clear();
  if(conn.Connect(host, port, security == gxsMAIL_IMPLICIT_TLS) != 0) {
    return fail(conn.Error());
  }
  if(read_reply() < 0) return -1;
  if(last_code != 220) return fail("SMTP exception: " + last_reply);
  if(ehlo() != 0) return -1;
  if(security == gxsMAIL_STARTTLS) {
    if(!HasExtension("STARTTLS")) {
      conn.Close();
      return fail("SMTP exception: " + host + " does not offer STARTTLS");
    }
    if(Command("STARTTLS", "220") != 0) return -1;
    if(conn.StartTLS() != 0) return fail(conn.Error());
    if(ehlo() != 0) return -1; // Ask again over TLS
  }
  return 0;
}

int gxsSMTPClient::Login(const std::string &user, const std::string &passwd)
{
  std::string auth;
  for(const auto &e : extensions) {
    if(starts_with(e, "AUTH ")) auth = e;
  }
  if(auth.find(" PLAIN") != std::string::npos || auth.empty()) {
    std::string creds;
    creds += '\0';
    creds += user;
    creds += '\0';
    creds += passwd;
    return Command("AUTH PLAIN " + gxsBase64Encode(creds), "235");
  }
  if(Command("AUTH LOGIN", "334") != 0) return -1;
  if(Command(gxsBase64Encode(user), "334") != 0) return -1;
  return Command(gxsBase64Encode(passwd), "235");
}

static std::string angle(const std::string &address)
{
  std::string a = trim(address);
  if(!a.empty() && a[0] == '<') return a;
  // "Name <user@host>" keeps only the address part
  std::size_t lt = a.find('<');
  if(lt != std::string::npos) return a.substr(lt);
  return "<" + a + ">";
}

int gxsSMTPClient::MailFrom(const std::string &address)
{
  return Command("MAIL FROM:" + angle(address), "250");
}

int gxsSMTPClient::RcptTo(const std::string &address)
{
  return Command("RCPT TO:" + angle(address), "25");
}

int gxsSMTPClient::Data(const std::string &message)
{
  if(Command("DATA", "354") != 0) return -1;
  // Normalize line ends to CR LF and dot stuff lines that start with '.'
  std::string out;
  out.reserve(message.size() + 64);
  bool line_start = true;
  for(std::size_t i = 0; i < message.size(); i++) {
    char c = message[i];
    if(line_start && c == '.') out += '.';
    if(c == '\r') {
      if(i + 1 < message.size() && message[i+1] == '\n') continue;
      c = '\n';
    }
    if(c == '\n') {
      out += "\r\n";
      line_start = true;
      continue;
    }
    out += c;
    line_start = false;
  }
  if(!line_start) out += "\r\n";
  out += ".\r\n";
  if(conn.Send(out) < 0) return fail("SMTP exception: " + conn.Error());
  if(read_reply() < 0) return -1;
  if(last_code != 250) return fail("SMTP exception: " + last_reply);
  return 0;
}

int gxsSMTPClient::Reset()
{
  return Command("RSET", "250");
}

int gxsSMTPClient::Quit()
{
  int rv = Command("QUIT", "221");
  conn.Close();
  return rv;
}

int gxsSMTPClient::SendMessage(const std::string &from, 
			       const std::vector<std::string> &to,
			       const std::string &subject, 
			       const std::string &body)
{
  if(to.empty()) return fail("SMTP exception: No recipients");
  for(const std::string *h : { &from, &subject }) {
    if(h->find('\r') != std::string::npos || h->find('\n') != std::string::npos) {
      return fail("SMTP exception: Header contains a line break");
    }
  }
  if(MailFrom(from) != 0) return -1;
  std::string to_header;
  for(const auto &t : to) {
    if(t.find('\r') != std::string::npos || t.find('\n') != std::string::npos) {
      return fail("SMTP exception: Header contains a line break");
    }
    if(RcptTo(t) != 0) return -1;
    if(!to_header.empty()) to_header += ", ";
    to_header += t;
  }
  std::string domain = "localhost";
  std::size_t at = from.rfind('@');
  if(at != std::string::npos) {
    domain = from.substr(at + 1);
    std::size_t gt = domain.find('>');
    if(gt != std::string::npos) domain.erase(gt);
  }
  std::string msg;
  msg += "Date: " + smtp_time() + "\r\n";
  msg += "From: " + from + "\r\n";
  msg += "To: " + to_header + "\r\n";
  msg += "Subject: " + subject + "\r\n";
  msg += "Message-ID: <" + std::to_string((long long)std::time(0)) + "." + 
    std::to_string(std::rand()) + "@" + domain + ">\r\n";
  msg += "MIME-Version: 1.0\r\n";
  msg += "Content-Type: text/plain; charset=utf-8\r\n";
  msg += "Content-Transfer-Encoding: 8bit\r\n";
  msg += "\r\n";
  msg += body;
  return Data(msg);
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
