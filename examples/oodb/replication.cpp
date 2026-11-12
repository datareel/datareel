// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: replication.cpp
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
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
USA

Database replication over gxStream: a primary writes, a replica
follows, and both files end up the same.

About this example: it builds replication by hand from the basic
pieces (gxDatabase and gxStream) to show the idea and the parts that
matter. For real use, DataReel 7.3.1 and later have replication built
in: the gxrepl tool (utils/gxrepl, see utils/gxrepl/README.md) and its
classes (gxdrepl.h, gxrepl.h). This example shows:

 - The primary is the only process that writes. For each change it
   makes to its own database file (insert, update or delete) it sends
   one operation to the replica: the operation, the file address and
   the record bytes.
 - The replica applies the operations in the same order to its own
   file. gxDatabase gives out file addresses deterministically (the
   same sequence of Alloc() and Delete() calls on two files gives the
   same addresses), so the replica's records land at the primary's
   addresses. The replica answers every insert with the address it
   got, and the primary checks that they are equal.
 - At the end the primary asks the replica for a checksum of its file
   (record count and a CRC-32 of every live record with its address)
   and compares it with its own.
 - Every operation travels over TLS (gxStream always uses TLS) and is
   acknowledged before the next one is sent, so the replica is never
   more than one operation behind.

A production replication system needs more than this: a write-ahead
log the replica can catch up from after it was down, transactions
(gxdJournal, see journal_recovery.cpp), several readers, reconnects
and a way to promote the replica. That is what gxrepl provides.

The primary and the replica are two processes that talk over
localhost. On POSIX systems "demo" forks the replica itself. On any
system you can run the two halves in two terminals.

Usage:
  replication demo cert.pem key.pem [--ca ca.pem | --insecure]
        Fork a replica on a free port, run the primary against it and
        stop the replica (POSIX only).
  replication replica port cert.pem key.pem
        Run only the replica. It writes replica.gxd until the primary
        stops it. Port 0 lets the system pick one; it is printed.
  replication primary port [--ca ca.pem | --insecure]
        Run only the primary against a replica on this machine. It
        writes primary.gxd.

Exits with 0 when the replica's file matches the primary's.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#if !defined(_WIN32)
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#include "gxcrc32.h"
#include "gxdbase.h"
#include "gxstream.h"
#include "strutil.h"

using namespace datareel;

const char *PRIMARY_FILE = "primary.gxd";
const char *REPLICA_FILE = "replica.gxd";

// The operations the primary ships. Each one is sent as a block:
// an OpHeader followed by the record bytes (none for a delete).
enum OpCode : std::uint8_t { OP_INSERT = 1, OP_UPDATE = 2, OP_DELETE = 3 };
struct OpHeader {
  std::uint8_t op;
  std::uint8_t pad[7];
  std::int64_t addr; // The record's file address in the primary
};

// The sample record: an account and its balance
struct Account {
  std::int32_t id;
  std::int32_t version; // Bumped on each update
  double balance;
  char owner[48];
};

// Count the live records in a database file and CRC them with their
// addresses, in file order. Two files with the same records at the
// same addresses give the same answer.
static void file_checksum(gxDatabase &db, std::uint64_t &count, std::uint32_t &crc)
{
  std::vector<char> buf;
  unsigned long c = 0xFFFFFFFFUL;
  count = 0;
  for(FAU_t addr = db.FindFirstObject(); addr != (FAU_t)0; addr = db.FindNextObject(addr)) {
    buf.resize(db.ObjectLength(addr));
    if(db.Read(buf.data(), (__ULWORD__)buf.size(), addr) != gxDBASE_NO_ERROR) continue;
    std::int64_t a = (std::int64_t)addr;
    for(std::size_t i = 0; i < sizeof(a); i++) c = calcCRC32(((unsigned char *)&a)[i], c);
    for(char ch : buf) c = calcCRC32((unsigned char)ch, c);
    count++;
  }
  crc = (std::uint32_t)(c ^ 0xFFFFFFFFUL);
}

