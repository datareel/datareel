// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxsimap.cpp
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/16/2026
// Date Last Modified: 11/16/2026
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

Internet Message Access Protocol (IMAP) client class, see gxsimap.h.

Changes:
==============================================================
11/16/2026: First version, based on the 2007 IMAP client by Douglas
Gaer. Changes from that client: a reply ends only at the line that
starts with the command's tag (it ended at "NO" or "BAD" anywhere in
the text, including message text), NO and BAD replies are errors,
literals are read by count, arguments are quoted (or sent as literals),
the fetches do not set \Seen, plain text connections and STARTTLS are
supported, and replies are limited in size.
==============================================================
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include "gxsimap.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

gxsIMAPClient::gxsIMAPClient()
{
  imap_connected = 0;
  use_tls = 0;
  allow_plain_login = 0;
  tag_number = 0;
  time_out_sec = 60;
  time_out_usec = 0;
  max_reply = gxsIMAP_DEF_MAX_REPLY;
  record_proto = 0;
  have_literal = 0;
  rx_pos = rx_len = 0;
  reply_bytes = 0;
  imap_error = "IMAP: no errors reported";
}

gxsIMAPClient::~gxsIMAPClient()
{
  CloseClient();
}

gxSocketError gxsIMAPClient::IMAPError(const char *mesg)
// Sets the error message and returns gxSOCKET_IMAP_ERROR, or the socket
// error if a socket error is already set
{
  imap_error << clear << "IMAP: " << mesg;
  if(socket_error == gxSOCKET_NO_ERROR) socket_error = gxSOCKET_IMAP_ERROR;
  return socket_error;
}

void gxsIMAPClient::LogLine(const char *dir, const gxString &s)
{
  if(!record_proto) return;
  proto_log << dir << s;
  if(s.is_null() || s[s.length()-1] != '\n') proto_log << "\n";
}

void gxsIMAPClient::NextTag(gxString &tag)
{
  char buf[32];
  tag_number++;
  if(tag_number > 9999999) tag_number = 1;
  sprintf(buf, "A%04d", tag_number);
  tag = buf;
}

gxString gxsIMAPClient::Quote(const char *s)
// Returns s as an IMAP quoted string, or an empty string if s cannot be
// quoted (CR, LF, NUL and 8-bit characters must be sent as literals)
{
  gxString q;
  if(!s) return q;
  q << "\"";
  for(const unsigned char *p = (const unsigned char *)s; *p; p++) {
    if(*p == '\r' || *p == '\n' || *p >= 0x80) {
      q.Clear();
      return q;
    }
    if(*p == '"' || *p == '\\') q << '\\';
    q << (char)*p;
  }
  q << "\"";
  return q;
}

// --------------------------------------------------------------
// Socket I/O
// --------------------------------------------------------------
int gxsIMAPClient::WriteBytes(const char *buf, unsigned n)
// Returns 0 if all n bytes were sent
{
  if(n == 0) return 0;
#if defined (__USE_GX_SSL_EXTENSIONS__)
  if(use_tls) {
    int rv = openssl.SendSSLSocket(buf, (int)n);
    if(rv != (int)n) {
      socket_error = gxSOCKET_TRANSMIT_ERROR;
      return -1;
    }
    return 0;
  }
#endif
  if(Send(buf, (int)n) != (int)n) {
    if(socket_error == gxSOCKET_NO_ERROR) socket_error = gxSOCKET_TRANSMIT_ERROR;
    return -1;
  }
  return 0;
}

