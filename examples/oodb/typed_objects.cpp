// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: typed_objects.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/12/2026
// Date Last Modified: 11/12/2026
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

Typed objects over gxStream, with a plain text export and import.

The program shows the whole life of a typed object in DataReel:

 1. A C++ class (TradeEvent) packs itself into a block of bytes with
    a small type header (a type code, a version and the body size),
    so any program that reads the block knows what it holds.
 2. A client sends each packed object over TLS with gxStream. A
    server thread in the same program stores every block it receives
    in a gxDatabase file and answers with the block's file address.
 3. The client reads each object back by address and checks it.
 4. The program walks the database file, unpacks every object and
    exports them to a text file: one line per object in key=value
    form, under a comment line that says the same thing in plain
    words. The text file can be read, searched, kept in version
    control or mailed, and it does not depend on the binary layout.
 5. The text file is imported into a second database file and every
    object is compared with the original, field by field.

gxStream is only the transport here: the objects are the program's
own class, and the database file stores whatever bytes the class
packs. The text export is the long-lived, portable form.

The client and the server talk over localhost on a port the system
picks. gxStream always uses TLS, so the program needs a certificate
and key for localhost (see examples/README section "Typed objects"
for the openssl commands).

Usage: typed_objects cert.pem key.pem [--ca ca.pem | --insecure]

  cert.pem, key.pem  Server certificate and private key for localhost
  --ca ca.pem        CA file the client checks the certificate with
                     (for a self-signed certificate, the cert itself)
  --insecure         Do not check the certificate (testing only)

Files written in the current directory: typed_objects.gxd (what the
server stored), typed_objects.txt (the export) and
typed_objects_import.gxd (the import). The program exits with 0 when
every object survives all three steps.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include "gxdbase.h"
#include "gxstream.h"
#include "strutil.h"

using namespace datareel;

// ----------------------------------------------------------- //
// The typed object
// ----------------------------------------------------------- //

// Every packed object starts with this header. The type code tells a
// reader which class the block holds and the version lets the class
// change later and still read old blocks.
struct ObjectHeader {
  std::uint32_t type;    // A four character code, 'TEVT' here
  std::uint16_t version; // Layout version of the body
  std::uint16_t size;    // Body bytes after the header
};

const std::uint32_t TRADE_EVENT_TYPE = 0x54564554; // "TEVT" in memory
const std::uint16_t TRADE_EVENT_VERSION = 1;

// One trading event: a fill, a cancel or a note about a ticker.
class TradeEvent
{
public:
  TradeEvent() { }
  TradeEvent(std::int64_t id_, int day_, int seconds_, const char *ticker_,
	     char side_, int qty_, double price_, const char *note_)
    : id(id_), day(day_), seconds(seconds_), side(side_), qty(qty_),
      price(price_) {
    copy_to(ticker, sizeof(ticker), ticker_);
    copy_to(note, sizeof(note), note_);
  }

public: // Binary form, used in the database file and on the network
  // The body is written field by field with fixed sizes, so the
  // layout does not depend on the compiler's structure padding.
  // Numbers are in this machine's byte order: files and streams move
  // between machines of the same byte order (x86-64 and ARM64 are
  // both little endian).
  std::vector<unsigned char> Pack() const {
    std::vector<unsigned char> buf;
    ObjectHeader h{TRADE_EVENT_TYPE, TRADE_EVENT_VERSION, (std::uint16_t)BODY_SIZE};
    put(buf, &h.type, sizeof(h.type));
    put(buf, &h.version, sizeof(h.version));
    put(buf, &h.size, sizeof(h.size));
    put(buf, &id, sizeof(id));
    put(buf, &day, sizeof(day));
    put(buf, &seconds, sizeof(seconds));
    put(buf, ticker, sizeof(ticker));
    put(buf, &side, sizeof(side));
    put(buf, &qty, sizeof(qty));
    put(buf, &price, sizeof(price));
    put(buf, note, sizeof(note));
    return buf;
  }

