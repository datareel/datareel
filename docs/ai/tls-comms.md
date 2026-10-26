# TLS communications with DataReel 7

DataReel 7 networking is TLS only (TLS 1.2 and 1.3), over IPv4 and
IPv6, on Linux (OpenSSL 1.1.1 or later) and Windows (Schannel or
OpenSSL 3.x). Telnet and ping are the only plain text protocols.

| Task | Class | Header |
|------|-------|--------|
| Any TLS client or server connection | `gxsTLSConnection` | `gxstls.h` |
| Accept TLS clients | `gxsTLSServer` | `gxstls.h` |
| HTTPS requests | `gxsHTTPClient`, `gxsHTTPMessage` | `gxshttpc.h` |
| HTTPS server messages | `gxsHTTPReadRequest`, `gxsHTTPWriteResponse` | `gxshttp.h` |
| Send mail | `gxsSMTPClient` | `gxsmtp.h` |
| Read mail | `gxsPOP3Client` | `gxspop3.h` |
| File transfer | `gxsFTPClient` (FTPS) | `gxsftp.h` |
| Database blocks client/server | `gxStream` | `gxstream.h` |
| Thread pool for servers | `ThreadPool` | `thrpool.h` |
| URL parsing, percent decoding | `gxsURL` | `gxsurl.h` |
| Base 64 | `gxsBase64Encode`, `gxsBase64Decode` | `gxs_b64.h` |
| Raw TCP/UDP socket | `gxSocket` | `gxsocket.h` |

## Conventions

- Functions return `0` on success and `-1` on failure; the reason is in
  `Error()`. `ReadLine()` and `ReadAll()` return `bool`.
- Set options (`SetTimeout`, `SetCAFile`, `SetVerifyPeer`,
  `SetClientCert`) before `Connect()`.
- The server certificate is checked against the system trust store
  (on Windows, the Windows certificate store) and the host name or IP
  address you connected to. Add a private CA with `SetCAFile()`.
  `SetVerifyPeer(false)` turns checking off: tests only.
- `Connect(host, port)` resolves the name and tries every address,
  IPv6 and IPv4. Numeric addresses such as `::1` and `127.0.0.1` work.
- `implicit_tls = true` (the default) starts TLS immediately. Pass
  `false` to connect in plain text, exchange the protocol's STARTTLS
  command yourself, then call `StartTLS()`.
- Timeouts are in seconds and apply to the connect and to each read.
- A `gxsTLSConnection` is not copyable. Servers create one per client,
  usually in a `std::shared_ptr` handed to a thread pool task.
- Socket library start-up on Windows (WSAStartup) is automatic.

## TLS client

<!-- sample: docs/ai/samples/tls_client.cpp -->
```cpp
// docs/ai/samples/tls_client.cpp
// DataReel 7 sample: TLS client that sends lines and prints the replies.
// Works with examples/tls/tls_echo_server.
// Usage: tls_client host port [ca.pem]
#include <cstdlib>
#include <iostream>
#include <string>
#include "gxstls.h"

using namespace datareel;

int main(int argc, char **argv)
{
  if(argc < 3) {
    std::cerr << "Usage: tls_client host port [ca.pem]\n";
    return 1;
  }
  gxsTLSConnection conn;
  conn.SetTimeout(30);                 // Seconds, for connect and each read
  if(argc > 3) conn.SetCAFile(argv[3]); // Trust a private CA as well
  // Certificates are verified against the system store and the host
  // name by default. Connect() returns 0 on success.
  if(conn.Connect(argv[1], std::atoi(argv[2])) != 0) {
    std::cerr << "Connect failed: " << conn.Error() << "\n";
    return 1;
  }
  for(const char *msg : { "hello", "DataReel 7" }) {
    std::string reply;
    if(conn.SendLine(msg) < 0 || !conn.ReadLine(reply)) {
      std::cerr << "I/O failed: " << conn.Error() << "\n";
      return 1;
    }
    std::cout << "Sent \"" << msg << "\", got \"" << reply << "\"\n";
  }
  conn.SendLine("quit");
  conn.Close();
  return 0;
}
```
<!-- end sample -->

Other data functions: `Send(buf, n)` writes all bytes,
`ReadSome(buf, n)` returns what is available (up to n),
`Read(str, n)` reads exactly n bytes, `ReadAll(str)` reads until the
peer closes. For a client certificate call
`SetClientCert("client.pem", "client.key")` before `Connect()`.

## TLS server

`gxsTLSServer::Listen()` loads the certificate chain and key and
listens on one dual-stack socket. `Accept()` blocks for the next
client and completes its TLS handshake.

