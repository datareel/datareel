// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxspop3.cpp
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/23/2001
// Date Last Modified: 11/01/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description and Details ------------- // 
// ----------------------------------------------------------- // 
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.
 
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

Post Office Protocol 3 (POP3) classes used with applications
that require use of embedded POP3 client/server functions.

Changes:
==============================================================
09/22/2005:Modified gxsPOP3Client::RecvResponse() to signal 
a graceful socket disconnect.

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
#include "gxdlcode.h"

#include <cstdlib>
#include <functional>
#include "gxspop3.h"
#include "strutil.h"

namespace datareel {



int gxsPOP3Client::read_status()
{
  if(!conn.ReadLine(last_reply, 4096)) {
    return fail("POP3 exception: No reply from server" + 
		(conn.Error().empty() ? std::string() : ": " + conn.Error()));
  }
  if(!starts_with(last_reply, "+OK")) {
    return fail("POP3 exception: " + last_reply);
  }
  return 0;
}

// 11/01/2026: One multi-line parser for the connection and for
// buffers. Returns 0, -1 if a line could not be read or -2 if the
// reply is over max_size bytes.
static int parse_multiline(const std::function<bool(std::string &)> &next_line,
			   std::string &data, std::size_t max_size)
{
  data.clear();
  std::string line;
  for(;;) {
    if(!next_line(line)) return -1;
    if(line == ".") return 0;
    if(!line.empty() && line[0] == '.') line.erase(0, 1); // Dot stuffing
    if(data.size() + line.size() + 2 > max_size) return -2;
    data += line;
    data += "\r\n";
  }
}

int gxsPOP3Client::ParseMultiline(std::string_view buf, std::string &data,
				  std::size_t max_size, std::size_t *consumed)
{
  std::size_t pos = 0;
  auto next_line = [&](std::string &line) -> bool {
    std::size_t nl = buf.find('\n', pos);
    if(nl == std::string_view::npos || nl - pos > 1024*1024) return false;
    line.assign(buf.data() + pos, nl - pos);
    pos = nl + 1;
    if(!line.empty() && line.back() == '\r') line.pop_back();
    return true;
  };
  int rv = parse_multiline(next_line, data, max_size);
  if(consumed) *consumed = pos;
  return rv < 0 ? -1 : 0;
}

int gxsPOP3Client::read_multiline(std::string &data)
{
  auto next_line = [this](std::string &line) -> bool {
    return conn.ReadLine(line, 1024*1024);
  };
  int rv = parse_multiline(next_line, data, max_size);
  if(rv == -1) {
    return fail("POP3 exception: Incomplete reply" +
		(conn.Error().empty() ? std::string() : ": " + conn.Error()));
  }
  if(rv < 0) {
    // 11/01/2026: The rest of the reply is not read, so the 
    // connection cannot be used for another command
    conn.Close();
    return fail("POP3 exception: Reply is larger than " + 
		std::to_string(max_size) + " bytes");
  }
  return 0;
}

int gxsPOP3Client::Command(const std::string &cmd)
{
  if(cmd.find('\r') != std::string::npos || cmd.find('\n') != std::string::npos) {
    return fail("POP3 exception: Command contains a line break");
  }
  if(conn.SendLine(cmd) < 0) return fail("POP3 exception: " + conn.Error());
  return read_status();
}

int gxsPOP3Client::Connect(const std::string &host, int port,
			   gxsMailSecurity security)
{
  error.clear();
  if(conn.Connect(host, port, security == gxsMAIL_IMPLICIT_TLS) != 0) {
    return fail(conn.Error());
  }
  if(read_status() != 0) return -1;
  if(security == gxsMAIL_STARTTLS) {
    if(Command("STLS") != 0) {
      conn.Close();
      return fail("POP3 exception: " + host + " does not accept STLS");
    }
    if(conn.StartTLS() != 0) return fail(conn.Error());
  }
  return 0;
}

int gxsPOP3Client::Login(const std::string &user, const std::string &passwd)
{
  if(Command("USER " + user) != 0) return -1;
  return Command("PASS " + passwd);
}

int gxsPOP3Client::Stat(int &num_messages, long long &total_bytes)
{
  num_messages = 0;
  total_bytes = 0;
  if(Command("STAT") != 0) return -1;
  std::vector<std::string> f = split(last_reply, ' ', true);
  if(f.size() < 3) return fail("POP3 exception: Bad STAT reply: " + last_reply);
  // 11/01/2026: Digits only, no atoi() overflow
  for(int i = 1; i <= 2; i++) {
    if(f[i].empty() || f[i].size() > 18 || 
       f[i].find_first_not_of("0123456789") != std::string::npos) {
      return fail("POP3 exception: Bad STAT reply: " + last_reply);
    }
  }
  long long n = std::strtoll(f[1].c_str(), 0, 10);
  if(n > 0x7FFFFFFF) return fail("POP3 exception: Bad STAT reply: " + last_reply);
  num_messages = (int)n;
  total_bytes = std::strtoll(f[2].c_str(), 0, 10);
  return 0;
}

int gxsPOP3Client::List(std::string &listing)
{
  if(Command("LIST") != 0) return -1;
  return read_multiline(listing);
}

int gxsPOP3Client::Retr(int message_number, std::string &message)
{
  if(Command("RETR " + std::to_string(message_number)) != 0) return -1;
  return read_multiline(message);
}

int gxsPOP3Client::Top(int message_number, int lines, std::string &message)
{
  if(Command("TOP " + std::to_string(message_number) + " " + 
	     std::to_string(lines)) != 0) return -1;
  return read_multiline(message);
}

int gxsPOP3Client::Delete(int message_number)
{
  return Command("DELE " + std::to_string(message_number));
}

int gxsPOP3Client::Reset()
{
  return Command("RSET");
}

int gxsPOP3Client::Quit()
{
  int rv = Command("QUIT");
  conn.Close();
  return rv;
}
} // namespace datareel

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