  // Returns false if the block is not a TradeEvent this code can read
  bool Unpack(const unsigned char *p, std::size_t len) {
    const std::size_t HDR = 8;
    if(len < HDR) return false;
    ObjectHeader h;
    std::size_t at = 0;
    get(p, at, &h.type, sizeof(h.type));
    get(p, at, &h.version, sizeof(h.version));
    get(p, at, &h.size, sizeof(h.size));
    if(h.type != TRADE_EVENT_TYPE || h.version != TRADE_EVENT_VERSION ||
       h.size != BODY_SIZE || len != HDR + BODY_SIZE) return false;
    get(p, at, &id, sizeof(id));
    get(p, at, &day, sizeof(day));
    get(p, at, &seconds, sizeof(seconds));
    get(p, at, ticker, sizeof(ticker));
    get(p, at, &side, sizeof(side));
    get(p, at, &qty, sizeof(qty));
    get(p, at, &price, sizeof(price));
    get(p, at, note, sizeof(note));
    ticker[sizeof(ticker) - 1] = 0; // Never trust a block's strings
    note[sizeof(note) - 1] = 0;
    return true;
  }

public: // Text form, used by the export and the import
  // What the object says, in plain words: the comment line above
  // each record in the export
  std::string Narrative() const {
    const char *what = side == 'B' ? "bought" : side == 'S' ? "sold" : "noted";
    if(side == 'N') return format("%s %s at %s: %s", ticker, what, Time().c_str(), note);
    return format("%s %s %d shares at %.4f at %s ET on %s (%s)", ticker, what, qty,
		  price, Time().c_str(), Day().c_str(), note);
  }

  // One line of key=value fields. note= is always last and runs to the
  // end of the line, so it may hold spaces.
  std::string TextLine() const {
    return format("event id=%lld day=%s time=%s ticker=%s side=%c qty=%d "
		  "price=%.4f note=%s", (long long)id, Day().c_str(),
		  Time().c_str(), ticker, side, qty, price, note);
  }

  // Parse a line written by TextLine(). Returns false if it is not one.
  bool FromText(const std::string &line) {
    if(!starts_with(line, "event ")) return false;
    std::string rest = line.substr(6);
    std::size_t np = rest.find(" note=");
    if(np == std::string::npos) return false;
    copy_to(note, sizeof(note), rest.substr(np + 6));
    *this = TradeEvent(0, 0, 0, "", 'N', 0, 0, note);
    int got = 0;
    for(const std::string &field : split(rest.substr(0, np), ' ', true)) {
      std::size_t eq = field.find('=');
      if(eq == std::string::npos) return false;
      std::string k = field.substr(0, eq), v = field.substr(eq + 1);
      if(k == "id") id = to_llong(v), got++;
      else if(k == "day") { // YYYY-MM-DD
	auto ymd = split(v, '-');
	if(ymd.size() != 3) return false;
	day = to_int(ymd[0]) * 10000 + to_int(ymd[1]) * 100 + to_int(ymd[2]);
	got++;
      }
      else if(k == "time") { // HH:MM:SS
	auto hms = split(v, ':');
	if(hms.size() != 3) return false;
	seconds = to_int(hms[0]) * 3600 + to_int(hms[1]) * 60 + to_int(hms[2]);
	got++;
      }
      else if(k == "ticker") copy_to(ticker, sizeof(ticker), v), got++;
      else if(k == "side" && v.size() == 1) side = v[0], got++;
      else if(k == "qty") qty = to_int(v), got++;
      else if(k == "price") price = to_double(v), got++;
      else return false; // An unknown field: refuse rather than guess
    }
    return got == 7;
  }

  std::string Day() const {
    return format("%04d-%02d-%02d", day / 10000, day / 100 % 100, day % 100);
  }
  std::string Time() const {
    return format("%02d:%02d:%02d", seconds / 3600, seconds / 60 % 60, seconds % 60);
  }

  // Field by field. The price went through "%.4f" in the text, so
  // compare it to that precision.
  bool Same(const TradeEvent &o) const {
    return id == o.id && day == o.day && seconds == o.seconds &&
      strcmp(ticker, o.ticker) == 0 && side == o.side && qty == o.qty &&
      format("%.4f", price) == format("%.4f", o.price) &&
      strcmp(note, o.note) == 0;
  }

private:
  static void put(std::vector<unsigned char> &b, const void *v, std::size_t n) {
    const unsigned char *p = (const unsigned char *)v;
    b.insert(b.end(), p, p + n);
  }
  static void get(const unsigned char *p, std::size_t &at, void *v, std::size_t n) {
    memcpy(v, p + at, n);
    at += n;
  }

public:
  std::int64_t id = 0;
  std::int32_t day = 0;     // YYYYMMDD
  std::int32_t seconds = 0; // Seconds after midnight, Eastern time
  char ticker[12] = {0};
  char side = 'N';          // B buy, S sell, N note
  std::int32_t qty = 0;
  double price = 0;
  char note[64] = {0};

  static const std::size_t BODY_SIZE = 8 + 4 + 4 + 12 + 1 + 4 + 8 + 64;
};

// ----------------------------------------------------------- //
// The server: stores each block it receives in a database file
// ----------------------------------------------------------- //

