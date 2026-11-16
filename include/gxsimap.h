// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxsimap.h
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/16/2026
// Date Last Modified: 11/16/2026
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

Internet Message Access Protocol (IMAP) client class used by
applications that read and manage email on an IMAP server, the IMAP
counterpart of gxsPOP3Client. Written to RFC 3501 (IMAP4rev1) and RFC
2595 (STARTTLS). Based on the IMAP client written by Douglas Gaer in
2007 to automate the receipt of email messages.

Connections are made with TLS from the start (IMAPS, port 993) or in
plain text (port 143) followed by StartTLS(). TLS needs a library built
with __USE_GX_SSL_EXTENSIONS__ (OpenSSL, or Schannel on Windows). By
default Login() refuses to send a password over a connection without
TLS; AllowPlainLogin(1) turns that check off.

Every command is sent with its own tag (A0001, A0002, ...) and the
reply is read up to the line that starts with that tag. A reply is
successful only if that line says OK; NO and BAD replies return
gxSOCKET_IMAP_ERROR with the server's text in IMAPExceptionMessage().
Literals ({n} followed by n bytes) are read by count, so message text
cannot end a reply early. Message numbers are sequence numbers in the
selected mailbox.

*/
// ----------------------------------------------------------- //
#ifndef __GX_EMBEDDED_IMAP_LIB_HPP__
#define __GX_EMBEDDED_IMAP_LIB_HPP__

#include "gxdlcode.h"

#include "gxsocket.h"
#include "gxstring.h"
#if defined (__USE_GX_SSL_EXTENSIONS__)
#include "gxssl.h"
#endif

// Default largest reply read for one command, in bytes
const unsigned gxsIMAP_DEF_MAX_REPLY = 64 * 1024 * 1024;

// IMAP client - See RFC 3501 for specifications
class GXDLCODE_API gxsIMAPClient : public gxSocket
{
public:
  gxsIMAPClient();
  ~gxsIMAPClient();

public: // Connection functions
  // Connects to the server and reads its greeting. With use_tls the TLS
  // handshake is made first (IMAPS). The host name is used for the
  // server name (SNI) and for VerifyCert() if openssl.RequireVerifyCert()
  // was set. Returns 0 if successful.
  gxSocketError ConnectClient(const char *host,
			      int port = gxSOCKET_SIMAP_PORT,
			      int use_tls = 1);
  // STARTTLS on a plain text connection (port 143). Returns 0 if
  // successful.
  gxSocketError StartTLS();
  gxSocketError Login(const char *username, const char *password);
  // LOGOUT, then closes the connection
  gxSocketError Logout();
  void CloseClient();
  int IMAPConnected() { return imap_connected; }
  int IsTLS() { return use_tls; }
  void AllowPlainLogin(int allow = 1) { allow_plain_login = allow; }

public: // Mailbox functions
  // SELECT (or EXAMINE if read_only) a mailbox. num_messages is the
  // EXISTS count. Mailbox names that are not ASCII must be passed in
  // modified UTF-7 (RFC 3501 5.1.3).
  gxSocketError SelectMailbox(const char *mailbox, int &num_messages,
			      int read_only = 0);
  // LIST: one mailbox name per line in names
  gxSocketError ListMailboxes(gxString &names, const char *pattern = "*");
  // SEARCH with criteria such as "ALL", "UNSEEN" or "SINCE 1-Nov-2026".
  // The message numbers are passed back separated by spaces.
  gxSocketError Search(const char *criteria, gxString &message_numbers);
  gxSocketError Noop();
  gxSocketError Capability(gxString &capabilities);

public: // Message functions. The fetch functions do not set \Seen.
  gxSocketError FetchHeader(int message_number, gxString &header);
  gxSocketError FetchBody(int message_number, gxString &body);
  gxSocketError FetchMessage(int message_number, gxString &message);
  // The date the server received the message (INTERNALDATE) and the
  // message's Date: header
  gxSocketError FetchDates(int message_number, gxString &received_date,
			   gxString &sent_date);
  // STORE +FLAGS (or -FLAGS if add is 0), for example "\\Seen"
  gxSocketError StoreFlags(int message_number, const char *flags,
			   int add = 1);
  gxSocketError MarkSeen(int message_number) {
    return StoreFlags(message_number, "\\Seen");
  }
  gxSocketError MarkDeleted(int message_number) {
    return StoreFlags(message_number, "\\Deleted");
  }
  gxSocketError Expunge();

public: // Command functions
  // Sends a command (without the tag) and reads the whole reply, the
  // untagged lines, literals and the tagged status line, into reply.
  // Returns 0 if the status is OK.
  gxSocketError SendCommand(const char *command, gxString &reply);
  // Returns a quoted string for a command argument, or an empty string
  // if s has CR, LF, NUL or 8-bit characters (send those with
  // SendCommand() and a literal).
  static gxString Quote(const char *s);
  // The text of the first literal in the last reply (the message text
  // of a FETCH)
  const gxString &LastLiteral() { return last_literal; }

public: // Error and setting functions
  const char *IMAPExceptionMessage() { return imap_error.c_str(); }
  void SetTimeOut(int seconds, int useconds = 0) {
    time_out_sec = seconds;
    time_out_usec = useconds;
  }
  void SetMaxReply(unsigned bytes) { max_reply = bytes; }
  // Keep a copy of the session in ProtoLog(). Passwords are not kept.
  void RecordSession(int record = 1) { record_proto = record; }
  const gxString &ProtoLog() { return proto_log; }

private:
  gxSocketError IMAPError(const char *mesg);
  gxSocketError Exchange(const gxString *parts, int num_parts,
			 gxString &reply, int no_log = 0);
  int ReadLine(gxString &line);
  int ReadCount(unsigned n, gxString &data);
  int FillBuffer();
  int WriteBytes(const char *buf, unsigned n);
  int ReadReply(const gxString &tag, gxString &reply, int want_cont = 0);
  void NextTag(gxString &tag);
  gxSocketError FetchLiteral(int message_number, const char *item,
			     gxString &text);
  void LogLine(const char *dir, const gxString &s);
#if defined (__USE_GX_SSL_EXTENSIONS__)
  gxString TLSFailMessage();
#endif

public:
#if defined (__USE_GX_SSL_EXTENSIONS__)
  gxSSL openssl; // TLS settings: SetCAList(), RequireVerifyCert(), ...
#endif

private:
  int imap_connected; // A connection is open and not closed by the server
  int use_tls;
  gxString server_host; // Host name given to ConnectClient()
  int allow_plain_login;
  int tag_number;
  int time_out_sec;  // Seconds to wait for data from the server
  int time_out_usec;
  unsigned max_reply;
  int record_proto;
  gxString proto_log;
  gxString imap_error;
  gxString last_literal;
  int have_literal;
  char rx_buf[4096];
  unsigned rx_pos;
  unsigned rx_len;
  unsigned reply_bytes;
};

#endif // __GX_EMBEDDED_IMAP_LIB_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