int gxsIMAPClient::FillBuffer()
// Reads what the server has sent into rx_buf. Returns the number of
// bytes, 0 if the server closed the connection, or -1 for an error or
// a timeout.
{
  rx_pos = rx_len = 0;
  int n;
#if defined (__USE_GX_SSL_EXTENSIONS__)
  if(use_tls) {
    n = openssl.RecvSomeSSLSocket(rx_buf, (int)sizeof(rx_buf),
				  time_out_sec, time_out_usec);
    if(n < 0) {
      socket_error = (openssl.GetSSLError() == gxSSL_REQUEST_TIMEOUT) ?
	gxSOCKET_REQUEST_TIMEOUT : gxSOCKET_RECEIVE_ERROR;
      return -1;
    }
    if(n == 0) socket_error = gxSOCKET_DISCONNECTED_ERROR;
    rx_len = (unsigned)n;
    return n;
  }
#endif
  if(!ReadSelect(gxsocket, time_out_sec, time_out_usec)) {
    socket_error = gxSOCKET_REQUEST_TIMEOUT;
    return -1;
  }
  n = RawRead(rx_buf, (int)sizeof(rx_buf));
  if(n < 0) return -1;
  if(n == 0) socket_error = gxSOCKET_DISCONNECTED_ERROR;
  rx_len = (unsigned)n;
  return n;
}

int gxsIMAPClient::ReadLine(gxString &line)
// Reads one line, with its CRLF. Returns 0 if successful.
{
  line.Clear();
  for(;;) {
    if(rx_pos >= rx_len) {
      if(FillBuffer() <= 0) return -1;
    }
    char *start = rx_buf + rx_pos;
    char *nl = (char *)memchr(start, '\n', rx_len - rx_pos);
    unsigned n = nl ? (unsigned)(nl - start) + 1 : rx_len - rx_pos;
    reply_bytes += n;
    if(reply_bytes > max_reply) {
      socket_error = gxSOCKET_BUFOVER_ERROR;
      return -1;
    }
    line.Cat(start, n);
    rx_pos += n;
    if(nl) return 0;
  }
}

int gxsIMAPClient::ReadCount(unsigned n, gxString &data)
// Reads exactly n bytes (a literal). Returns 0 if successful.
{
  data.Clear();
  if(n > max_reply || reply_bytes + n > max_reply) {
    socket_error = gxSOCKET_BUFOVER_ERROR;
    return -1;
  }
  while(n > 0) {
    if(rx_pos >= rx_len) {
      if(FillBuffer() <= 0) return -1;
    }
    unsigned k = rx_len - rx_pos;
    if(k > n) k = n;
    data.Cat(rx_buf + rx_pos, k);
    rx_pos += k;
    n -= k;
    reply_bytes += k;
  }
  return 0;
}

static int imap_literal_size(const gxString &line, unsigned &size)
// Returns 1 if the line ends with a literal size, {n}CRLF
{
  int len = (int)line.length();
  const char *s = line.c_str();
  while(len > 0 && (s[len-1] == '\n' || s[len-1] == '\r')) len--;
  if(len < 3 || s[len-1] != '}') return 0;
  int i = len - 2;
  while(i >= 0 && s[i] >= '0' && s[i] <= '9') i--;
  if(i < 0 || s[i] != '{' || i == len - 2) return 0;
  if((len - 2 - i) > 10) return 0;
  unsigned long long v = strtoull(s + i + 1, 0, 10);
  if(v > 0xFFFFFFFFULL) return 0;
  size = (unsigned)v;
  return 1;
}

int gxsIMAPClient::ReadReply(const gxString &tag, gxString &reply,
			     int want_cont)