static void serve(gxsTLSServer *server, const char *dbname, int *stored)
{
  gxDatabase db;
  if(db.Create(dbname) != gxDBASE_NO_ERROR) {
    std::cerr << "server: cannot create " << dbname << "\n";
    return;
  }
  gxStream stream;
  if(server->Accept(stream.Connection()) != 0) {
    std::cerr << "server: accept failed: " << server->Error() << "\n";
    return;
  }
  std::vector<unsigned char> block;
  for(bool done = false; !done; ) {
    gxBlockHeader hdr;
    if(stream.ReadHeader(hdr) != gxSOCKET_NO_ERROR) break;
    switch(gxStream::BlockStatus(hdr)) {
      case gxAddRemoteBlock: { // Store the block, answer with its address
	if(stream.ReadBlock(block, hdr) != gxSOCKET_NO_ERROR) { done = true; break; }
	FAU_t addr = db.Alloc((__ULWORD__)block.size());
	if(addr == (FAU_t)0 ||
	   db.Write(block.data(), (__ULWORD__)block.size(), addr) != gxDBASE_NO_ERROR) {
	  stream.WriteResultBlock(0);
	  break;
	}
	(*stored)++;
	stream.WriteResultBlock((int)addr);
	break;
      }
      case gxRequestBlock: { // Send back the block at an address
	if(stream.ReadBlock(block, hdr) != gxSOCKET_NO_ERROR) { done = true; break; }
	std::int64_t addr = 0;
	if(block.size() == sizeof(addr)) memcpy(&addr, block.data(), sizeof(addr));
	std::vector<unsigned char> data;
	__ULWORD__ len = addr > 0 ? db.ObjectLength((FAU_t)addr) : 0;
	if(len > 0) {
	  data.resize(len);
	  if(db.Read(data.data(), len, (FAU_t)addr) != gxDBASE_NO_ERROR) data.clear();
	}
	stream.WriteBlock(data.data(), (int)data.size());
	break;
      }
      default: // gxCloseConnection: the client is done
	done = true;
	break;
    }
  }
  stream.Close();
  db.Close();
}

// ----------------------------------------------------------- //
// Export and import
// ----------------------------------------------------------- //

// Walk every object in a database file and write the TradeEvents to
// a text file. Returns the number written, -1 on error.
static int export_text(const char *dbname, const char *txtname)
{
  gxDatabase db;
  if(db.Open(dbname, gxDBASE_READONLY) != gxDBASE_NO_ERROR) return -1;
  std::ofstream out(txtname, std::ios::trunc);
  out << "# TradeEvent export from " << dbname << "\n"
      << "# One event per line; the comment above each says it in words.\n";
  int n = 0;
  std::vector<unsigned char> buf;
  for(FAU_t addr = db.FindFirstObject(); addr != (FAU_t)0;
      addr = db.FindNextObject(addr)) {
    buf.resize(db.ObjectLength(addr));
    if(db.Read(buf.data(), (__ULWORD__)buf.size(), addr) != gxDBASE_NO_ERROR) return -1;
    TradeEvent e;
    if(!e.Unpack(buf.data(), buf.size())) continue; // Some other type
    out << "# " << e.Narrative() << "\n" << e.TextLine() << "\n";
    n++;
  }
  db.Close();
  return out.good() ? n : -1;
}

// Read the text file back into objects and store them in a new
// database file. Comment and blank lines are skipped.
static int import_text(const char *txtname, const char *dbname,
		       std::vector<TradeEvent> &events)
{
  std::ifstream in(txtname);
  gxDatabase db;
  if(!in || db.Create(dbname) != gxDBASE_NO_ERROR) return -1;
  std::string line;
  int lineno = 0;
  while(std::getline(in, line)) {
    lineno++;
    std::string t = trim(line);
    if(t.empty() || t[0] == '#') continue;
    TradeEvent e;
    if(!e.FromText(t)) {
      std::cerr << txtname << ":" << lineno << ": not an event line\n";
      return -1;
    }
    std::vector<unsigned char> b = e.Pack();
    FAU_t addr = db.Alloc((__ULWORD__)b.size());
    if(addr == (FAU_t)0 || db.Write(b.data(), (__ULWORD__)b.size(), addr) != gxDBASE_NO_ERROR)
      return -1;
    events.push_back(e);
  }
  db.Close();
  return (int)events.size();
}

