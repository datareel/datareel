// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxftp.cpp
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
==============================================================
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cstdlib>
#include <fstream>
#include "gxsftp.h"
#include "strutil.h"

using namespace datareel;

int gxsFTPClient::read_reply()
// Read a reply, which may span several lines ("150-" ... "150 ").
// Returns the reply code or -1 if the connection failed.
{
  last_reply.clear();
  last_code = 0;
  std::string line;
  if(!conn.ReadLine(line, 8192)) {
    fail("FTP exception: No reply from server" +
	 (conn.Error().empty() ? std::string() : ": " + conn.Error()));
    return -1;
  }
  last_reply = line;
  if(line.size() < 3) { fail("FTP exception: Bad reply: " + line); return -1; }
  std::string code = line.substr(0, 3);
  if(line.size() > 3 && line[3] == '-') {
    for(int lines = 0; ; lines++) {
      if(lines > 10000 || !conn.ReadLine(line, 8192)) {
	fail("FTP exception: Incomplete reply");
	return -1;
      }
      last_reply += "\n" + line;
      if(line.size() >= 4 && line.compare(0, 3, code) == 0 && line[3] == ' ') break;
    }
  }
  last_code = std::atoi(code.c_str());
  return last_code;
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
    std::size_t a = last_reply.find("(|||");
    if(a != std::string::npos) port = std::atoi(last_reply.c_str() + a + 4);
  }
  if(port <= 0) {
    if(Command("PASV", "227") != 0) return -1;
    // 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2). Only the port is
    // used; the data connection goes to the control connection's host.
    std::size_t a = last_reply.find('(');
    std::size_t b = last_reply.find(')', a == std::string::npos ? 0 : a);
    if(a == std::string::npos || b == std::string::npos) {
      return fail("FTP exception: Bad PASV reply: " + last_reply);
    }
    std::vector<std::string> f = split(last_reply.substr(a + 1, b - a - 1), ',');
    if(f.size() != 6) return fail("FTP exception: Bad PASV reply: " + last_reply);
    port = std::atoi(f[4].c_str()) * 256 + std::atoi(f[5].c_str());
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
  if(!data.ReadAll(listing)) {
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
  if(!data.ReadAll(out)) {
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
  bytes = std::atoll(last_reply.c_str() + 4);
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
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