// Reads lines and literals until the line that starts with tag, or a
// continuation request ("+") if want_cont is set. Returns 0 if the
// status is OK, 1 for NO or BAD, 2 for a continuation request, -1 for
// a socket error.
{
  gxString line, lit;
  for(;;) {
    if(ReadLine(line) != 0) {
      imap_connected = 0;
      return -1;
    }
    reply << line;
    LogLine("S: ", line);
    // Literals: {n} at the end of the line, then n bytes, then the
    // rest of the line
    unsigned size;
    gxString logical = line;
    while(imap_literal_size(line, size)) {
      if(ReadCount(size, lit) != 0) {
	imap_connected = 0;
	return -1;
      }
      reply << lit;
      if(!have_literal) {
	last_literal = lit;
	have_literal = 1;
      }
      if(record_proto) {
	gxString note;
	note << clear << "(" << (int)size << " bytes)";
	LogLine("S: ", note);
      }
      if(ReadLine(line) != 0) {
	imap_connected = 0;
	return -1;
      }
      reply << line;
      LogLine("S: ", line);
      logical << line;
    }
    const char *s = logical.c_str();
    if(want_cont && s[0] == '+') return 2;
    {
      unsigned tl = tag.length();
      if(logical.length() > tl && strncmp(s, tag.c_str(), tl) == 0 &&
	 s[tl] == ' ') {
	gxString status = logical;
	status.DeleteAt(0, tl + 1);
	status.TrimTrailing('\n');
	status.TrimTrailing('\r');
	if(strncmp(status.c_str(), "OK", 2) == 0 &&
	   (status.length() == 2 || status[2] == ' ')) {
	  return 0;
	}
	imap_error << clear << "IMAP: server replied " << status;
	socket_error = gxSOCKET_IMAP_ERROR;
	return 1;
      }
    }
    if(strncmp(s, "* BYE", 5) == 0) {
      // The server is closing the connection. The tagged reply to
      // LOGOUT follows; for any other command the reply never comes.
      gxString bye = logical;
      bye.TrimTrailing('\n');
      bye.TrimTrailing('\r');
      imap_error << clear << "IMAP: " << bye;
    }
  }
}

gxSocketError gxsIMAPClient::Exchange(const gxString *parts, int num_parts,
				      gxString &reply, int no_log)
// Sends a tagged command made of parts. Every part but the last ends
// with a literal size {n}; the next part starts with the n bytes and
// is sent after the server's continuation request. Reads the reply.
{
  reply.Clear();
  last_literal.Clear();
  have_literal = 0;
  reply_bytes = 0;
  if(!imap_connected) return IMAPError("not connected to a server");
  socket_error = gxSOCKET_NO_ERROR;

  gxString tag, sbuf;
  NextTag(tag);
  for(int i = 0; i < num_parts; i++) {
    sbuf.Clear();
    if(i == 0) sbuf << tag << " ";
    sbuf << parts[i] << "\r\n";
    if(record_proto) {
      if(no_log) {
	gxString hidden;
	hidden << clear << (i == 0 ? tag.c_str() : "") << (i == 0 ? " " : "")
	       << "(not recorded)\r\n";
	LogLine("C: ", hidden);
      }
      else {
	LogLine("C: ", sbuf);
      }
    }
    // The CRLF of a part that ends in a literal size is part of the
    // command; the literal bytes follow after the continuation
    if(WriteBytes(sbuf.c_str(), sbuf.length()) != 0) {
      imap_connected = 0;
      sbuf.Clear(1);
      return IMAPError("could not send the command");
    }
    sbuf.Clear(1);
    if(i < num_parts - 1) {
      // The server asks for the literal with "+", or refuses the
      // command with a tagged NO or BAD
      int rv = ReadReply(tag, reply, 1);
      if(rv < 0) return IMAPError("no reply from the server");
      if(rv == 1) return gxSOCKET_IMAP_ERROR; // imap_error has the reply
      if(rv != 2) return IMAPError("no continuation request for a literal");
    }
  }
  int rv = ReadReply(tag, reply);
  if(rv < 0) return IMAPError("no reply from the server");
  if(rv > 0) return gxSOCKET_IMAP_ERROR; // imap_error has the reply
  return socket_error = gxSOCKET_NO_ERROR;
}

static void imap_add_arg(gxString *parts, int &n, const char *arg)
// Adds a quoted argument to the command in parts, or a literal if it
// cannot be quoted
{
  gxString q = gxsIMAPClient::Quote(arg);
  if(!q.is_null()) {
    parts[n] << " " << q;
    return;
  }
  parts[n] << " {" << (int)strlen(arg) << "}";
  n++;
  parts[n] = arg;
}