int main(int argc, char **argv)
{
  if(argc < 3) {
    std::cerr << "Usage: " << argv[0] << " cert.pem key.pem [--ca ca.pem | --insecure]\n";
    return 1;
  }
  std::string ca_file;
  bool insecure = false;
  for(int i = 3; i < argc; i++) {
    if(strcmp(argv[i], "--ca") == 0 && i + 1 < argc) ca_file = argv[++i];
    else if(strcmp(argv[i], "--insecure") == 0) insecure = true;
  }

  // The objects this program writes
  const std::vector<TradeEvent> events = {
    TradeEvent(1001, 20261109, 9 * 3600 + 41 * 60 + 7, "AMTX", 'B', 100, 3.25, "opening flush, back over VWAP"),
    TradeEvent(1002, 20261109, 9 * 3600 + 58 * 60 + 30, "AMTX", 'S', 100, 3.61, "target hit"),
    TradeEvent(1003, 20261109, 10 * 3600 + 2 * 60, "SOUN", 'N', 0, 0, "halted, news pending"),
    TradeEvent(1004, 20261110, 4 * 3600 + 15 * 60, "BTCS", 'B', 250, 4.0125, "pre-market gap, small size"),
    TradeEvent(1005, 20261110, 15 * 3600 + 59 * 60 + 59, "BTCS", 'S', 250, 3.9, "flat at the close"),
  };

  // Start the server on a port the system picks (0), on this machine
  gxsTLSServer server;
  if(server.Listen(0, argv[1], argv[2], "", "127.0.0.1") != 0) {
    std::cerr << "Listen failed: " << server.Error() << "\n";
    return 1;
  }
  int port = server.Port(), stored = 0;
  std::cout << "Server listening on 127.0.0.1:" << port << "\n";
  std::thread server_thread(serve, &server, "typed_objects.gxd", &stored);

  // The client: send every object, then read each one back by address
  int failures = 0;
  {
    gxStream stream;
    if(!ca_file.empty()) stream.Connection().SetCAFile(ca_file);
    if(insecure) stream.Connection().SetVerifyPeer(false);
    stream.SetTimeOut(30);
    if(stream.Connect("localhost", port) != gxSOCKET_NO_ERROR) {
      std::cerr << "Connect failed: " << stream.Error() << "\n";
      server.Close();
      server_thread.join();
      return 1;
    }
    for(const TradeEvent &e : events) {
      std::vector<unsigned char> b = e.Pack();
      gxBlockHeader hdr;
      hdr.block_length = (__ULWORD__)b.size();
      int addr = 0;
      if(stream.AddBlock(b.data(), hdr) != gxSOCKET_NO_ERROR ||
	 stream.ReadResultBlock(addr) != gxSOCKET_NO_ERROR || addr == 0) {
	std::cerr << "Send failed: " << stream.SocketExceptionMessage() << "\n";
	return 1;
      }
      std::int64_t req = addr;
      gxBlockHeader req_hdr, reply_hdr;
      req_hdr.block_length = sizeof(req);
      std::vector<unsigned char> reply;
      TradeEvent back;
      bool ok = stream.RequestBlock(&req, req_hdr, reply_hdr, reply) == gxSOCKET_NO_ERROR &&
	back.Unpack(reply.data(), reply.size()) && back.Same(e);
      if(!ok) failures++;
      std::cout << "sent id " << e.id << ", stored at " << addr << ", read back "
		<< (ok ? "OK" : "DIFFERENT") << "\n";
    }
    stream.CloseConnection();
    stream.Close();
  }
  server_thread.join();
  server.Close();
  std::cout << "Server stored " << stored << " objects in typed_objects.gxd\n";

  // Export the stored objects to text, then import the text
  int exported = export_text("typed_objects.gxd", "typed_objects.txt");
  std::cout << "Exported " << exported << " objects to typed_objects.txt\n";
  std::vector<TradeEvent> imported;
  int n = import_text("typed_objects.txt", "typed_objects_import.gxd", imported);
  std::cout << "Imported " << n << " objects into typed_objects_import.gxd\n";

  // Compare what came back with what was written
  if(exported != (int)events.size() || n != (int)events.size()) failures++;
  for(std::size_t i = 0; i < imported.size() && i < events.size(); i++) {
    if(!imported[i].Same(events[i])) {
      std::cout << "id " << events[i].id << " changed in the text round trip\n";
      failures++;
    }
  }
  std::cout << "Sample of the export:\n";
  std::ifstream txt("typed_objects.txt");
  std::string line;
  for(int i = 0; i < 6 && std::getline(txt, line); i++) std::cout << "  " << line << "\n";
  std::cout << (failures == 0 ? "All objects match" : "Some objects differ") << "\n";
  return failures == 0 ? 0 : 1;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
