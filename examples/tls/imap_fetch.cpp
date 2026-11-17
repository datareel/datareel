// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: imap_fetch.cpp
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

Read a mailbox with gxsIMAPClient (gxsimap.h). Based on the 2007
IMAP client program that downloads report messages to a spool
folder: the messages that match a search are listed newest first,
and with --spool each message's text is written to the folder once
(a message already there is skipped).

Implicit TLS (port 993) is used unless --starttls (port 143, then
STARTTLS) or --plain (port 143, plain text: a local server or a
test only) is given. --plain is also what lets Login() send the
password unencrypted (gxsIMAPClient::AllowPlainLogin()).

Usage: imap_fetch host port user password [--mailbox INBOX]
       [--search CRITERIA] [-r number] [--spool folder] [--debug]
       [--starttls | --plain] [--ca ca.pem] [--insecure]

  --search   IMAP search criteria, such as UNSEEN or
             'SUBJECT "report"' (default ALL)
  -r number  print that whole message
  --debug    print the session at the end, the password masked
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include "gxsimap.h"
#include "strutil.h"
#include "common_opts.h"

using namespace datareel;

// 11/17/2026: the first n characters of line equal name, ignoring case (strncasecmp is POSIX only: the Windows build
// failed)
static bool same_name(const std::string &line, const char *name, std::size_t n)
{
  for(std::size_t i = 0; i < n; i++) {
    if(std::tolower((unsigned char)line[i]) != std::tolower((unsigned char)name[i])) return false;
  }
  return true;
}

static std::string header_field(const std::string &header, const char *name)
{
  // The first "Name: value" line of a header (unfolded lines only)
  std::size_t n = std::strlen(name);
  std::size_t pos = 0;
  while(pos < header.size()) {
    std::size_t eol = header.find('\n', pos);
    if(eol == std::string::npos) eol = header.size();
    std::string line = header.substr(pos, eol - pos);
    if(!line.empty() && line.back() == '\r') line.pop_back();
    if(line.size() > n && line[n] == ':' && same_name(line, name, n)) {
      std::size_t v = n + 1;
      while(v < line.size() && line[v] == ' ') v++;
      return line.substr(v);
    }
    pos = eol + 1;
  }
  return std::string();
}

static bool file_has_data(const std::string &fname)
{
  struct stat st;
  return stat(fname.c_str(), &st) == 0 && st.st_size > 0;
}

// 11/16/2026 (beta5, G4-1): the spool file name, "<arrival date>_<number>.txt". The date is the server's text, so
// only letters, digits, "-", "_" and "+" are kept (anything else, "/", "\", ".", ":", spaces and control
// characters, becomes "_") and it is cut to 60 characters: the name can never leave the spool folder or be a
// device or hidden name on any platform.
static std::string spool_name(const std::string &datetime, int n)
{
  std::string s;
  for(std::size_t i = 0; i < datetime.size() && s.size() < 60; i++) {
    unsigned char c = (unsigned char)datetime[i];
    bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
      c == '-' || c == '_' || c == '+';
    s += ok ? (char)c : '_';
  }
  return s + "_" + std::to_string(n) + ".txt";
}

static int return_error(gxsIMAPClient &client, bool debug)
{
  std::cerr << client.Error() << "\n";
  if(debug) std::cerr << "\n--- Session ---\n" << client.ProtoString();
  client.Close();
  return 1;
}

int main(int argc, char **argv)
{
  TLSOptions opt;
  opt.Parse(argc, argv);
  std::vector<std::string> a;
  std::string mailbox = "INBOX", criteria = "ALL", spool_dir;
  int retr = 0;
  bool plain = false, debug = false;
  for(std::size_t i = 0; i < opt.args.size(); i++) {
    const std::string &s = opt.args[i];
    bool more = i + 1 < opt.args.size();
    if(s == "--mailbox" && more) mailbox = opt.args[++i];
    else if(s == "--search" && more) criteria = opt.args[++i];
    else if(s == "--spool" && more) spool_dir = opt.args[++i];
    else if(s == "-r" && more) retr = std::atoi(opt.args[++i].c_str());
    else if(s == "--plain") plain = true;
    else if(s == "--debug") debug = true;
    else a.push_back(s);
  }
  if(a.size() < 4 || (plain && opt.starttls)) {
    std::cerr << "Usage: " << argv[0] << " host port user password "
	      << "[--mailbox INBOX] [--search CRITERIA] [-r number] "
	      << "[--spool folder] [--debug] [--starttls | --plain] "
	      << "[--ca ca.pem] [--insecure]\n";
    return 1;
  }

  gxsIMAPClient client;
  opt.Apply(client);
  client.SetRecordProto(debug);
  if(plain) client.AllowPlainLogin(); // --plain: the password goes unencrypted, by request
  int port = std::atoi(a[1].c_str());
  int rv = plain ? client.ConnectPlain(a[0], port) :
    client.Connect(a[0], port, opt.starttls ? gxsMAIL_STARTTLS : gxsMAIL_IMPLICIT_TLS);
  if(rv != 0 || client.Login(a[2], a[3]) != 0) return return_error(client, debug);
  std::cout << "Connected to " << a[0] << " port " << port
	    << (client.IsSecure() ? " (TLS)" : " (plain text)") << "\n";

  int num_messages = 0;
  if(client.SelectMailbox(mailbox, num_messages) != 0) return return_error(client, debug);
  std::cout << mailbox << " has " << num_messages << " messages\n";

  std::vector<unsigned> found;
  if(client.Search(criteria, found) != 0) return return_error(client, debug);
  std::cout << found.size() << " match " << criteria << "\n";

  // Newest first, as the 2007 program read them
  for(auto it = found.rbegin(); it != found.rend(); ++it) {
    int n = (int)*it;
    std::string header, datetime, sent_datetime, body;
    if(client.FetchHeader(n, header, datetime, sent_datetime) != 0) {
      return return_error(client, debug);
    }
    std::cout << n << ": " << header_field(header, "Subject") << "\n"
	      << "   received " << datetime << ", sent " << sent_datetime << "\n";
    if(spool_dir.empty()) continue;

    // The spool file is named by the arrival time and number
    std::string fname = spool_dir + "/" + spool_name(datetime, n);
    if(file_has_data(fname)) {
      std::cout << "   already in the spool, skipped\n";
      continue;
    }
    if(client.FetchBody(n, body) != 0) return return_error(client, debug);
    std::ofstream out(fname, std::ios::binary);
    if(!out.write(body.data(), (std::streamsize)body.size()) || !out.flush()) {
      std::cerr << "Error writing " << fname << "\n";
      out.close();
      std::remove(fname.c_str());
      continue;
    }
    std::cout << "   saved " << body.size() << " bytes to " << fname << "\n";
  }

  if(retr > 0) {
    std::string message;
    if(client.FetchMessage(retr, message) != 0) return return_error(client, debug);
    std::cout << "\n--- Message " << retr << " ---\n" << message;
  }

  if(client.Logout() != 0) return return_error(client, debug);
  if(debug) std::cout << "\n--- Session ---\n" << client.ProtoString();
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