// ----------------------------------------------------------- //
// The replica
// ----------------------------------------------------------- //

// Apply operations from one primary until it stops the replica.
// Returns 0 when it was stopped cleanly.
static int run_replica(gxsTLSServer &server)
{
  gxDatabase db;
  if(db.Create(REPLICA_FILE) != gxDBASE_NO_ERROR) {
    std::cerr << "replica: cannot create " << REPLICA_FILE << "\n";
    return 1;
  }
  int applied = 0;
  for(bool running = true; running; ) {
    gxStream stream;
    if(server.Accept(stream.Connection()) != 0) {
      std::cerr << "replica: accept failed: " << server.Error() << "\n";
      return 1;
    }
    std::vector<unsigned char> block;
    for(bool session = true; session; ) {
      gxBlockHeader hdr;
      if(stream.ReadHeader(hdr) != gxSOCKET_NO_ERROR) break; // Primary gone
      switch(gxStream::BlockStatus(hdr)) {
	case gxAddRemoteBlock: { // One operation
	  if(stream.ReadBlock(block, hdr) != gxSOCKET_NO_ERROR || block.size() < sizeof(OpHeader)) {
	    session = false;
	    break;
	  }
	  OpHeader op;
	  memcpy(&op, block.data(), sizeof(op));
	  const unsigned char *data = block.data() + sizeof(op);
	  __ULWORD__ len = (__ULWORD__)(block.size() - sizeof(op));
	  int result = 0;
	  if(op.op == OP_INSERT) {
	    // Allocate in the same order as the primary did: the address
	    // should be the same. Send it back so the primary can check.
	    FAU_t addr = db.Alloc(len);
	    if(addr != (FAU_t)0 && db.Write(data, len, addr) == gxDBASE_NO_ERROR) result = (int)addr;
	  }
	  else if(op.op == OP_UPDATE) {
	    result = db.Write(data, len, (FAU_t)op.addr) == gxDBASE_NO_ERROR ? 1 : 0;
	  }
	  else if(op.op == OP_DELETE) {
	    result = db.Delete((FAU_t)op.addr) ? 1 : 0;
	  }
	  if(result) applied++;
	  db.Flush(); // Make each change visible in the file before the ack
	  stream.WriteResultBlock(result);
	  break;
	}
	case gxRequestBlock: { // The primary asks for the file's checksum
	  if(stream.ReadBlock(block, hdr) != gxSOCKET_NO_ERROR) { session = false; break; }
	  std::uint64_t count = 0;
	  std::uint32_t crc = 0;
	  file_checksum(db, count, crc);
	  unsigned char reply[12];
	  memcpy(reply, &count, 8);
	  memcpy(reply + 8, &crc, 4);
	  stream.WriteBlock(reply, (int)sizeof(reply));
	  break;
	}
	case gxKillServer:
	  running = false;
	  session = false;
	  break;
	default: // gxCloseConnection
	  session = false;
	  break;
      }
    }
    stream.Close();
  }
  db.Close();
  std::cout << "replica: applied " << applied << " operations to " << REPLICA_FILE << "\n";
  return 0;
}

// ----------------------------------------------------------- //
// The primary
// ----------------------------------------------------------- //

class Primary
{
public:
  Primary() { }

  bool Start(int port, const std::string &ca_file, bool insecure) {
    if(db.Create(PRIMARY_FILE) != gxDBASE_NO_ERROR) {
      std::cerr << "primary: cannot create " << PRIMARY_FILE << "\n";
      return false;
    }
    if(!ca_file.empty()) stream.Connection().SetCAFile(ca_file);
    if(insecure) stream.Connection().SetVerifyPeer(false);
    stream.SetTimeOut(30);
    if(stream.Connect("localhost", port) != gxSOCKET_NO_ERROR) {
      std::cerr << "primary: connect failed: " << stream.Error() << "\n";
      return false;
    }
    return true;
  }

