// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: testprog.cpp
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
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

Example program for the IMAP client class, gxsIMAPClient. Runs one
command against an IMAP server:

  testprog [options] server username command [arguments]

Options:
  --port N      Server port (993 with TLS, 143 with --plain or
                --starttls)
  --starttls    Connect in plain text and switch to TLS with STARTTLS
  --plain       No TLS. The password is sent in plain text, so this
                is refused unless --allow-plain is also given.
  --allow-plain Allow a login without TLS (test servers only)
  --ca FILE     Check the server certificate against the CAs in FILE
                (with no --ca the system certificate store is used)
  --name NAME   Check the certificate against NAME instead of server
  --no-verify   Do not check the server certificate (test servers)
  --log         Print the IMAP session (passwords are not printed)

Commands:
  list                    Mailbox names
  count   MAILBOX         Number of messages
  search  MAILBOX WHAT    Message numbers, for example "UNSEEN" or
                          "SINCE 1-Nov-2026"
  header  MAILBOX N       Header of message N
  body    MAILBOX N       Body of message N
  message MAILBOX N       Whole message N
  dates   MAILBOX N       Date received and Date: header of message N
  seen    MAILBOX N       Mark message N as read
  delete  MAILBOX N       Delete message N (mark it and expunge)

The password is read from the IMAP_PASSWORD environment variable or,
if that is not set, from the first line of standard input, so it does
not show in the process list.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include "gxsimap.h"
#if defined (__WIN32__)
#include <io.h>
#include <fcntl.h>
#endif

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

static void Usage(const char *prog)
{
  printf("Usage: %s [--port N] [--starttls | --plain [--allow-plain]]\n"
	 "       [--ca FILE] [--name NAME] [--no-verify] [--log] server username command "
	 "[arguments]\n", prog);
  printf("Commands: list, count MAILBOX, search MAILBOX WHAT, "
	 "header|body|message|dates|seen|delete MAILBOX N\n");
  printf("The password is read from IMAP_PASSWORD or standard input.\n");
}

static int Fail(gxsIMAPClient &imap, int show_log)
{
  printf("%s\n", imap.Error().c_str());
  if(show_log) printf("%s", imap.ProtoString().c_str());
  imap.CloseClient();
  return 1;
}

int main(int argc, char **argv)
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

#if defined (__WIN32__)
  // Messages are printed as received, with their CR LF line ends
  _setmode(_fileno(stdout), _O_BINARY);
#endif
  int port = 0, starttls = 0, plain = 0, allow_plain = 0, verify = 1;
  int show_log = 0;
  const char *ca = 0, *name = 0;
  int i = 1;
  for(; i < argc && strncmp(argv[i], "--", 2) == 0; i++) {
    if(strcmp(argv[i], "--port") == 0 && i + 1 < argc) port = atoi(argv[++i]);
    else if(strcmp(argv[i], "--starttls") == 0) starttls = 1;
    else if(strcmp(argv[i], "--plain") == 0) plain = 1;
    else if(strcmp(argv[i], "--allow-plain") == 0) allow_plain = 1;
    else if(strcmp(argv[i], "--ca") == 0 && i + 1 < argc) ca = argv[++i];
    else if(strcmp(argv[i], "--name") == 0 && i + 1 < argc) name = argv[++i];
    else if(strcmp(argv[i], "--no-verify") == 0) verify = 0;
    else if(strcmp(argv[i], "--log") == 0) show_log = 1;
    else { Usage(argv[0]); return 1; }
  }
  if(argc - i < 3) { Usage(argv[0]); return 1; }
  const char *server = argv[i];
  const char *username = argv[i+1];
  const char *command = argv[i+2];
  const char *mailbox = (argc - i > 3) ? argv[i+3] : "INBOX";
  const char *arg = (argc - i > 4) ? argv[i+4] : "";
  int use_tls = !(starttls || plain);
  if(port == 0) port = use_tls ? gxSOCKET_SIMAP_PORT : gxSOCKET_IMAP_PORT;

  // The password, from the environment or the first input line
  char password[1024];
  password[0] = 0;
  const char *env = getenv("IMAP_PASSWORD");
  if(env) {
    strncpy(password, env, sizeof(password) - 1);
    password[sizeof(password) - 1] = 0;
  }
  else if(fgets(password, sizeof(password), stdin)) {
    password[strcspn(password, "\r\n")] = 0;
  }

  // 11/16/2026: The calls below are the DataReel 7.4 gxsIMAPClient
  // interface, which 6.4 also has, so this code builds with either.
  gxsIMAPClient imap;
  imap.SetTimeout(60);
  imap.SetRecordProto(show_log != 0);
  if(ca) imap.SetCAFile(ca);
  imap.SetVerifyPeer(verify != 0);