#if defined (__USE_GX_SSL_EXTENSIONS__)
gxString gxsIMAPClient::TLSFailMessage()
// The reason a TLS handshake failed, with the certificate check result
// when the handshake checked the server certificate (OpenSSL)
{
  gxString m = openssl.SSLExceptionMessage();
#if !defined (__USE_GX_SCHANNEL__)
  if(openssl.HasVerifyCert() && openssl.GetSSL()) {
    long v = SSL_get_verify_result(openssl.GetSSL());
    if(v != X509_V_OK) {
      m << ": server certificate: " << gxSSL::CertVerifyErrorString(v);
    }
  }
#endif
  return m;
}
#endif

// --------------------------------------------------------------
// Connection functions
// --------------------------------------------------------------
gxSocketError gxsIMAPClient::ConnectClient(const char *host, int port,
					   int tls)
{
  CloseClient();
  socket_error = gxSOCKET_NO_ERROR;
  if(!host || !*host) return IMAPError("no host name");
#if !defined (__USE_GX_SSL_EXTENSIONS__)
  if(tls) {
    socket_error = gxSOCKET_SSL_ERROR;
    return IMAPError("TLS needs a library built with __USE_GX_SSL_EXTENSIONS__");
  }
#endif
  server_host = host;
  if(InitSocketLibrary() != 0 ||
     InitSocket(SOCK_STREAM, port, (char *)host) < 0 || Connect() < 0) {
    gxSocketError err = socket_error;
    imap_error << clear << "IMAP: " << SocketExceptionMessage();
    Close();
    return socket_error = err;
  }
  imap_connected = 1;
  tag_number = 0;
  rx_pos = rx_len = 0;

  if(tls) {
#if defined (__USE_GX_SSL_EXTENSIONS__)
    if(!openssl.GetCTX()) {
      openssl.MakeClient();
      if(!openssl.InitCTX()) {
	CloseClient();
	socket_error = gxSOCKET_SSL_ERROR;
	return IMAPError(openssl.SSLExceptionMessage());
      }
    }
    if(!*openssl.HostName()) openssl.SetHostName(host);
    if(openssl.OpenSSLSocket(gxsocket) != gxSSL_NO_ERROR ||
       openssl.ConnectSSLSocket() != gxSSL_NO_ERROR) {
      gxString m = TLSFailMessage();
      CloseClient();
      socket_error = gxSOCKET_SSL_ERROR;
      return IMAPError(m.c_str());
    }
    if(openssl.HasVerifyCert() &&
       openssl.VerifyCert(openssl.HostName()) != gxSSL_NO_ERROR) {
      gxString m = openssl.SSLExceptionMessage();
      CloseClient();
      socket_error = gxSOCKET_SSL_ERROR;
      return IMAPError(m.c_str());
    }
    use_tls = 1;
#endif
  }

  // The greeting: "* OK" (or "* PREAUTH"); "* BYE" means no service
  gxString line;
  reply_bytes = 0;
  if(ReadLine(line) != 0) {
    CloseClient();
    return IMAPError("no greeting from the server");
  }
  LogLine("S: ", line);
  if(strncmp(line.c_str(), "* OK", 4) != 0 &&
     strncmp(line.c_str(), "* PREAUTH", 9) != 0) {
    line.TrimTrailing('\n');
    line.TrimTrailing('\r');
    CloseClient();
    socket_error = gxSOCKET_IMAP_ERROR;
    gxString m;
    m << "server refused the connection: " << line;
    return IMAPError(m.c_str());
  }
  return socket_error = gxSOCKET_NO_ERROR;
}

