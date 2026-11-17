// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: imap_login_test.cpp
// Compiler Used: MSVC, GCC
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

DataReel 7.4 beta5 (G4-2): gxsIMAPClient::Login() on a plain text
connection is refused, before anything is sent, unless the caller
called AllowPlainLogin(). Run against the run_tests mock server's
plain IMAP port:

  imap_login_test PORT
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include "gxsimap.h"

using namespace datareel;

static int failures = 0;
#define CHECK(c, what) do { if(!(c)) { std::cout << "FAIL " << what << "\n"; failures++; } } while(0)

int main(int argc, char **argv)
{
  if(argc < 2) {
    std::cerr << "Usage: imap_login_test PORT\n";
    return 2;
  }
  int port = std::atoi(argv[1]);

  { // No opt-in: refused, and no LOGIN goes to the server
    gxsIMAPClient c;
    c.SetRecordProto(true);
    CHECK(c.ConnectPlain("localhost", port) == 0, "plain connect: " << c.Error());
    CHECK(!c.IsSecure(), "the mock's plain port is not TLS");
    CHECK(c.Login("test", "secret") != 0, "a plain text login without AllowPlainLogin() was sent");
    CHECK(std::string(c.Error()).find("plain text") != std::string::npos, "no reason given: " << c.Error());
    CHECK(c.ProtoString().find("LOGIN") == std::string::npos, "LOGIN reached the server");
    CHECK(c.ProtoString().find("secret") == std::string::npos, "the password reached the session");
    c.Close();
  }
  { // AllowPlainLogin(false) is the same as no opt-in
    gxsIMAPClient c;
    c.AllowPlainLogin(false);
    CHECK(c.ConnectPlain("localhost", port) == 0, "plain connect 2: " << c.Error());
    CHECK(c.Login("test", "secret") != 0, "AllowPlainLogin(false) let a plain login through");
    c.Close();
  }
  { // Opted in: the login goes through
    gxsIMAPClient c;
    c.AllowPlainLogin();
    CHECK(c.ConnectPlain("localhost", port) == 0, "plain connect 3: " << c.Error());
    CHECK(c.Login("test", "secret") == 0, "an opted-in plain login failed: " << c.Error());
    CHECK(c.Logout() == 0, "logout: " << c.Error());
  }
  if(failures) {
    std::cout << failures << " imap_login_test checks failed\n";
    return 1;
  }
  std::cout << "PASS imap_login_test (refused without AllowPlainLogin(), sent with it)\n";
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