  // Each change is made to the primary's file first, then shipped.
  // The functions return false if the replica did not apply it.
  bool Insert(const Account &a, FAU_t &addr) {
    addr = db.Alloc(sizeof(a));
    if(addr == (FAU_t)0 || db.Write(&a, sizeof(a), addr) != gxDBASE_NO_ERROR) return false;
    int got = ship(OP_INSERT, addr, &a, sizeof(a));
    if(got != (int)addr) {
      std::cout << "  insert id " << a.id << ": primary address " << (long long)addr
		<< " but the replica used " << got << "\n";
      return false;
    }
    return true;
  }
  bool Update(FAU_t addr, const Account &a) {
    if(db.Write(&a, sizeof(a), addr) != gxDBASE_NO_ERROR) return false;
    return ship(OP_UPDATE, addr, &a, sizeof(a)) == 1;
  }
  bool Delete(FAU_t addr) {
    if(!db.Delete(addr)) return false;
    return ship(OP_DELETE, addr, nullptr, 0) == 1;
  }

  // Compare the replica's file with ours
  bool Compare() {
    db.Flush();
    std::uint64_t pc = 0, rc = 0;
    std::uint32_t pcrc = 0, rcrc = 0;
    file_checksum(db, pc, pcrc);
    std::int64_t dummy = 0;
    gxBlockHeader req, reply_hdr;
    req.block_length = sizeof(dummy);
    std::vector<unsigned char> reply;
    if(stream.RequestBlock(&dummy, req, reply_hdr, reply) != gxSOCKET_NO_ERROR || reply.size() != 12) {
      std::cerr << "primary: checksum request failed\n";
      return false;
    }
    memcpy(&rc, reply.data(), 8);
    memcpy(&rcrc, reply.data() + 8, 4);
    std::cout << format("primary: %llu records, CRC %08X\n", (unsigned long long)pc, pcrc)
	      << format("replica: %llu records, CRC %08X\n", (unsigned long long)rc, rcrc);
    return pc == rc && pcrc == rcrc;
  }

  void Stop(bool stop_replica) {
    if(stop_replica) stream.TerminateConnection();
    else stream.CloseConnection();
    stream.Close();
    db.Close();
  }

  int shipped = 0;

private:
  // Send one operation and wait for the replica's answer
  int ship(OpCode code, FAU_t addr, const void *data, std::size_t len) {
    OpHeader op;
    memset(&op, 0, sizeof(op));
    op.op = code;
    op.addr = (std::int64_t)addr;
    std::vector<unsigned char> block(sizeof(op) + len);
    memcpy(block.data(), &op, sizeof(op));
    if(len) memcpy(block.data() + sizeof(op), data, len);
    gxBlockHeader hdr;
    hdr.block_length = (__ULWORD__)block.size();
    int result = 0;
    if(stream.AddBlock(block.data(), hdr) != gxSOCKET_NO_ERROR ||
       stream.ReadResultBlock(result) != gxSOCKET_NO_ERROR) return -1;
    shipped++;
    return result;
  }

  gxDatabase db;
  gxStream stream;
};

static Account make_account(int id, double balance, const char *owner)
{
  Account a;
  memset(&a, 0, sizeof(a));
  a.id = id;
  a.balance = balance;
  copy_to(a.owner, sizeof(a.owner), owner);
  return a;
}