gxSocketError gxsIMAPClient::StartTLS()
{
#if defined (__USE_GX_SSL_EXTENSIONS__)
  if(use_tls) return IMAPError("the connection already uses TLS");
  gxString cmd = "STARTTLS", reply;
  gxSocketError err = Exchange(&cmd, 1, reply);
  if(err != gxSOCKET_NO_ERROR) return err;
  // Bytes sent after the OK, before the handshake, would be read as if
  // they came over TLS (command injection, CVE-2011-0411 and others)
  if(rx_pos < rx_len) {
    CloseClient();
    return IMAPError("the server sent data before the TLS handshake");
  }
  if(!openssl.GetCTX()) {
    openssl.MakeClient();
    if(!openssl.InitCTX()) {
      CloseClient();
      socket_error = gxSOCKET_SSL_ERROR;
      return IMAPError(openssl.SSLExceptionMessage());
    }
  }
  if(!*openssl.HostName()) openssl.SetHostName(server_host.c_str());
  if(openssl.OpenSSLSocket(gxsocket) != gxSSL_NO_ERROR ||
     openssl.ConnectSSLSocket() != gxSSL_NO_ERROR) {
    gxString m = TLSFailMessage();
    CloseClient();
    socket_error = gxSOCKET_SSL_ERROR;
    return IMAPError(m.c_str());
  }
  if(openssl.HasVerifyCert() &&
     openssl.VerifyCert(openssl.HostName()) != gxSSL_NO_ERROR) {
    gxString m = openssl.SSLExceptionMessage();
    CloseClient();
    socket_error = gxSOCKET_SSL_ERROR;
    return IMAPError(m.c_str());
  }
  use_tls = 1;
  return socket_error = gxSOCKET_NO_ERROR;
#else
  socket_error = gxSOCKET_SSL_ERROR;
  return IMAPError("TLS needs a library built with __USE_GX_SSL_EXTENSIONS__");
#endif
}

gxSocketError gxsIMAPClient::Login(const char *username, const char *password)
{
  if(!username || !password) return IMAPError("no user name or password");
  if(!use_tls && !allow_plain_login) {
    return IMAPError("refusing to send a password without TLS "
		     "(use TLS, StartTLS() or AllowPlainLogin())");
  }
  gxString parts[3], reply;
  int n = 0;
  parts[0] = "LOGIN";
  imap_add_arg(parts, n, username);
  imap_add_arg(parts, n, password);
  gxSocketError err = Exchange(parts, n + 1, reply, 1);
  for(int i = 0; i < 3; i++) parts[i].Clear(1);
  if(err != gxSOCKET_NO_ERROR) {
    gxString m = imap_error;
    m.DeleteAt(0, 6); // "IMAP: "
    imap_error << clear << "IMAP: login failed: " << m;
  }
  return err;
}

gxSocketError gxsIMAPClient::Logout()
{
  if(!imap_connected) return socket_error = gxSOCKET_NO_ERROR;
  gxString cmd = "LOGOUT", reply;
  gxSocketError err = Exchange(&cmd, 1, reply);
  CloseClient();
  return err;
}

void gxsIMAPClient::CloseClient()
{
#if defined (__USE_GX_SSL_EXTENSIONS__)
  if(use_tls) openssl.CloseSSLSocket();
#endif
  Close();
  imap_connected = 0;
  use_tls = 0;
  rx_pos = rx_len = 0;
}

// --------------------------------------------------------------
// Mailbox functions
// --------------------------------------------------------------
gxSocketError gxsIMAPClient::SelectMailbox(const char *mailbox,
					   int &num_messages, int read_only)
{
  num_messages = 0;
  if(!mailbox) return IMAPError("no mailbox name");
  gxString parts[2], reply;
  int n = 0;
  parts[0] = read_only ? "EXAMINE" : "SELECT";
  imap_add_arg(parts, n, mailbox);
  gxSocketError err = Exchange(parts, n + 1, reply);
  if(err != gxSOCKET_NO_ERROR) return err;
  // "* n EXISTS"
  int pos = 0;
  while((pos = reply.Find("* ", (unsigned)pos)) != -1) {
    if(pos == 0 || reply[pos-1] == '\n') {
      const char *p = reply.c_str() + pos + 2;
      char *end;
      long v = strtol(p, &end, 10);
      if(end != p && strncmp(end, " EXISTS", 7) == 0) {
	num_messages = (int)v;
	break;
      }
    }
    pos += 2;
  }
  return err;
}