<!-- sample: docs/ai/samples/tls_server.cpp -->
```cpp
// docs/ai/samples/tls_server.cpp
// DataReel 7 sample: multi-threaded TLS line server. Replies to each
// line with the line in upper case. "quit" ends a session and "stop"
// stops the server.
// Usage: tls_server port cert.pem key.pem
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include "gxstls.h"
#include "thrpool.h"

using namespace datareel;

static std::atomic<bool> stopping(false);

static void session(std::shared_ptr<gxsTLSConnection> conn, int port)
{
  std::string line;
  while(conn->ReadLine(line)) { // false on close, timeout or error
    if(line == "quit") break;
    if(line == "stop") {
      stopping = true;
      gxSocket wake; // Wake the blocked Accept() so main() can exit
      wake.Connect("localhost", port, 5);
      break;
    }
    for(char &c : line) c = (char)std::toupper((unsigned char)c);
    if(conn->SendLine(line) < 0) break;
  }
  conn->Close();
}

int main(int argc, char **argv)
{
  if(argc < 4) {
    std::cerr << "Usage: tls_server port cert.pem key.pem\n";
    return 1;
  }
  gxsTLSServer server;
  // Listens on IPv4 and IPv6. Returns 0 on success.
  if(server.Listen(std::atoi(argv[1]), argv[2], argv[3]) != 0) {
    std::cerr << "Listen failed: " << server.Error() << "\n";
    return 1;
  }
  std::cout << "Listening on port " << server.Port() << std::endl;
  ThreadPool pool(8);
  while(!stopping) {
    auto conn = std::make_shared<gxsTLSConnection>();
    conn->SetTimeout(120); // Idle sessions are dropped after two minutes
    // Accept() completes the TLS handshake; a failed handshake only
    // affects that client.
    if(server.Accept(*conn) != 0) continue;
    if(stopping) break;
    pool.Submit(session, conn, server.Port());
  }
  pool.Wait();
  return 0;
}
```
<!-- end sample -->

Make a test certificate:

```sh
openssl req -x509 -newkey rsa:2048 -nodes -keyout key.pem -out cert.pem \
  -days 30 -subj "/CN=localhost" \
  -addext "subjectAltName=DNS:localhost,IP:127.0.0.1,IP:::1"
# Clients then use SetCAFile("cert.pem")
```

## HTTPS client

`gxsHTTPClient` opens a new TLS connection per request, follows
redirects, decodes chunked bodies and refuses `http://` URLs. It does
not do HTTP/2, cookies or compression.

<!-- sample: docs/ai/samples/https_fetch.cpp -->
```cpp
// docs/ai/samples/https_fetch.cpp
// DataReel 7 sample: HTTPS GET and POST with gxsHTTPClient.
// Usage: https_fetch https://host/path [ca.pem]
#include <iostream>
#include <string>
#include "gxshttpc.h"

using namespace datareel;

int main(int argc, char **argv)
{
  if(argc < 2) {
    std::cerr << "Usage: https_fetch https://host/path [ca.pem]\n";
    return 1;
  }
  gxsHTTPClient client; // https:// only; http:// URLs are refused
  client.SetTimeout(30);
  if(argc > 2) client.SetCAFile(argv[2]);
  client.AddHeader("Accept", "application/json, text/plain, */*");

  gxsHTTPMessage resp;
  // Returns 0 when any response arrived, so check resp.status too.
  // Redirects are followed (5 by default).
  if(client.Get(argv[1], resp) != 0) {
    std::cerr << "GET failed: " << client.Error() << "\n";
    return 1;
  }
  std::cout << "GET " << client.FinalURL() << " -> " << resp.status << " "
	    << resp.reason << ", " << resp.body.size() << " bytes, "
	    << "Content-Type: " << resp.Header("content-type") << "\n";

  // POST a JSON body. Request() sends any other method.
  gxsHTTPMessage post_resp;
  if(client.Post(argv[1], "{\"name\":\"DataReel\"}", "application/json",
		 post_resp) != 0) {
    std::cerr << "POST failed: " << client.Error() << "\n";
    return 1;
  }
  std::cout << "POST -> " << post_resp.status << " " << post_resp.reason
	    << "\n";
  return (resp.status >= 200 && resp.status < 300) ? 0 : 2;
}
```
<!-- end sample -->

- `Download(url, file, resp)` streams the body to a file.
- `SetBasicAuth(user, password)` sends basic auth (only to the first
  host in a redirect chain). `AddHeader()` adds headers such as
  `Authorization: Bearer ...` to every request.
- `gxsHTTPMessage` fields: `status`, `reason`, `version`, `headers`
  (vector of name/value pairs), `body`; `Header(name)` looks up a
  header without regard to case.

## HTTPS server

There is no server framework; read a request and write a response on
each accepted `gxsTLSConnection`:

```cpp
#include "gxshttp.h"
#include "gxstls.h"
using namespace datareel;

void handle(gxsTLSConnection &conn)
{
  gxsHTTPMessage req, resp;
  if(gxsHTTPReadRequest(conn, req, 1024*1024) != 0) return; // 1 MB body limit
  resp.status = 200;
  resp.SetHeader("Content-Type", "application/json");
  resp.SetHeader("Connection", "close");
  resp.body = "{\"path\":\"" + req.target + "\"}"; // Escape in real code
  gxsHTTPWriteResponse(conn, resp); // Sets Content-Length
}
```