// The primary's work: inserts, updates, deletes and inserts that reuse
// the deleted blocks, each one shipped as it happens
static int run_primary(int port, const std::string &ca_file, bool insecure, bool stop_replica)
{
  Primary p;
  if(!p.Start(port, ca_file, insecure)) return 1;
  int failures = 0;
  std::vector<FAU_t> addrs;
  const char *owners[] = {"Avery", "Blake", "Casey", "Drew", "Emery"};
  for(int id = 1; id <= 20; id++) {
    FAU_t addr = 0;
    if(!p.Insert(make_account(id, 100.0 * id, owners[id % 5]), addr)) failures++;
    addrs.push_back(addr);
  }
  std::cout << "primary: inserted 20 accounts\n";
  for(int i : {2, 5, 8, 11, 14}) { // Deposits
    Account a = make_account(i + 1, 100.0 * (i + 1) + 50.25, owners[(i + 1) % 5]);
    a.version = 1;
    if(!p.Update(addrs[i], a)) failures++;
  }
  std::cout << "primary: updated 5 accounts\n";
  for(int i : {3, 9, 15, 19}) if(!p.Delete(addrs[i])) failures++;
  std::cout << "primary: deleted 4 accounts\n";
  for(int id = 21; id <= 23; id++) { // These reuse deleted blocks
    FAU_t addr = 0;
    if(!p.Insert(make_account(id, 10.0 * id, owners[id % 5]), addr)) failures++;
    std::cout << "primary: account " << id << " went to address " << (long long)addr
	      << " on both sides\n";
  }
  std::cout << "primary: shipped " << p.shipped << " operations, " << failures
	    << " not applied the same way\n";
  bool same = p.Compare();
  p.Stop(stop_replica);
  std::cout << (same && failures == 0 ? "The replica matches the primary"
		: "The replica DIFFERS from the primary") << "\n";
  return same && failures == 0 ? 0 : 1;
}

static void tls_options(int argc, char **argv, int first, std::string &ca, bool &insecure)
{
  for(int i = first; i < argc; i++) {
    if(strcmp(argv[i], "--ca") == 0 && i + 1 < argc) ca = argv[++i];
    else if(strcmp(argv[i], "--insecure") == 0) insecure = true;
  }
}

int main(int argc, char **argv)
{
  std::string mode = argc > 1 ? argv[1] : "";
  std::string ca;
  bool insecure = false;

  if(mode == "replica" && argc >= 5) {
    gxsTLSServer server;
    if(server.Listen(atoi(argv[2]), argv[3], argv[4], "", "127.0.0.1") != 0) {
      std::cerr << "replica: listen failed: " << server.Error() << "\n";
      return 1;
    }
    std::cout << "replica: listening on 127.0.0.1:" << server.Port() << std::endl;
    return run_replica(server);
  }
  if(mode == "primary" && argc >= 3) {
    tls_options(argc, argv, 3, ca, insecure);
    return run_primary(atoi(argv[2]), ca, insecure, true);
  }
#if !defined(_WIN32)
  if(mode == "demo" && argc >= 4) {
    tls_options(argc, argv, 4, ca, insecure);
    // Listen before forking so the port is known and the replica is
    // ready before the primary connects
    gxsTLSServer server;
    if(server.Listen(0, argv[2], argv[3], "", "127.0.0.1") != 0) {
      std::cerr << "replica: listen failed: " << server.Error() << "\n";
      return 1;
    }
    int port = server.Port();
    std::cout << "replica: listening on 127.0.0.1:" << port << std::endl;
    pid_t pid = fork();
    if(pid < 0) { perror("fork"); return 1; }
    if(pid == 0) { // The replica process
      int rc = run_replica(server);
      std::cout.flush();
      _exit(rc);
    }
    server.Close(); // The parent is only the primary
    int rc = run_primary(port, ca, insecure, true);
    int status = 0;
    waitpid(pid, &status, 0);
    if(!WIFEXITED(status) || WEXITSTATUS(status) != 0) rc = 1;
    return rc;
  }
#endif
  std::cerr << "Usage:\n"
#if !defined(_WIN32)
	    << "  " << argv[0] << " demo cert.pem key.pem [--ca ca.pem | --insecure]\n"
#endif
	    << "  " << argv[0] << " replica port cert.pem key.pem\n"
	    << "  " << argv[0] << " primary port [--ca ca.pem | --insecure]\n";
  return 1;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
