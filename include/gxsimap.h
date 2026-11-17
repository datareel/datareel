// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxsimap.h
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 04/17/2007
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

Internet Message Access Protocol (IMAP) client used to read and
automate the receipt of email messages.

This client was written in accordance with the following Internet
Request for Comments:

RFC 3501 - Internet Message Access Protocol - Version 4rev1
RFC 8314 - Use of TLS for Email Submission and Access (port 993)

Changes:
==============================================================
04/17/2007: gxsIMAPClient written by Douglas Gaer for a secure IMAP
client program that downloads report messages to a spool folder.

11/16/2026: DataReel 7.4 adds gxsIMAPClient to the library, based on
that 2007 class and released under the LGPL. Its structure and calls are kept (ConnectClient(),
Login(), Logout(), SelectMailbox(), FetchHeader(), FetchBody(),
SendCommand(), RecvBuffer(), ParseResponse() and the recorded
session with the password masked), with:

  - The DataReel 7 mail interface of gxsPOP3Client: Connect() with
    implicit TLS (port 993), STARTTLS (port 143) or, only when asked
    for by name, plain text (ConnectPlain(), port 143); Login(user,
    password); Command(); Error() and LastReply(); std::string
    results. TLS goes through gxsTLSConnection and gxSSL, with the
    server certificate and host name checked by default.
  - Replies read by their tagged completion line. The old client
    stopped reading when "NO" or "BAD" appeared anywhere in the
    data, so a message containing those letters was cut short.
  - Message data read as IMAP literals ({size} then that many
    bytes) or quoted strings. The old FetchBody() cut the body at
    its first "}" and at the command tag's text.
  - One tag a command (A0001, A0002, ...); the old GetTag() counted
    two a command.
  - Search(), ListMailboxes(), Examine(), Capability(), Noop(),
    Delete() / Expunge() and FetchMessage() / FetchInternalDate().
  - Quoted strings for user names, passwords and mailbox names (\\
    and " escaped; the 2007 client sent LOGIN arguments as they were),
    and synchronizing literals for 8-bit ones. A user name or password
    with a line break or a NUL is refused before anything is sent. A
    server that announces LOGINDISABLED is not sent a password.
  - Replies stop at SetMaxSize() bytes (64 MB by default).
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_EMBEDDED_IMAP_LIB_HPP__
#define __GX_EMBEDDED_IMAP_LIB_HPP__

#include "gxdlcode.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "gxsmtp.h" // gxsMailSecurity
#include "gxstls.h"

namespace datareel {

const int gxsIMAP_RECV_TIMEOUT_SECS = 300; // 5 minute timeout (as in 2007)

// One untagged reply ("* ...") of a command. Literals are taken out of
// the text: each one leaves a NUL, its index and a NUL in text, and its
// bytes in literals[index].
struct GXDLCODE_API gxsIMAPUntagged
{
  std::string text;
  std::vector<std::string> literals;
};

// IMAP4rev1 client (RFC 3501). Connect() uses TLS; ConnectPlain() is
// plain text, for a server on the same host or a test.
class GXDLCODE_API gxsIMAPClient
{
public:
  gxsIMAPClient() { conn.SetTimeout(gxsIMAP_RECV_TIMEOUT_SECS); }
  ~gxsIMAPClient() { Close(); }

  gxsIMAPClient(const gxsIMAPClient &) = delete;
  gxsIMAPClient &operator=(const gxsIMAPClient &) = delete;

public: // Settings used before Connect()
  void SetTimeout(int seconds) { conn.SetTimeout(seconds); }
  void SetCAFile(const std::string &fname) { conn.SetCAFile(fname); }
  void SetVerifyPeer(bool on) { conn.SetVerifyPeer(on); }
  void SetMaxSize(std::size_t n) { max_size = n; } // Largest reply kept
  // 11/16/2026 (beta5): Login() on a connection without TLS sends the
  // password as plain text, so it is refused unless this is called first
  // (a server on the same host or a test). TLS connections ignore it.
  void AllowPlainLogin(bool on = true) { allow_plain_login = on; }

public: // Connections. Each returns 0 if successful, or -1 (see Error())
  // Implicit TLS (port 993) or STARTTLS on the plain port (143). A
  // server that does not accept STARTTLS is refused.
  int Connect(const std::string &host, int port = gxSOCKET_SIMAP_PORT,
	      gxsMailSecurity security = gxsMAIL_IMPLICIT_TLS);
  // Plain text, user name and password included. Use it only for a
  // server on the same host or a private test network; Login() on it
  // needs AllowPlainLogin().
  int ConnectPlain(const std::string &host, int port = gxSOCKET_IMAP_PORT);
  // 2007 name: implicit TLS on port
  int ConnectClient(const std::string &host, int port = gxSOCKET_SIMAP_PORT) {
    return Connect(host, port, gxsMAIL_IMPLICIT_TLS);
  }
  void Close() { conn.Close(); connected = false; }
  int CloseClient() { Close(); return 0; }   // 2007 name
  bool IsConnected() const { return connected && conn.IsOpen(); }
  bool IsSecure() const { return conn.IsSecure(); }

public: // Session. Each returns 0 if the server replied OK
  int Login(const std::string &user, const std::string &passwd);
  // 2007 form: connects with implicit TLS first if not connected
  int Login(const std::string &host, int port, const std::string &user,
	    const std::string &passwd);
  int Logout(int close_sockets = 1);
  int Capability(std::vector<std::string> &caps);
  bool HasCapability(const std::string &cap) const;
  int Noop();

public: // Mailboxes
  int SelectMailbox(const std::string &mailbox, int &num_messages);
  // 2007 form: rxbuf gets the server's reply lines
  int SelectMailbox(const std::string &mailbox, std::string &rxbuf,
		    int &num_messages);
  int Examine(const std::string &mailbox, int &num_messages); // Read-only
  // Mailbox names matching pattern ("*" every one, "%" the top level)
  int ListMailboxes(std::vector<std::string> &names,
		    const std::string &pattern = "*");

public: // Messages, by message sequence number (1 to num_messages)
  // Numbers matching search criteria such as "ALL", "UNSEEN" or
  // "SUBJECT \"report\"" (the criteria are sent as given)
  int Search(const std::string &criteria, std::vector<unsigned> &numbers);
  // The header lines (with CR LF); FetchBody() and FetchMessage() do
  // not set the \Seen flag either (BODY.PEEK)
  int FetchHeader(int message_number, std::string &header);
  // 2007 form: also the server's arrival time (INTERNALDATE) and the
  // Date: of the message's envelope
  int FetchHeader(int message_number, std::string &header,
		  std::string &datetime, std::string &sent_datetime);
  int FetchBody(int message_number, std::string &body); // The text
  int FetchMessage(int message_number, std::string &message); // Whole
  int FetchInternalDate(int message_number, std::string &datetime);
  int Delete(int message_number); // Sets \Deleted
  int Expunge();                   // Removes the \Deleted messages

public: // Commands
  // Send a command (the tag is added) and read the reply up to its
  // tagged completion. Returns 0 for OK, or -1 for NO, BAD, BYE or an
  // error. The untagged replies are in Untagged().
  int Command(const std::string &cmd);
  // 2007 calls. SendCommand() sends one command and returns its tag;
  // RecvBuffer() reads the reply to tag into buf (untagged lines,
  // literals in place, then the tagged line).
  int SendCommand(const std::string &command, std::string &tag);
  int RecvBuffer(const std::string &tag, std::string &buf);

public: // Parsers, used on the connection and in tests
  // 2007 ParseResponse(): find the first line in rxbuf that starts
  // with start_str, ends with end_str and contains line_str (or any
  // line if line_str is empty). With parse_mid_str the text between
  // start_str and line_str is returned, trimmed. Returns 1 if found.
  static int ParseResponse(const std::string &rxbuf,
			   const std::string &start_str,
			   const std::string &end_str,
			   const std::string &line_str, std::string &line,
			   int parse_mid_str);
  // The value of a FETCH item (such as "BODY[TEXT]" or "INTERNALDATE")
  // in an untagged FETCH reply: a literal, a quoted string or NIL.
  // Returns 0, or -1 if the item is not there.
  static int FetchItem(const gxsIMAPUntagged &u, const std::string &item,
		       std::string &value);
  // Quote s as an IMAP quoted string. Returns false if it cannot be
  // quoted (CR, LF, NUL or 8-bit characters: send a literal).
  static bool Quote(const std::string &s, std::string &quoted);

public: // Status
  const std::string &LastReply() const { return last_reply; } // Tagged line
  const std::string &Error() const { return error; }
  const char *IMAPExceptionMessage() const { return error.c_str(); } // 2007
  const std::vector<gxsIMAPUntagged> &Untagged() const { return untagged; }
  gxsTLSConnection &Connection() { return conn; }

public: // Recorded session (2007 record_proto), LOGIN arguments masked
  void SetRecordProto(bool on) { record_proto = on; }
  const std::string &ProtoString() const { return proto_string; }

private:
  int greeting();
  int send_parts(const std::vector<std::pair<bool, std::string>> &parts,
		 std::string &tag, bool mask);
  int read_reply(const std::string &tag, std::string *raw);
  int read_line_with_literals(gxsIMAPUntagged &u, std::string *raw,
			      std::size_t &total);
  int select_cmd(const char *verb, const std::string &mailbox,
		 int &num_messages, std::string *raw);
  int fetch_one(int message_number, const std::string &request,
		const std::string &item, std::string &value);
  int mailbox_arg(const std::string &mailbox,
		  std::vector<std::pair<bool, std::string>> &parts,
		  const std::string &before);
  void record(const char *who, std::string_view s);
  int fail(const std::string &msg) { error = msg; return -1; }

private:
  gxsTLSConnection conn;
  bool connected = false;
  unsigned tag_number = 0;
  std::size_t max_size = 64*1024*1024;
  std::vector<std::string> capabilities;
  std::vector<gxsIMAPUntagged> untagged;
  std::string last_reply;
  std::string error;
  bool record_proto = false;
  bool allow_plain_login = false;
  std::string proto_string;
};

} // namespace datareel

#endif // __GX_EMBEDDED_IMAP_LIB_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