`examples/tls/https_server.cpp` is a complete static file server with
a thread pool and document root checks.

## Mail and file transfer

All three clients take a `gxsMailSecurity` value:
`gxsMAIL_IMPLICIT_TLS` (TLS from the start: SMTP 465, POP3 995, FTPS
990) or `gxsMAIL_STARTTLS` (STARTTLS, STLS or AUTH TLS after the
greeting: SMTP 587, POP3 110, FTP 21). A server that does not offer TLS
is refused.

<!-- sample: docs/ai/samples/send_mail.cpp -->
```cpp
// docs/ai/samples/send_mail.cpp
// DataReel 7 sample: send a message through a mail server's
// submission port (587) with STARTTLS and a login.
// Usage: send_mail host port user password from to [ca.pem]
#include <cstdlib>
#include <iostream>
#include <string>
#include "gxsmtp.h"

using namespace datareel;

int main(int argc, char **argv)
{
  if(argc < 7) {
    std::cerr << "Usage: send_mail host port user password from to [ca.pem]\n";
    return 1;
  }
  gxsSMTPClient smtp;
  smtp.SetTimeout(30);
  if(argc > 7) smtp.SetCAFile(argv[7]);
  // Port 587 (submission) uses gxsMAIL_STARTTLS; port 465 uses
  // gxsMAIL_IMPLICIT_TLS, the default. A server that does not offer STARTTLS is
  // refused; nothing is ever sent in plain text.
  if(smtp.Connect(argv[1], std::atoi(argv[2]), gxsMAIL_STARTTLS) != 0 ||
     smtp.Login(argv[3], argv[4]) != 0) {
    std::cerr << "SMTP: " << smtp.Error() << "\n";
    return 1;
  }
  if(smtp.SendMessage(argv[5], { argv[6] }, "Test from DataReel 7",
		      "This message was sent over TLS.\n") != 0) {
    std::cerr << "Send failed: " << smtp.Error() << "\n";
    return 1;
  }
  smtp.Quit();
  std::cout << "Sent\n";
  return 0;
}
```
<!-- end sample -->

For several recipients, attachments or custom headers use the
low-level calls: `MailFrom()`, `RcptTo()` for each recipient, then
`Data(message)` with complete RFC 5322 headers and body (`smtp_time()`
in `systime.h` makes the Date header, `gxsBase64Encode` encodes
attachments).

POP3:

```cpp
gxsPOP3Client pop3;
if(pop3.Connect("mail.example.com") != 0 ||      // 995, implicit TLS
   pop3.Login("user", "password") != 0) { /* pop3.Error() */ }
int count; long long bytes;
pop3.Stat(count, bytes);
std::string msg;
if(count > 0) pop3.Retr(1, msg);                 // Whole message, dot-unstuffed
pop3.Quit();
```

FTPS (data connections are encrypted with PROT P and reuse the control
connection's TLS session, which most servers require):

```cpp
gxsFTPClient ftp;
long long bytes = 0;
if(ftp.Connect("ftp.example.com") != 0 ||         // 21, AUTH TLS
   ftp.Login("user", "password") != 0) { /* ftp.Error() */ }
ftp.Put("report.pdf", "upload/report.pdf", bytes);
ftp.Get("data/latest.csv", "latest.csv", bytes);
std::string listing;
ftp.List("data", listing);
ftp.Quit();
```

## Database blocks over TLS (gxStream)

`gxStream` sends `gxBlockHeader` framed blocks over a
`gxsTLSConnection`, used to add, request, change and delete database
records on a remote server. Each block's status byte says what it is
(`gxAddRemoteBlock`, `gxRequestBlock`, `gxCloseConnection`, ...). A
server accepts with `server.Accept(stream.Connection())`, then loops on
`ReadHeader()` / `ReadBlock()` and answers with `WriteBlock()` or
`WriteResultBlock()`. Block sizes from the network are limited by
`SetMaxBlockSize()` (64 MB by default). See
`examples/tls/stream_server.cpp` and `stream_client.cpp`.

## Raw sockets

`gxSocket` is the IPv4/IPv6 socket under everything above:
`Connect(host, port, timeout)`, `Listen(port)`, `Accept()` (returns a
new `gxSocket`), `Send`, `Recv` (exact count), `RecvSome`, and UDP with
`OpenDatagram()`, `SendTo()`, `RecvFrom()`. Use it only for protocols
that cannot use TLS; attach a connected socket to TLS with
`gxsTLSConnection::AttachClient()` or `AttachServer()`.

## Mistakes to avoid

- Expecting `http://`, port 25 plain SMTP or plain FTP to work: they are
  refused by design.
- Disabling certificate checks to "fix" a test: give the client the CA
  with `SetCAFile()` instead, and put the host name or IP address in
  the certificate's subjectAltName.
- Using one `gxsTLSConnection` from two threads at the same time.
- Treating a `0` return from `gxsHTTPClient::Get()` as HTTP success:
  it means a response arrived; check `resp.status`.
