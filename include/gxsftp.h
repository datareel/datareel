// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxsftp.h
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

File Transfer Protocol (FTP) classes used with applications
that require use of embedded FTP client/server functions.

Changes:
==============================================================
03/10/2002: The gxsFTPClient class no longer uses the C++ fstream 
class as the underlying file system. The gxDatabase file pointer 
routines are now to used define the underlying file system used by 
the gxsFTPClient class. This change was made to support large files 
and NTFS file system enhancements. To enable large file support 
users must define the __64_BIT_DATABASE_ENGINE__ preprocessor 
directive.

06/10/2002: Added gxsFTP_BUF_SIZE constant used to replace
gxsBUF_SIZE, which was previously used to set the fixed FTP 
replay buffer sizes. This change was made to accommodate larger 
fixed FTP reply buffers without changing the gxsFTP_BUF_SIZE 
constant.

06/10/2002: Added the gxsFTPClient::passive_mode variable 
used internally to toggle passive FTP mode on and off.

06/10/2002: Added the gxsFTPClient::FTPPassive() and 
gxsFTPClient::FTPActive() functions used to toggle passive FTP
mode on and off.

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
#ifndef __GX_EMBEDDED_FTP_LIB_HPP__
#define __GX_EMBEDDED_FTP_LIB_HPP__

#include "gxdlcode.h"

#include <string>

#include "gxsmtp.h" // gxsMailSecurity
#include "gxstls.h"

// FTPS client (RFC 959 with RFC 4217 TLS). Plain FTP is not supported.
class GXDLCODE_API gxsFTPClient
{
public:
  gxsFTPClient() { }
  ~gxsFTPClient() { Close(); }

public: // Settings used before Connect()
  void SetTimeout(int seconds) { conn.SetTimeout(seconds); timeout_sec = seconds; }
  void SetCAFile(const std::string &fname) { conn.SetCAFile(fname); }
  void SetVerifyPeer(bool on) { conn.SetVerifyPeer(on); }

public: // Each returns 0 if successful
  // gxsMAIL_STARTTLS sends AUTH TLS after the greeting (explicit FTPS)
  int Connect(const std::string &host, int port = gxSOCKET_FTP_PORT,
	      gxsMailSecurity security = gxsMAIL_STARTTLS);
  int Login(const std::string &user, const std::string &passwd);
  int Type(char type = 'I'); // 'I' binary, 'A' ASCII
  int List(const std::string &path, std::string &listing, bool full = true);
  int ChDir(const std::string &dir);
  int MkDir(const std::string &dir);
  int RmDir(const std::string &dir);
  int PWD(std::string &dir);
  int Size(const std::string &path, long long &bytes);
  int Delete(const std::string &path);
  int Rename(const std::string &from, const std::string &to);
  int Get(const std::string &remote, const std::string &local_fname, 
	  long long &bytes);
  int Put(const std::string &local_fname, const std::string &remote, 
	  long long &bytes);
  int GetString(const std::string &remote, std::string &data);
  int PutString(const std::string &data, const std::string &remote);
  int Quit();
  void Close() { conn.Close(); }

  // Send a command and read the reply. Returns 0 if the reply code
  // starts with expect, such as "2" or "250".
  int Command(const std::string &cmd, const std::string &expect);

public:
  int LastCode() const { return last_code; }
  const std::string &LastReply() const { return last_reply; }
  const std::string &Error() const { return error; }
  gxsTLSConnection &Connection() { return conn; }

private:
  int read_reply();
  int open_data(gxsTLSConnection &data, const std::string &cmd);
  int finish_data(gxsTLSConnection &data);
  int fail(const std::string &msg) { error = msg; return -1; }

private:
  gxsTLSConnection conn;
  std::string host_name;
  int timeout_sec = 60;
  int last_code = 0;
  std::string last_reply;
  std::string error;
};

#endif // __GX_EMBEDDED_FTP_LIB_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
