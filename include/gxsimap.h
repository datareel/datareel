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
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

Internet Message Access Protocol (IMAP) client class used by
applications that read and manage email on an IMAP server, the IMAP
counterpart of gxsPOP3Client. Written to RFC 3501 (IMAP4rev1) and RFC
2595 (STARTTLS). Based on work from the 2007 IMAP client written to
automate the receipt of email messages.

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

#include <string>
#include <vector>
#include <cstddef>
#include "gxsocket.h"
#include "gxstring.h"
#if defined (__USE_GX_SSL_EXTENSIONS__)
#include "gxssl.h"
#endif

// Default largest reply read for one command, in bytes
const unsigned gxsIMAP_DEF_MAX_REPLY = 64 * 1024 * 1024;
const int gxsIMAP_RECV_TIMEOUT_SECS = 300; // 5 minutes (2007 client, 7.4)

#ifndef __GX_MAIL_SECURITY_ENUM__
#define __GX_MAIL_SECURITY_ENUM__
// The DataReel 7.4 mail security choice, for Connect()
enum gxsMailSecurity {
  gxsMAIL_IMPLICIT_TLS, // TLS from the start (IMAPS 993)
  gxsMAIL_STARTTLS      // Plain greeting, then STARTTLS (143)
};
#endif

// One untagged reply ("* ...") of a command, as in DataReel 7.4.
// Literals are taken out of the text: each one leaves a NUL, its index
// and a NUL in text, and its bytes in literals[index].
struct GXDLCODE_API gxsIMAPUntagged
{
  std::string text;
  std::vector<std::string> literals;
};

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
  // LOGOUT, then closes the connection (or only sends LOGOUT if
  // close_sockets is 0). Returns 0 if successful.
  int Logout(int close_sockets = 1);
  int CloseClient();
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

public: // ------------------------------------------------------------
  // The DataReel 7.4 interface, with the 2007 work client's calls, so a
  // program builds against either version. Each returns 0 if successful
  // (the server replied OK) or -1; Error() has the reason.
  // ------------------------------------------------------------
  // Settings used before Connect()
  void SetTimeout(int seconds) { SetTimeOut(seconds, 0); }
  void SetCAFile(const std::string &fname);
  void SetVerifyPeer(bool on);
  void SetMaxSize(std::size_t n) { max_reply = (unsigned)(n > 0xFFFFFFFFUL ? 0xFFFFFFFFUL : n); }
  // Implicit TLS (port 993) or STARTTLS on the plain port (143). A server
  // that does not accept STARTTLS is refused.
  int Connect(const std::string &host, int port = gxSOCKET_SIMAP_PORT,
	      gxsMailSecurity security = gxsMAIL_IMPLICIT_TLS);
  // Plain text, user name and password included. Use it only for a
  // server on the same host or a private test network.
  int ConnectPlain(const std::string &host, int port = gxSOCKET_IMAP_PORT);
  // 2007 form: implicit TLS
  int ConnectClient(const std::string &host, int port = gxSOCKET_SIMAP_PORT) {
    return ConnectClient(host.c_str(), port, 1) == gxSOCKET_NO_ERROR ? 0 : -1;
  }
  void Close() { CloseClient(); }
  bool IsConnected() { return imap_connected != 0; }
  bool IsSecure() { return use_tls != 0; }
  int Login(const std::string &user, const std::string &passwd);
  // 2007 form: connects with implicit TLS first if not connected
  int Login(const std::string &host, int port, const std::string &user,
	    const std::string &passwd);
  int Capability(std::vector<std::string> &caps);
  bool HasCapability(const std::string &cap) const;
  int SelectMailbox(const std::string &mailbox, int &num_messages);
  // 2007 form: rxbuf gets the server's reply lines
  int SelectMailbox(const std::string &mailbox, std::string &rxbuf,
		    int &num_messages);
  int Examine(const std::string &mailbox, int &num_messages); // Read-only
  int ListMailboxes(std::vector<std::string> &names,
		    const std::string &pattern = "*");
  int Search(const std::string &criteria, std::vector<unsigned> &numbers);
  int FetchHeader(int message_number, std::string &header);
  // 2007 form: also INTERNALDATE and the envelope's Date:
  int FetchHeader(int message_number, std::string &header,
		  std::string &datetime, std::string &sent_datetime);
  int FetchBody(int message_number, std::string &body);
  int FetchMessage(int message_number, std::string &message);
  int FetchInternalDate(int message_number, std::string &datetime);
  int Delete(int message_number); // Sets \Deleted; Expunge() removes it
  // Sends a command (the tag is added) and reads the reply up to its
  // tagged completion. The untagged replies are in Untagged().
  int Command(const std::string &cmd);
  // 2007 calls: SendCommand() sends one command and passes back its tag;
  // RecvBuffer() reads the reply to that tag into buf.
  int SendCommand(const std::string &command, std::string &tag);
  int RecvBuffer(const std::string &tag, std::string &buf);
  // 2007 ParseResponse(): the first line in rxbuf that starts with
  // start_str, ends with end_str and contains line_str (or any line if
  // line_str is empty); with parse_mid_str the text between start_str
  // and line_str, trimmed. Returns 1 if found.
  static int ParseResponse(const std::string &rxbuf,
			   const std::string &start_str,
			   const std::string &end_str,
			   const std::string &line_str, std::string &line,
			   int parse_mid_str);
  // The value of a FETCH item (such as "BODY[TEXT]" or "INTERNALDATE")
  // in an untagged FETCH reply: a literal, a quoted string or NIL.
  static int FetchItem(const gxsIMAPUntagged &u, const std::string &item,
		       std::string &value);
  static bool Quote(const std::string &s, std::string &quoted);
  const std::string &LastReply() const { return last_reply; } // Tagged line
  std::string Error() const { return std::string(imap_error.c_str()); }
  const std::vector<gxsIMAPUntagged> &Untagged() const { return untagged; }
  void SetRecordProto(bool on) { record_proto = on ? 1 : 0; }
  std::string ProtoString() const { return std::string(proto_log.c_str(), proto_log.length()); }

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
  gxSocketError SendParts(const gxString *parts, int num_parts,
			  gxString &tag, gxString &reply, int no_log);
  int Fail(const char *mesg);
  int FetchOne(int message_number, const std::string &request,
	       const std::string &item, std::string &value);
  int SelectCmd(const char *verb, const std::string &mailbox,
		int &num_messages, std::string *raw);
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
  std::vector<gxsIMAPUntagged> untagged; // Untagged replies of the last command
  std::string last_reply;               // Its tagged status line
  std::vector<std::string> capabilities;
};

#endif // __GX_EMBEDDED_IMAP_LIB_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