static int imap_next_string(const char *&p, gxString &out)
// Reads an atom, a quoted string or a NIL at p. Returns 1 if one was
// read.
{
  out.Clear();
  while(*p == ' ') p++;
  if(*p == '"') {
    p++;
    while(*p && *p != '"') {
      if(*p == '\\' && p[1]) p++;
      out << *p++;
    }
    if(*p == '"') p++;
    return 1;
  }
  const char *start = p;
  while(*p && *p != ' ' && *p != '\r' && *p != '\n') p++;
  if(p == start) return 0;
  out.Cat(start, (unsigned)(p - start));
  return 1;
}

gxSocketError gxsIMAPClient::ListMailboxes(gxString &names,
					   const char *pattern)
{
  names.Clear();
  if(!pattern) pattern = "*";
  gxString parts[3], reply;
  int n = 0;
  parts[0] = "LIST \"\"";
  imap_add_arg(parts, n, pattern);
  gxSocketError err = Exchange(parts, n + 1, reply);
  if(err != gxSOCKET_NO_ERROR) return err;
  // * LIST (flags) "delimiter" name ; a name sent as a literal follows
  // the line that ends with {n}
  const char *s = reply.c_str();
  const char *end = s + reply.length();
  while(s < end) {
    const char *eol = (const char *)memchr(s, '\n', end - s);
    if(!eol) eol = end;
    if(strncmp(s, "* LIST ", 7) == 0) {
      const char *p = strchr(s, ')');
      if(p && p < eol) {
	p++;
	gxString delim, name;
	imap_next_string(p, delim);
	unsigned size;
	gxString line;
	line.Cat(s, (unsigned)(eol - s) + (eol < end ? 1 : 0));
	if(imap_literal_size(line, size)) {
	  const char *lit = eol + 1;
	  if(lit + size <= end) {
	    name.Cat(lit, size);
	    eol = lit + size;
	  }
	}
	else {
	  imap_next_string(p, name);
	}
	if(!name.is_null()) names << name << "\n";
      }
    }
    s = eol + 1;
  }
  return err;
}

gxSocketError gxsIMAPClient::Search(const char *criteria,
				    gxString &message_numbers)
{
  message_numbers.Clear();
  if(!criteria || !*criteria) criteria = "ALL";
  for(const char *p = criteria; *p; p++) {
    if(*p == '\r' || *p == '\n') {
      return IMAPError("CR or LF in the search criteria");
    }
  }
  gxString cmd, reply;
  cmd << "SEARCH " << criteria;
  gxSocketError err = Exchange(&cmd, 1, reply);
  if(err != gxSOCKET_NO_ERROR) return err;
  const char *s = reply.c_str();
  while(s && *s) {
    if(strncmp(s, "* SEARCH", 8) == 0) {
      const char *p = s + 8;
      while(*p && *p != '\r' && *p != '\n') {
	while(*p == ' ') p++;
	const char *start = p;
	while(*p >= '0' && *p <= '9') p++;
	if(p > start) {
	  if(!message_numbers.is_null()) message_numbers << " ";
	  message_numbers.Cat(start, (unsigned)(p - start));
	}
	else if(*p && *p != '\r' && *p != '\n') {
	  p++;
	}
      }
    }
    s = strchr(s, '\n');
    if(s) s++;
  }
  return err;
}

gxSocketError gxsIMAPClient::Noop()
{
  gxString cmd = "NOOP", reply;
  return Exchange(&cmd, 1, reply);
}

gxSocketError gxsIMAPClient::Capability(gxString &capabilities)
{
  capabilities.Clear();
  gxString cmd = "CAPABILITY", reply;
  gxSocketError err = Exchange(&cmd, 1, reply);
  if(err != gxSOCKET_NO_ERROR) return err;
  int pos = reply.Find("* CAPABILITY ");
  if(pos != -1) {
    const char *p = reply.c_str() + pos + 13;
    const char *e = p;
    while(*e && *e != '\r' && *e != '\n') e++;
    capabilities.Cat(p, (unsigned)(e - p));
  }
  return err;
}