#if defined (__USE_GX_SSL_EXTENSIONS__)
  if(!plain && name) imap.openssl.SetHostName(name); // 6.4 only
#endif

  int crv;
  if(plain) {
    if(!allow_plain) {
      printf("Refusing to send a password without TLS (add --allow-plain)\n");
      return 1;
    }
    crv = imap.ConnectPlain(server, port);
  }
  else {
    crv = imap.Connect(server, port, starttls ? gxsMAIL_STARTTLS :
		       gxsMAIL_IMPLICIT_TLS);
  }
  if(crv != 0) return Fail(imap, show_log);
  if(imap.Login(std::string(username), std::string(password)) != 0) {
    memset(password, 0, sizeof(password));
    return Fail(imap, show_log);
  }
  memset(password, 0, sizeof(password));

  int rv = 0;
  std::string out, out2;
  if(strcmp(command, "list") == 0) {
    std::vector<std::string> names;
    rv = imap.ListMailboxes(names);
    for(size_t k = 0; rv == 0 && k < names.size(); k++) printf("%s\n", names[k].c_str());
  }
  else {
    int count = 0;
    int writes = (strcmp(command, "seen") == 0 || strcmp(command, "delete") == 0);
    rv = writes ? imap.SelectMailbox(std::string(mailbox), count) :
      imap.Examine(mailbox, count);
    int n = atoi(arg);
    if(rv != 0) {
      ;
    }
    else if(strcmp(command, "count") == 0) {
      printf("%d\n", count);
    }
    else if(strcmp(command, "search") == 0) {
      std::vector<unsigned> nums;
      rv = imap.Search(*arg ? arg : "ALL", nums);
      for(size_t k = 0; rv == 0 && k < nums.size(); k++) {
	printf("%s%u", k ? " " : "", nums[k]);
      }
      if(rv == 0) printf("\n");
    }
    else if(strcmp(command, "header") == 0) {
      rv = imap.FetchHeader(n, out);
      if(rv == 0) fwrite(out.data(), 1, out.size(), stdout);
    }
    else if(strcmp(command, "body") == 0) {
      rv = imap.FetchBody(n, out);
      if(rv == 0) fwrite(out.data(), 1, out.size(), stdout);
    }
    else if(strcmp(command, "message") == 0) {
      rv = imap.FetchMessage(n, out);
      if(rv == 0) fwrite(out.data(), 1, out.size(), stdout);
    }
    else if(strcmp(command, "dates") == 0) {
      std::string hdr;
      rv = imap.FetchHeader(n, hdr, out, out2); // The 2007 form
      if(rv == 0) printf("received: %s\nsent: %s\n", out.c_str(), out2.c_str());
    }
    else if(strcmp(command, "seen") == 0) {
      char cmd[64];
      sprintf(cmd, "STORE %d +FLAGS.SILENT (\\Seen)", n);
      rv = imap.Command(cmd);
    }
    else if(strcmp(command, "delete") == 0) {
      rv = imap.Delete(n);
      if(rv == 0) rv = imap.Expunge();
    }
    else {
      imap.Logout();
      Usage(argv[0]);
      return 1;
    }
  }
  if(rv != 0) return Fail(imap, show_log);
  imap.Logout();
  if(show_log) printf("%s", imap.ProtoString().c_str());
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
