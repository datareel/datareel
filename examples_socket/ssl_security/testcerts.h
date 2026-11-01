// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: testcerts.h
// Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/31/2026
// Date Last Modified: 10/31/2026
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

Test helper (UNIX): makes throwaway certificates with the openssl
command line program in a new private directory. The certificates
are valid for two days and are deleted by RemoveTestCerts().

  ca.pem, ca.key          Test CA
  other_ca.pem            Second CA (signs nothing used here)
  server.pem, server.key  Server, CN=wrong.example, alternative names
                          DNS:localhost and IP:127.0.0.1, signed by ca
  client1.pem/.key        Client, CN=client1, signed by ca
  client2.pem/.key        Client, CN=client2, signed by ca
  rogue.pem/.key          Client, CN=client1, self-signed
  client1.fp              SHA-256 fingerprint of client1.pem
*/
// ----------------------------------------------------------- //
#ifndef __GX_TEST_CERTS_HPP__
#define __GX_TEST_CERTS_HPP__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char gxTestCertDir[512];

static inline int gxTestRun(const char *cmd)
{
  char sbuf[4096];
  snprintf(sbuf, sizeof(sbuf), "cd '%s' && (%s) >/dev/null 2>&1",
	   gxTestCertDir, cmd);
  return system(sbuf) == 0 ? 0 : -1;
}

static inline const char *gxTestCertPath(const char *fname)
// Path of a file in the certificate directory. Uses a ring of buffers
// so a few paths can be used in one call.
{
  static char sbuf[8][600];
  static int next = 0;
  char *p = sbuf[next++ & 7];
  snprintf(p, 600, "%s/%s", gxTestCertDir, fname);
  return p;
}

static inline int MakeTestCerts()
// Returns 0 if the certificates were made
{
  char tmpl[] = "/tmp/gxtestcerts.XXXXXX";
  const char *tmpdir = getenv("TMPDIR");
  if(tmpdir && *tmpdir && strlen(tmpdir) < 400) {
    snprintf(gxTestCertDir, sizeof(gxTestCertDir), "%s/gxtestcerts.XXXXXX",
	     tmpdir);
  }
  else {
    strcpy(gxTestCertDir, tmpl);
  }
  if(!mkdtemp(gxTestCertDir)) return -1;
  const char *cmds[] = {
    "openssl req -x509 -newkey rsa:2048 -nodes -keyout ca.key -out ca.pem"
    " -days 2 -subj '/CN=DataReel Test CA'",
    "openssl req -x509 -newkey rsa:2048 -nodes -keyout other_ca.key"
    " -out other_ca.pem -days 2 -subj '/CN=DataReel Other CA'",
    "printf 'subjectAltName=DNS:localhost,IP:127.0.0.1\\n"
    "basicConstraints=CA:FALSE\\nextendedKeyUsage=serverAuth\\n' > server.ext",
    "printf 'basicConstraints=CA:FALSE\\nextendedKeyUsage=clientAuth\\n'"
    " > client.ext",
    "openssl req -newkey rsa:2048 -nodes -keyout server.key -out server.csr"
    " -subj '/CN=wrong.example'",
    "openssl x509 -req -in server.csr -CA ca.pem -CAkey ca.key"
    " -CAcreateserial -out server.pem -days 2 -extfile server.ext",
    "openssl req -newkey rsa:2048 -nodes -keyout client1.key -out client1.csr"
    " -subj '/CN=client1'",
    "openssl x509 -req -in client1.csr -CA ca.pem -CAkey ca.key"
    " -CAcreateserial -out client1.pem -days 2 -extfile client.ext",
    "openssl req -newkey rsa:2048 -nodes -keyout client2.key -out client2.csr"
    " -subj '/CN=client2'",
    "openssl x509 -req -in client2.csr -CA ca.pem -CAkey ca.key"
    " -CAcreateserial -out client2.pem -days 2 -extfile client.ext",
    "openssl req -x509 -newkey rsa:2048 -nodes -keyout rogue.key"
    " -out rogue.pem -days 2 -subj '/CN=client1'",
    "openssl x509 -in client1.pem -noout -fingerprint -sha256 > client1.fp",
    0
  };
  for(int i = 0; cmds[i]; i++) {
    if(gxTestRun(cmds[i]) != 0) {
      fprintf(stderr, "Test certificate command failed: %s\n", cmds[i]);
      return -1;
    }
  }
  return 0;
}

static inline int ReadTestFile(const char *fname, char *buf, int size)
// Read the first line of a file in the certificate directory
{
  FILE *fp = fopen(gxTestCertPath(fname), "rb");
  if(!fp) return -1;
  if(!fgets(buf, size, fp)) { fclose(fp); return -1; }
  fclose(fp);
  buf[strcspn(buf, "\r\n")] = 0;
  return 0;
}

static inline void RemoveTestCerts()
{
  if(gxTestCertDir[0]) {
    char sbuf[700];
    snprintf(sbuf, sizeof(sbuf), "rm -rf '%s'", gxTestCertDir);
    if(system(sbuf) != 0) { }
    gxTestCertDir[0] = 0;
  }
}

#endif // __GX_TEST_CERTS_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