// --------------------------------------------------------------
// Message functions
// --------------------------------------------------------------
gxSocketError gxsIMAPClient::FetchLiteral(int message_number,
					  const char *item, gxString &text)
{
  text.Clear();
  if(message_number <= 0) return IMAPError("invalid message number");
  gxString cmd, reply;
  cmd << "FETCH " << message_number << " (" << item << ")";
  gxSocketError err = Exchange(&cmd, 1, reply);
  if(err != gxSOCKET_NO_ERROR) return err;
  if(!have_literal) {
    // Some servers send a short value as a quoted string
    int pos = reply.Find("] \"");
    if(pos != -1) {
      const char *p = reply.c_str() + pos + 2;
      imap_next_string(p, text);
      return err;
    }
    if(reply.Find("] NIL") != -1) return err; // Empty
    return IMAPError("no message text in the reply");
  }
  text = last_literal;
  return err;
}

gxSocketError gxsIMAPClient::FetchHeader(int message_number, gxString &header)
{
  return FetchLiteral(message_number, "BODY.PEEK[HEADER]", header);
}

gxSocketError gxsIMAPClient::FetchBody(int message_number, gxString &body)
{
  return FetchLiteral(message_number, "BODY.PEEK[TEXT]", body);
}

gxSocketError gxsIMAPClient::FetchMessage(int message_number,
					  gxString &message)
{
  return FetchLiteral(message_number, "BODY.PEEK[]", message);
}

gxSocketError gxsIMAPClient::FetchDates(int message_number, 
					gxString &received_date,
					gxString &sent_date)
{
  received_date.Clear();
  sent_date.Clear();
  if(message_number <= 0) return IMAPError("invalid message number");
  gxString cmd, reply;
  cmd << "FETCH " << message_number 
      << " (INTERNALDATE BODY.PEEK[HEADER.FIELDS (DATE)])";
  gxSocketError err = Exchange(&cmd, 1, reply);
  if(err != gxSOCKET_NO_ERROR) return err;
  // INTERNALDATE "dd-Mon-yyyy hh:mm:ss +zzzz"
  int pos = reply.Find("INTERNALDATE ");
  if(pos != -1) {
    const char *p = reply.c_str() + pos + 13;
    imap_next_string(p, received_date);
  }
  // "Date: ..." from the header literal, folded lines joined
  gxString hdr = last_literal;
  int d = hdr.IFind("Date:");
  if(have_literal && d != -1) {
    const char *p = hdr.c_str() + d + 5;
    while(*p) {
      if(*p == '\r') { p++; continue; }
      if(*p == '\n') {
	if(p[1] == ' ' || p[1] == '\t') { p++; continue; }
	break;
      }
      sent_date << *p++;
    }
    sent_date.TrimLeadingSpaces();
    sent_date.TrimTrailingSpaces();
  }
  return err;
}

gxSocketError gxsIMAPClient::StoreFlags(int message_number, const char *flags,
					int add)
{
  if(message_number <= 0) return IMAPError("invalid message number");
  if(!flags || !*flags) return IMAPError("no flags");
  for(const char *p = flags; *p; p++) {
    if(*p == '\r' || *p == '\n' || *p == '(' || *p == ')') {
      return IMAPError("bad character in the flags");
    }
  }
  gxString cmd, reply;
  cmd << "STORE " << message_number << (add ? " +FLAGS.SILENT (" :
					" -FLAGS.SILENT (") << flags << ")";
  return Exchange(&cmd, 1, reply);
}

gxSocketError gxsIMAPClient::Expunge()
{
  gxString cmd = "EXPUNGE", reply;
  return Exchange(&cmd, 1, reply);
}

gxSocketError gxsIMAPClient::SendCommand(const char *command, gxString &reply)
{
  reply.Clear();
  if(!command || !*command) return IMAPError("no command");
  for(const char *p = command; *p; p++) {
    if(*p == '\r' || *p == '\n') return IMAPError("CR or LF in the command");
  }
  gxString cmd = command;
  return Exchange(&cmd, 1, reply);
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
