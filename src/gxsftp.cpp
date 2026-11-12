// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxftp.cpp
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
version 2.1 of the License, or (at your option) any later version.
 
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  
USA

File Transfer Protocol (FTP) classes used with applications
that require use of embedded FTP client/server functions.

Changes:
==============================================================
06/10/2002: Modified the gxsFTPClient::RecvResponse(gxsSocket_t s)
function to check for FTP error codes 400 and higher instead of
waiting for a timeout error.

06/10/2002: Modified the following functions to support
passive FTP mode: gxsFTPClient::FTPGet(), gxsFTPClient::FTPPut(),
gxsFTPClient::FTPList(), gxsFTPClient::OpenDataPort(),
gxsFTPClient::ReadDataPort(), gxsFTPClient::WriteDataPort()

06/17/2002: Modified the gxsFTPClient::SendCommand() function to
clear the command and reply buffers before send a command. This 
change was made to ensure that strings are always null-terminated.

06/17/2002: Modified the gxsFTPClient::FTPGet() and 
gxsFTPClient::FTPList() functions not to read the server 226
reply string following a list or a get. This change was made 
because the FTP client was hanging on the blocking receive 
while waiting for the servers reply. This problem occurred 
when connecting to Windows based FTP servers such as the WAR
FTP server and the IIS FTP server. 

06/17/2002: Modified the gxsFTPClient::OpenDataPort() function
to check for 226 and 227 replies from the server when in passive
FTP mode.

06/20/2002: Modified the gxsFTPClient::RecvResponse() function 
to handle 110, 120, 125, 202, 225, and 332 FTP reply codes.

12/03/2002: Modified the gxsFTPClient:::FTPLogin() function to 
handle Welcome messages that send multiple 230 FTP reply codes. 
The last line of the Welcome message will be acknowledged when 
the terminating "230 " is received.

12/03/2002: Modified the gxsFTPClient::: FTPLogout () function to 
handle Exit messages that send multiple 221 FTP reply codes. The 
last line of the Exit message will be acknowledged when the 
terminating "221 " is received.

09/22/2005:Modified gxsFTPClient::RecvResponse() to signal 
a graceful socket disconnect.

10/26/2026: DataReel 7 rewrites gxsFTPClient as an FTPS client on 
gxsTLSConnection. The control connection uses explicit TLS (AUTH 
TLS on port 21) or implicit TLS (port 990), and every data 
connection is encrypted with PROT P and reuses the control 
connection's TLS session. Transfers are passive only (EPSV, then 
PASV), and the data connection always goes to the server's own 
address, never to the address in a PASV reply. Files are read and 
written with std::fstream.

11/01/2026: Replies are parsed by ParseReply() (3 digit codes only),
PASV/EPSV ports by ParsePASVPort()/ParseEPSVPort() with strtol() and
range checks (atoi() overflow was undefined), SIZE by 
ParseSizeReply() (reading "213" alone went past the end of the 
string), and List()/GetString() stop at SetMaxSize() bytes.
==============================================================
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cstdlib>
#include <fstream>
#include <functional>
#include "gxsftp.h"
#include "strutil.h"

