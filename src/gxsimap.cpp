// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxsimap.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 04/17/2007
// Date Last Modified: 11/16/2026
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

IMAP client (RFC 3501). See gxsimap.h for the changes since the
2007 class this one is based on.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cstdio>
#include <cstdlib>
#include "gxsimap.h"
#include "strutil.h"

namespace datareel {

typedef std::vector<std::pair<bool, std::string>> imap_parts; // (literal, text)

// -----------------------------------------------------------
// Tokens of a reply's text. A literal is a NUL, its index and a NUL.
// -----------------------------------------------------------
static void skip_spaces(const std::string &t, std::size_t &pos)
{
  while(pos < t.size() && t[pos] == ' ') pos++;
}

// Read one string value: a quoted string, a literal, NIL or an atom.
// Returns false if there is none at pos.
static bool read_string(const std::string &t, std::size_t &pos,
			const std::vector<std::string> &literals,
			std::string &value, bool *is_nil = 0)
{
  value.clear();
  if(is_nil) *is_nil = false;
  skip_spaces(t, pos);
  if(pos >= t.size()) return false;
  if(t[pos] == '"') {
    for(pos++; pos < t.size(); pos++) {
      if(t[pos] == '\\' && pos + 1 < t.size()) { value += t[++pos]; continue; }
      if(t[pos] == '"') { pos++; return true; }
      value += t[pos];
    }
    return false; // No closing quote
  }
  if(t[pos] == '\0') {
    std::size_t end = t.find('\0', pos + 1);
    if(end == std::string::npos) return false;
    std::size_t idx = (std::size_t)std::strtoul(t.c_str() + pos + 1, 0, 10);
    if(idx >= literals.size()) return false;
    value = literals[idx];
    pos = end + 1;
    return true;
  }
  if(t[pos] == '(' || t[pos] == ')') return false;
  std::size_t start = pos;
  while(pos < t.size() && t[pos] != ' ' && t[pos] != '(' && t[pos] != ')') {
    if(t[pos] == '[') { // BODY[HEADER.FIELDS (DATE)]
      std::size_t close = t.find(']', pos);
      if(close == std::string::npos) return false;
      pos = close + 1;
      continue;
    }
    pos++;
  }
  value.assign(t, start, pos - start);
  if(iequals(value, "NIL")) { value.clear(); if(is_nil) *is_nil = true; }
  return pos > start;
}

// Skip one value of any kind, parenthesized lists included
static bool skip_value(const std::string &t, std::size_t &pos)
{
  static const std::vector<std::string> none;
  skip_spaces(t, pos);
  if(pos >= t.size()) return false;
  if(t[pos] == '(') {
    pos++;
    for(;;) {
      skip_spaces(t, pos);
      if(pos >= t.size()) return false;
      if(t[pos] == ')') { pos++; return true; }
      if(!skip_value(t, pos)) return false;
    }
  }
  if(t[pos] == '\0') {
    std::size_t end = t.find('\0', pos + 1);
    if(end == std::string::npos) return false;
    pos = end + 1;
    return true;
  }
  std::string ignored;
  if(t[pos] == '"') {
    // Literal indexes are not needed to skip a quoted string
    return read_string(t, pos, none, ignored);
  }
  return read_string(t, pos, none, ignored);
}

// Leading message number of "N EXISTS", "N FETCH (...)"
static bool leading_number(const std::string &t, unsigned &n, std::size_t &pos)
{
  pos = 0;
  n = 0;
  while(pos < t.size() && pos < 10 && t[pos] >= '0' && t[pos] <= '9') {
    n = n * 10 + (unsigned)(t[pos] - '0');
    pos++;
  }
  return pos > 0 && (pos == t.size() || t[pos] == ' ');
}

static void parse_capabilities(const std::string &list,
			       std::vector<std::string> &caps)
{
  caps.clear();
  for(const std::string &c : split(list, ' ', true)) {
    caps.push_back(to_upper(c));
  }
}

// -----------------------------------------------------------
// Static parsers
// -----------------------------------------------------------
bool gxsIMAPClient::Quote(const std::string &s, std::string &quoted)
{
  quoted = "\"";
  for(unsigned char c : s) {
    if(c == 0 || c == '\r' || c == '\n' || c > 0x7E) return false;
    if(c == '"' || c == '\\') quoted += '\\';
    quoted += (char)c;
  }
  quoted += '"';
  return true;
}

int gxsIMAPClient::FetchItem(const gxsIMAPUntagged &u, const std::string &item,
			     std::string &value)
{
  value.clear();
  const std::string &t = u.text;
  unsigned n;
  std::size_t pos;
  if(!leading_number(t, n, pos)) return -1;
  skip_spaces(t, pos);
  if(!istarts_with(std::string_view(t).substr(pos), "FETCH")) return -1;
  pos += 5;
  skip_spaces(t, pos);
  if(pos >= t.size() || t[pos] != '(') return -1;
  pos++;
  for(;;) {
    skip_spaces(t, pos);
    if(pos >= t.size() || t[pos] == ')') return -1;
    std::string name;
    if(!read_string(t, pos, u.literals, name)) return -1;
    skip_spaces(t, pos);
    if(iequals(name, item)) {
      if(pos < t.size() && t[pos] == '(') { // A list: return its text
	std::size_t start = pos;
	if(!skip_value(t, pos)) return -1;
	value.assign(t, start, pos - start);
	return 0;
      }
      return read_string(t, pos, u.literals, value) ? 0 : -1;
    }
    if(!skip_value(t, pos)) return -1;
  }
}

int gxsIMAPClient::ParseResponse(const std::string &rxbuf,
				 const std::string &start_str,
				 const std::string &end_str,
				 const std::string &line_str, std::string &line,
				 int parse_mid_str)
{
  line.clear();
  if(start_str.empty() || end_str.empty()) return 0;
  std::size_t offset = 0;
  while(offset < rxbuf.size()) {
    offset = rxbuf.find(start_str, offset);
    if(offset == std::string::npos) break;
    std::size_t end_of_line = rxbuf.find(end_str, offset);
    if(end_of_line == std::string::npos) break;
    std::string tmp_line = rxbuf.substr(offset, end_of_line - offset + end_str.size());
    if(line_str.empty()) { // The line with the start and end strings
      line = tmp_line;
      return 1;
    }
    std::size_t mid = tmp_line.find(line_str, start_str.size());
    if(mid != std::string::npos) {
      if(parse_mid_str) { // The text between start_str and line_str
	tmp_line = tmp_line.substr(start_str.size(), mid - start_str.size());
	line = trim(tmp_line);
      }
      else {
	line = tmp_line;
      }
      return 1;
    }
    offset++;
  }
  return 0;
}

// -----------------------------------------------------------
// Sending and receiving
// -----------------------------------------------------------
void gxsIMAPClient::record(const char *who, std::string_view s)
{
  if(!record_proto) return;
  proto_string += who;
  proto_string.append(s.data(), s.size());
  if(s.empty() || s.back() != '\n') proto_string += "\r\n";
}

int gxsIMAPClient::read_line_with_literals(gxsIMAPUntagged &u, std::string *raw,
					   std::size_t &total)
{
  u.text.clear();
  u.literals.clear();
  for(;;) {
    std::string line;
    if(!conn.ReadLine(line, 1024*1024)) {
      Close();
      return fail("IMAP exception: No reply from server" +
		  (conn.Error().empty() ? std::string() : ": " + conn.Error()));
    }
    total += line.size() + 2;
    if(total > max_size) {
      Close(); // The rest of the reply is not read
      return fail("IMAP exception: Reply is larger than " +
		  std::to_string(max_size) + " bytes");
    }
    if(line.find('\0') != std::string::npos) {
      Close();
      return fail("IMAP exception: The server sent a NUL character");
    }
    record("server->", line);
    if(raw) { *raw += line; *raw += "\r\n"; }

    // A literal: the line ends in {size} (or {size+}), then size bytes
    std::size_t open = std::string::npos;
    if(!line.empty() && line.back() == '}') open = line.rfind('{');
    std::size_t bytes = 0;
    bool is_literal = false;
    if(open != std::string::npos) {
      std::string digits = line.substr(open + 1, line.size() - open - 2);
      if(!digits.empty() && digits.back() == '+') digits.pop_back();
      if(!digits.empty() && digits.size() <= 18 &&
	 digits.find_first_not_of("0123456789") == std::string::npos) {
	bytes = (std::size_t)std::strtoull(digits.c_str(), 0, 10);
	is_literal = true;
      }
    }
    if(!is_literal) {
      u.text += line;
      return 0;
    }
    if(bytes > max_size || total + bytes > max_size) {
      Close();
      return fail("IMAP exception: Reply is larger than " +
		  std::to_string(max_size) + " bytes");
    }
    std::string data;
    if(conn.Read(data, bytes) != (int)bytes) {
      Close();
      return fail("IMAP exception: Incomplete reply" +
		  (conn.Error().empty() ? std::string() : ": " + conn.Error()));
    }
    total += bytes;
    if(record_proto) {
      proto_string += "server-> (" + std::to_string(bytes) + " bytes)\r\n";
    }
    if(raw) *raw += data;
    u.text.append(line, 0, open);
    u.text += '\0';
    u.text += std::to_string(u.literals.size());
    u.text += '\0';
    u.literals.push_back(std::move(data));
  }
}

int gxsIMAPClient::read_reply(const std::string &tag, std::string *raw)
{
  untagged.clear();
  last_reply.clear();
  std::size_t total = 0;
  std::string bye;
  for(;;) {
    gxsIMAPUntagged u;
    if(read_line_with_literals(u, raw, total) != 0) {
      if(!bye.empty()) error = "IMAP exception: Server closed the session: " + bye;
      return -1;
    }
    if(starts_with(u.text, "* ")) {
      u.text.erase(0, 2);
      if(istarts_with(u.text, "BYE")) bye = u.text;
      if(istarts_with(u.text, "CAPABILITY ")) {
	parse_capabilities(u.text.substr(11), capabilities);
      }
      untagged.push_back(std::move(u));
      continue;
    }
    if(starts_with(u.text, tag + " ")) {
      last_reply = u.text;
      std::string status = u.text.substr(tag.size() + 1);
      if(istarts_with(status, "OK")) return 0;
      for(char &c : status) if(c == '\0') c = ' ';
      return fail("IMAP exception: " + status);
    }
    if(starts_with(u.text, "+")) {
      Close(); // The server waits for data this client never sends
      return fail("IMAP exception: Unexpected continuation request");
    }
    // Anything else is not IMAP
    Close();
    return fail("IMAP exception: Unexpected reply: " + u.text.substr(0, 200));
  }
}

int gxsIMAPClient::send_parts(const imap_parts &parts, std::string &tag, bool mask)
{
  if(!conn.IsOpen()) {
    connected = false;
    return fail("IMAP exception: Client is not connected to server");
  }
  char buf[32];
  std::snprintf(buf, sizeof(buf), "A%04u", ++tag_number);
  tag = buf;
  std::string pending = tag + " ";
  std::string logged = pending;
  bool masked = false; // The command word is recorded, its arguments are not
  for(const auto &p : parts) {
    bool hide = mask && &p != &parts.front();
    masked = masked || hide;
    if(!p.first) {
      if(p.second.find('\r') != std::string::npos ||
	 p.second.find('\n') != std::string::npos ||
	 p.second.find('\0') != std::string::npos) {
	return fail("IMAP exception: Command contains a line break");
      }
      pending += p.second;
      if(!hide) logged += p.second;
      continue;
    }
    // A synchronizing literal: wait for the server's "+" first
    pending += "{" + std::to_string(p.second.size()) + "}";
    if(!hide) logged += "{" + std::to_string(p.second.size()) + "}";
    if(conn.SendLine(pending) < 0) return fail("IMAP exception: " + conn.Error());
    pending.clear();
    for(;;) {
      std::string line;
      if(!conn.ReadLine(line, 1024*1024)) {
	Close();
	return fail("IMAP exception: No reply from server" +
		    (conn.Error().empty() ? std::string() : ": " + conn.Error()));
      }
      record("server->", line);
      if(starts_with(line, "+")) break;
      if(starts_with(line, tag + " ")) { // Refused
	last_reply = line;
	return fail("IMAP exception: " + line.substr(tag.size() + 1));
      }
      // An untagged line before the continuation is allowed
    }
    if(conn.Send(p.second) < 0) return fail("IMAP exception: " + conn.Error());
    if(!hide && record_proto) {
      proto_string += "client-> (" + std::to_string(p.second.size()) + " bytes)\r\n";
    }
  }
  if(masked) logged += "xxxxxxxx"; // 2007: no password in the recorded session
  record("client->", logged);
  if(conn.SendLine(pending) < 0) return fail("IMAP exception: " + conn.Error());
  return 0;
}

int gxsIMAPClient::SendCommand(const std::string &command, std::string &tag)
{
  error.clear();
  return send_parts(imap_parts{{false, command}}, tag, false);
}

int gxsIMAPClient::RecvBuffer(const std::string &tag, std::string &buf)
{
  buf.clear();
  return read_reply(tag, &buf);
}

int gxsIMAPClient::Command(const std::string &cmd)
{
  std::string tag;
  if(SendCommand(cmd, tag) != 0) return -1;
  return read_reply(tag, 0);
}

// -----------------------------------------------------------
// Connections and the session
// -----------------------------------------------------------
int gxsIMAPClient::greeting()
{
  gxsIMAPUntagged u;
  std::size_t total = 0;
  if(read_line_with_literals(u, 0, total) != 0) return -1;
  if(istarts_with(u.text, "* OK") || istarts_with(u.text, "* PREAUTH")) {
    last_reply = u.text;
    std::size_t c = ifind(u.text, "[CAPABILITY ");
    if(c != std::string::npos) {
      std::size_t end = u.text.find(']', c);
      if(end != std::string::npos) {
	parse_capabilities(u.text.substr(c + 12, end - c - 12), capabilities);
      }
    }
    return 0;
  }
  Close();
  return fail("IMAP exception: Server refused the connection: " + u.text);
}

int gxsIMAPClient::Connect(const std::string &host, int port,
			   gxsMailSecurity security)
{
  Close();
  error.clear();
  capabilities.clear();
  tag_number = 0;
  if(conn.Connect(host, port, security == gxsMAIL_IMPLICIT_TLS) != 0) {
    return fail(conn.Error());
  }
  if(greeting() != 0) return -1;
  connected = true;
  if(security == gxsMAIL_STARTTLS) {
    if(Command("STARTTLS") != 0) {
      Close();
      return fail("IMAP exception: " + host + " does not accept STARTTLS");
    }
    if(conn.StartTLS() != 0) {
      Close();
      return fail(conn.Error());
    }
    capabilities.clear(); // RFC 3501 6.2.1: ask again after TLS
  }
  return 0;
}

int gxsIMAPClient::ConnectPlain(const std::string &host, int port)
{
  Close();
  error.clear();
  capabilities.clear();
  tag_number = 0;
  if(conn.Connect(host, port, false) != 0) return fail(conn.Error());
  if(greeting() != 0) return -1;
  connected = true;
  return 0;
}

int gxsIMAPClient::Capability(std::vector<std::string> &caps)
{
  if(Command("CAPABILITY") != 0) return -1;
  caps = capabilities;
  return 0;
}

bool gxsIMAPClient::HasCapability(const std::string &cap) const
{
  for(const std::string &c : capabilities) if(iequals(c, cap)) return true;
  return false;
}

int gxsIMAPClient::Noop()
{
  return Command("NOOP");
}

static void astring(const std::string &s, imap_parts &parts)
{
  std::string q;
  if(gxsIMAPClient::Quote(s, q)) parts.push_back({false, q});
  else parts.push_back({true, s});
}

int gxsIMAPClient::Login(const std::string &user, const std::string &passwd)
{
  error.clear();
  if(!IsConnected()) return fail("IMAP exception: Client is not connected to server");
  // 11/16/2026 (beta5, G4-2): no password in plain text unless the caller asked for it
  if(!IsSecure() && !allow_plain_login) {
    return fail("IMAP exception: Login refused on a plain text connection (the password would be sent "
		"unencrypted); use Connect() with TLS, or call AllowPlainLogin() first");
  }
  if(capabilities.empty()) {
    std::vector<std::string> caps;
    if(Capability(caps) != 0) return -1;
  }
  // 11/16/2026: user names and passwords go as quoted strings (\\ and " escaped) or, with 8-bit bytes, as literals;
  // a line break or a NUL is refused before anything is sent (a literal could carry it, but no server takes it)
  for(const std::string *a : {&user, &passwd}) {
    if(a->find_first_of(std::string("\r\n\0", 3)) != std::string::npos) {
      return fail(std::string("IMAP exception: The ") + (a == &user ? "user name" : "password") +
		  " contains a line break or a NUL character");
    }
  }
  if(HasCapability("LOGINDISABLED")) {
    return fail("IMAP exception: The server does not accept LOGIN on this "
		"connection (LOGINDISABLED)");
  }
  imap_parts parts{{false, "LOGIN "}};
  astring(user, parts);
  parts.push_back({false, " "});
  astring(passwd, parts);
  std::string tag;
  if(send_parts(parts, tag, true) != 0) return -1;
  if(read_reply(tag, 0) != 0) {
    error = "IMAP exception: Login refused: " + error.substr(error.find(':') + 2);
    return -1;
  }
  capabilities.clear(); // They can change after a login
  return 0;
}

int gxsIMAPClient::Login(const std::string &host, int port,
			 const std::string &user, const std::string &passwd)
{
  if(!IsConnected()) {
    if(Connect(host, port, gxsMAIL_IMPLICIT_TLS) != 0) return -1;
  }
  return Login(user, passwd);
}

int gxsIMAPClient::Logout(int close_sockets)
{
  if(!IsConnected()) {
    connected = false;
    return 0;
  }
  int rv = Command("LOGOUT");
  connected = false;
  if(close_sockets) conn.Close();
  return rv;
}

// -----------------------------------------------------------
// Mailboxes
// -----------------------------------------------------------
int gxsIMAPClient::mailbox_arg(const std::string &mailbox, imap_parts &parts,
			       const std::string &before)
{
  if(mailbox.empty()) return fail("IMAP exception: No mailbox name");
  parts.push_back({false, before});
  astring(mailbox, parts);
  return 0;
}

int gxsIMAPClient::select_cmd(const char *verb, const std::string &mailbox,
			      int &num_messages, std::string *raw)
{
  num_messages = 0;
  error.clear();
  if(!IsConnected()) return fail("IMAP exception: Client is not connected to server");
  imap_parts parts;
  if(mailbox_arg(mailbox, parts, std::string(verb) + " ") != 0) return -1;
  std::string tag;
  if(send_parts(parts, tag, false) != 0) return -1;
  if(read_reply(tag, raw) != 0) return -1;
  for(const gxsIMAPUntagged &u : untagged) {
    unsigned n;
    std::size_t pos;
    if(leading_number(u.text, n, pos) && iequals(trim(u.text.substr(pos)), "EXISTS")) {
      num_messages = n > 0x7FFFFFFF ? 0x7FFFFFFF : (int)n;
    }
  }
  return 0;
}

int gxsIMAPClient::SelectMailbox(const std::string &mailbox, int &num_messages)
{
  return select_cmd("SELECT", mailbox, num_messages, 0);
}

int gxsIMAPClient::SelectMailbox(const std::string &mailbox, std::string &rxbuf,
				 int &num_messages)
{
  rxbuf.clear();
  return select_cmd("SELECT", mailbox, num_messages, &rxbuf);
}

int gxsIMAPClient::Examine(const std::string &mailbox, int &num_messages)
{
  return select_cmd("EXAMINE", mailbox, num_messages, 0);
}

int gxsIMAPClient::ListMailboxes(std::vector<std::string> &names,
				 const std::string &pattern)
{
  names.clear();
  error.clear();
  imap_parts parts{{false, "LIST \"\" "}};
  astring(pattern, parts);
  std::string tag;
  if(send_parts(parts, tag, false) != 0) return -1;
  if(read_reply(tag, 0) != 0) return -1;
  for(const gxsIMAPUntagged &u : untagged) {
    if(!istarts_with(u.text, "LIST ")) continue;
    std::size_t pos = 5;
    std::string delim, name;
    if(!skip_value(u.text, pos)) continue;                     // (flags)
    if(!read_string(u.text, pos, u.literals, delim)) continue; // "/" or NIL
    if(!read_string(u.text, pos, u.literals, name)) continue;
    names.push_back(name);
  }
  return 0;
}

// -----------------------------------------------------------
// Messages
// -----------------------------------------------------------
int gxsIMAPClient::Search(const std::string &criteria,
			  std::vector<unsigned> &numbers)
{
  numbers.clear();
  if(Command("SEARCH " + (criteria.empty() ? std::string("ALL") : criteria)) != 0) {
    return -1;
  }
  for(const gxsIMAPUntagged &u : untagged) {
    if(!istarts_with(u.text, "SEARCH")) continue;
    for(const std::string &f : split(u.text.substr(6), ' ', true)) {
      if(f.empty() || f.size() > 10 ||
	 f.find_first_not_of("0123456789") != std::string::npos) continue;
      numbers.push_back((unsigned)std::strtoul(f.c_str(), 0, 10));
    }
  }
  return 0;
}

// The untagged FETCH reply for message_number that has item
static const gxsIMAPUntagged *find_fetch(const std::vector<gxsIMAPUntagged> &untagged,
					 int message_number, const std::string &item)
{
  std::string value;
  for(const gxsIMAPUntagged &u : untagged) {
    unsigned n;
    std::size_t pos;
    if(!leading_number(u.text, n, pos) || n != (unsigned)message_number) continue;
    if(gxsIMAPClient::FetchItem(u, item, value) == 0) return &u;
  }
  return 0;
}

int gxsIMAPClient::fetch_one(int message_number, const std::string &request,
			     const std::string &item, std::string &value)
{
  value.clear();
  error.clear();
  if(message_number <= 0) {
    return fail("IMAP exception: Client using invalid message number");
  }
  if(!IsConnected()) return fail("IMAP exception: Client is not connected to server");
  if(Command("FETCH " + std::to_string(message_number) + " (" + request + ")") != 0) {
    return -1;
  }
  const gxsIMAPUntagged *u = find_fetch(untagged, message_number, item);
  if(!u) return fail("IMAP exception: No " + item + " in the reply");
  return FetchItem(*u, item, value);
}

int gxsIMAPClient::FetchHeader(int message_number, std::string &header)
{
  return fetch_one(message_number, "BODY.PEEK[HEADER]", "BODY[HEADER]", header);
}

int gxsIMAPClient::FetchHeader(int message_number, std::string &header,
			       std::string &datetime, std::string &sent_datetime)
{
  datetime.clear();
  sent_datetime.clear();
  if(fetch_one(message_number, "INTERNALDATE ENVELOPE BODY.PEEK[HEADER]",
	       "BODY[HEADER]", header) != 0) return -1;
  const gxsIMAPUntagged *u = find_fetch(untagged, message_number, "BODY[HEADER]");
  FetchItem(*u, "INTERNALDATE", datetime);
  // The envelope's first field is the Date: header
  std::string env;
  if(FetchItem(*u, "ENVELOPE", env) == 0 && env.size() > 1) {
    std::size_t pos = 1;
    read_string(env, pos, u->literals, sent_datetime);
  }
  return 0;
}

int gxsIMAPClient::FetchBody(int message_number, std::string &body)
{
  return fetch_one(message_number, "BODY.PEEK[TEXT]", "BODY[TEXT]", body);
}

int gxsIMAPClient::FetchMessage(int message_number, std::string &message)
{
  return fetch_one(message_number, "BODY.PEEK[]", "BODY[]", message);
}

int gxsIMAPClient::FetchInternalDate(int message_number, std::string &datetime)
{
  return fetch_one(message_number, "INTERNALDATE", "INTERNALDATE", datetime);
}

int gxsIMAPClient::Delete(int message_number)
{
  if(message_number <= 0) {
    return fail("IMAP exception: Client using invalid message number");
  }
  return Command("STORE " + std::to_string(message_number) +
		 " +FLAGS.SILENT (\\Deleted)");
}

int gxsIMAPClient::Expunge()
{
  return Command("EXPUNGE");
}

} // namespace datareel

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