namespace datareel {



// 11/01/2026: One reply parser for the connection and for buffers.
// next_line returns false at the end of input. Returns the code, -1
// if there was no reply, -2 if it was malformed or -3 if a multi-line
// reply did not end.
static int parse_reply(const std::function<bool(std::string &)> &next_line,
		       std::string &reply)
{
  reply.clear();
  std::string line;
  if(!next_line(line)) return -1;
  reply = line;
  if(line.size() < 3 || line[0] < '1' || line[0] > '5' ||
     line[1] < '0' || line[1] > '9' || line[2] < '0' || line[2] > '9' ||
     (line.size() > 3 && line[3] != ' ' && line[3] != '-')) return -2;
  std::string code = line.substr(0, 3);
  if(line.size() > 3 && line[3] == '-') {
    for(int lines = 0; ; lines++) {
      if(lines > 10000 || reply.size() > 1024*1024 || !next_line(line)) {
	return -3;
      }
      reply += "\n" + line;
      if(line.size() >= 4 && line.compare(0, 3, code) == 0 && line[3] == ' ') break;
    }
  }
  return (code[0] - '0') * 100 + (code[1] - '0') * 10 + (code[2] - '0');
}

int gxsFTPClient::ParseReply(std::string_view buf, std::string &reply,
			     std::size_t *consumed)
{
  std::size_t pos = 0;
  auto next_line = [&](std::string &line) -> bool {
    std::size_t nl = buf.find('\n', pos);
    if(nl == std::string_view::npos || nl - pos > 8192) return false;
    line.assign(buf.data() + pos, nl - pos);
    pos = nl + 1;
    if(!line.empty() && line.back() == '\r') line.pop_back();
    return true;
  };
  int rv = parse_reply(next_line, reply);
  if(consumed) *consumed = pos;
  return rv < 0 ? -1 : rv;
}

int gxsFTPClient::read_reply()
// Read a reply, which may span several lines ("150-" ... "150 ").
// Returns the reply code or -1 if the connection failed.
{
  last_reply.clear();
  last_code = 0;
  auto next_line = [this](std::string &line) -> bool {
    return conn.ReadLine(line, 8192);
  };
  int rv = parse_reply(next_line, last_reply);
  if(rv == -1) {
    fail("FTP exception: No reply from server" +
	 (conn.Error().empty() ? std::string() : ": " + conn.Error()));
    return -1;
  }
  if(rv == -2) { fail("FTP exception: Bad reply: " + last_reply); return -1; }
  if(rv < 0) { fail("FTP exception: Incomplete reply"); return -1; }
  last_code = rv;
  return last_code;
}

// 11/01/2026: A decimal number from 0 to max_value with no sign, 
// white space inside or other characters
static bool parse_number(std::string_view s, long max_value, long &out)
{
  std::string t = trim(s);
  if(t.empty() || t.size() > 6) return false;
  for(char c : t) if(c < '0' || c > '9') return false;
  char *end = 0;
  long v = std::strtol(t.c_str(), &end, 10);
  if(*end != '\0' || v < 0 || v > max_value) return false;
  out = v;
  return true;
}

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

int gxsFTPClient::ParsePASVPort(std::string_view reply)
{
  // 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)
  std::string_view fields;
  std::size_t a = reply.find('(');
  if(a != std::string_view::npos) {
    std::size_t b = reply.find(')', a);
    if(b == std::string_view::npos) return -1;
    fields = reply.substr(a + 1, b - a - 1);
  }
  else { // Some servers leave out the parentheses (RFC 1123 4.1.2.6)
    std::size_t i = reply.size() > 4 ? 4 : reply.size(); // Skip the code
    while(i < reply.size() && !is_digit(reply[i])) i++;
    fields = reply.substr(i);
    std::size_t e = fields.size();
    while(e > 0 && !is_digit(fields[e-1])) e--;
    fields = fields.substr(0, e);
  }
  std::vector<std::string> f = split(fields, ',');
  if(f.size() != 6) return -1;
  long v[6];
  for(int i = 0; i < 6; i++) {
    if(!parse_number(f[i], 255, v[i])) return -1;
  }
  long port = v[4] * 256 + v[5];
  if(port < 1 || port > 65535) return -1;
  return (int)port;
}

int gxsFTPClient::ParseEPSVPort(std::string_view reply)
{
  // 229 Entering Extended Passive Mode (|||port|), where '|' can be
  // any printable character other than a digit (RFC 2428)
  std::size_t a = reply.find('(');
  if(a == std::string_view::npos || a + 5 > reply.size()) return -1;
  char d = reply[a+1];
  if(d < 33 || d > 126 || is_digit(d)) return -1;
  if(reply[a+2] != d || reply[a+3] != d) return -1;
  std::size_t e = reply.find(d, a + 4);
  if(e == std::string_view::npos || e + 1 >= reply.size() || 
     reply[e+1] != ')') return -1;
  long port = 0;
  if(!parse_number(reply.substr(a + 4, e - a - 4), 65535, port)) return -1;
  if(port < 1) return -1;
  return (int)port;
}

bool gxsFTPClient::ParseSizeReply(std::string_view reply, long long &bytes)
{
  bytes = -1;
  std::size_t nl = reply.rfind('\n'); // Last line of the reply
  if(nl != std::string_view::npos) reply = reply.substr(nl + 1);
  if(reply.size() < 5 || reply.compare(0, 4, "213 ") != 0) return false;
  std::string n = trim(reply.substr(4));
  if(n.empty() || n.size() > 18) return false;
  for(char c : n) if(!is_digit(c)) return false;
  bytes = std::strtoll(n.c_str(), 0, 10);
  return true;
}

int gxsFTPClient::Command(const std::string &cmd, const std::string &expect)
{
  if(cmd.find('\r') != std::string::npos || cmd.find('\n') != std::string::npos) {
    return fail("FTP exception: Command contains a line break");
  }
  if(conn.SendLine(cmd) < 0) return fail("FTP exception: " + conn.Error());
  if(read_reply() < 0) return -1;
  if(!starts_with(std::to_string(last_code), expect)) {
    return fail("FTP exception: " + last_reply);
  }
  return 0;
}

int gxsFTPClient::Connect(const std::string &host, int port,
			  gxsMailSecurity security)
{
  error.clear();
  host_name = host;
  if(conn.Connect(host, port, security == gxsMAIL_IMPLICIT_TLS) != 0) {
    return fail(conn.Error());
  }
  if(read_reply() < 0) return -1;
  if(last_code != 220) return fail("FTP exception: " + last_reply);
  if(security == gxsMAIL_STARTTLS) {
    if(Command("AUTH TLS", "234") != 0) {
      conn.Close();
      return fail("FTP exception: " + host + " does not accept AUTH TLS");
    }
    if(conn.StartTLS() != 0) return fail(conn.Error());
  }
  return 0;
}

int gxsFTPClient::Login(const std::string &user, const std::string &passwd)
{
  if(Command("USER " + user, "") != 0) return -1;
  if(last_code == 331) {
    if(Command("PASS " + passwd, "2") != 0) return -1;
  }
  else if(last_code / 100 != 2) {
    return fail("FTP exception: " + last_reply);
  }
  // Encrypt every data connection
  if(Command("PBSZ 0", "2") != 0) return -1;
  if(Command("PROT P", "2") != 0) return -1;
  return Type('I');
}

int gxsFTPClient::Type(char type)
{
  return Command(std::string("TYPE ") + type, "2");
}

int gxsFTPClient::open_data(gxsTLSConnection &data, const std::string &cmd)
// Open a passive data connection, send cmd and start TLS on the data
// connection. Returns 0 if the server is ready to transfer.
{
  int port = 0;
  if(Command("EPSV", "229") == 0) {
    // 229 Entering Extended Passive Mode (|||port|)
    // 11/01/2026: Parsed with range checks by ParseEPSVPort()
    port = ParseEPSVPort(last_reply);
  }
  if(port <= 0) {
    if(Command("PASV", "227") != 0) return -1;
    // 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2). Only the port is
    // used; the data connection goes to the control connection's host.
    // 11/01/2026: Each field must be 0-255 (ParsePASVPort())
    port = ParsePASVPort(last_reply);
    if(port <= 0) return fail("FTP exception: Bad PASV reply: " + last_reply);
  }
  if(port <= 0 || port > 65535) return fail("FTP exception: Bad data port");

  gxSocket s;
  std::string addr = conn.Socket().PeerIPAddress();
  if(s.Connect(addr, port, timeout_sec) != 0) {
    return fail(std::string("FTP exception: Data connection: ") + 
		s.SocketExceptionMessage());
  }
  if(Command(cmd, "1") != 0) return -1;
  data.SetTimeout(timeout_sec);
  if(data.AttachClient(std::move(s), host_name, &conn) != 0) {
    return fail("FTP exception: Data connection TLS: " + data.Error());
  }
  return 0;
}

int gxsFTPClient::finish_data(gxsTLSConnection &data)
{
  data.Close();
  if(read_reply() < 0) return -1;
  if(last_code / 100 != 2) return fail("FTP exception: " + last_reply);
  return 0;
}

int gxsFTPClient::List(const std::string &path, std::string &listing, bool full)
{
  listing.clear();
  gxsTLSConnection data;
  std::string cmd = full ? "LIST" : "NLST";
  if(!path.empty()) cmd += " " + path;
  if(open_data(data, cmd) != 0) return -1;
  // 11/01/2026: At most max_size bytes
  if(!data.ReadAll(listing, max_size)) {
    data.Close();
    read_reply();
    return fail("FTP exception: Listing failed: " + data.Error());
  }
  return finish_data(data);
}

int gxsFTPClient::GetString(const std::string &remote, std::string &out)
{
  out.clear();
  gxsTLSConnection data;
  if(open_data(data, "RETR " + remote) != 0) return -1;
  // 11/01/2026: At most max_size bytes
  if(!data.ReadAll(out, max_size)) {
    data.Close();
    read_reply();
    return fail("FTP exception: Download failed: " + data.Error());
  }
  return finish_data(data);
}

int gxsFTPClient::Get(const std::string &remote, const std::string &local_fname,
		      long long &bytes)
{
  bytes = 0;
  std::ofstream out(local_fname, std::ios::out | std::ios::binary | 
		    std::ios::trunc);
  if(!out) return fail("FTP exception: Cannot create " + local_fname);
  gxsTLSConnection data;
  if(open_data(data, "RETR " + remote) != 0) return -1;
  char buf[65536];
  for(;;) {
    int n = data.ReadSome(buf, sizeof(buf));
    if(n == 0) break;
    if(n < 0) {
      data.Close();
      read_reply();
      return fail("FTP exception: Download failed: " + data.Error());
    }
    out.write(buf, n);
    if(!out) {
      data.Close();
      read_reply();
      return fail("FTP exception: Cannot write " + local_fname);
    }
    bytes += n;
  }
  return finish_data(data);
}

int gxsFTPClient::PutString(const std::string &in, const std::string &remote)
{
  gxsTLSConnection data;
  if(open_data(data, "STOR " + remote) != 0) return -1;
  if(data.Send(in) < 0) {
    data.Close();
    read_reply();
    return fail("FTP exception: Upload failed: " + data.Error());
  }
  return finish_data(data);
}

int gxsFTPClient::Put(const std::string &local_fname, const std::string &remote,
		      long long &bytes)
{
  bytes = 0;
  std::ifstream in(local_fname, std::ios::in | std::ios::binary);
  if(!in) return fail("FTP exception: Cannot open " + local_fname);
  gxsTLSConnection data;
  if(open_data(data, "STOR " + remote) != 0) return -1;
  char buf[65536];
  while(in) {
    in.read(buf, sizeof(buf));
    std::streamsize n = in.gcount();
    if(n <= 0) break;
    if(data.Send(buf, (int)n) < 0) {
      data.Close();
      read_reply();
      return fail("FTP exception: Upload failed: " + data.Error());
    }
    bytes += n;
  }
  if(in.bad()) {
    data.Close();
    read_reply();
    return fail("FTP exception: Cannot read " + local_fname);
  }
  return finish_data(data);
}

int gxsFTPClient::ChDir(const std::string &dir)
{
  return Command("CWD " + dir, "2");
}

int gxsFTPClient::MkDir(const std::string &dir)
{
  return Command("MKD " + dir, "2");
}

int gxsFTPClient::RmDir(const std::string &dir)
{
  return Command("RMD " + dir, "2");
}

int gxsFTPClient::PWD(std::string &dir)
{
  dir.clear();
  if(Command("PWD", "257") != 0) return -1;
  // 257 "/path" is the current directory ("" inside the name is a ")
  std::size_t a = last_reply.find('"');
  if(a == std::string::npos) return 0;
  for(std::size_t i = a + 1; i < last_reply.size(); i++) {
    if(last_reply[i] == '"') {
      if(i + 1 < last_reply.size() && last_reply[i+1] == '"') { dir += '"'; i++; continue; }
      break;
    }
    dir += last_reply[i];
  }
  return 0;
}

int gxsFTPClient::Size(const std::string &path, long long &bytes)
{
  bytes = -1;
  if(Command("SIZE " + path, "213") != 0) return -1;
  // 11/01/2026: c_str() + 4 read past the end of a "213" reply
  if(!ParseSizeReply(last_reply, bytes)) {
    return fail("FTP exception: Bad SIZE reply: " + last_reply);
  }
  return 0;
}

int gxsFTPClient::Delete(const std::string &path)
{
  return Command("DELE " + path, "2");
}

int gxsFTPClient::Rename(const std::string &from, const std::string &to)
{
  if(Command("RNFR " + from, "350") != 0) return -1;
  return Command("RNTO " + to, "2");
}

int gxsFTPClient::Quit()
{
  int rv = Command("QUIT", "2");
  conn.Close();
  return rv;
}
} // namespace datareel

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
