// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: object_service_test.cpp
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/18/2026
// Date Last Modified: 11/18/2026
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

11/18/2026 (7.4.1): the object service's address checks
(gxdbobject.cpp, gxdo_block_ok()). Before 7.4.1 a client address was
checked only by the check word of the block header in front of it, so
an address inside an object whose bytes held a copy of a real block
header let CHANGE write past that object into the next one (the next
object's header was overwritten), and REQUEST read past it. Run in an
empty folder: it writes objtest.gxd and its journal there.

  - normal: ADD, REQUEST, CHANGE, DELETE of real objects work
  - fake header: REQUEST, CHANGE and DELETE of an address inside an
    object, behind a copied real header, are refused, and the next
    object is unchanged
  - deleted: REQUEST, CHANGE and DELETE of a deleted object are refused
  - bounds: address 0, an address in the file header, past the end of
    the file and past 32 bits are refused
  - reopen: a new service on the same file knows the objects (the live
    list is made at open)
  - read only: a read channel's database refuses a deleted object and
    every change
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include "gxdbobject.h"
#include "gxdbase.h"

using namespace datareel;

static int failures = 0;

static void result(const char *name, bool ok, const std::string &why = std::string())
{
  if(ok) printf("PASS %s\n", name);
  else { printf("FAIL %s %s\n", name, why.c_str()); failures++; }
}

static void p64(std::vector<unsigned char> &v, uint64_t x)
{ for(int i = 0; i < 8; i++) v.push_back((unsigned char)(x >> (8 * i))); }
static uint64_t g64(const std::vector<unsigned char> &v, size_t o)
{ uint64_t x = 0; for(int i = 7; i >= 0; i--) x = (x << 8) | v[o + i]; return x; }

static int req(gxObjectService &s, const char *db, bool write, uint8_t op, const std::vector<unsigned char> &body,
	       std::vector<unsigned char> &reply, std::string &err)
{
  err.clear();
  return s.Request(db, write, op, body, reply, err);
}

static uint64_t add(gxObjectService &s, const std::vector<unsigned char> &obj)
{
  std::vector<unsigned char> b, r; std::string e;
  p64(b, obj.size()); b.insert(b.end(), obj.begin(), obj.end());
  if(req(s, "w", true, gxOBJ_ADD, b, r, e) != 0 || r.size() < 8) return 0;
  return g64(r, 0);
}

static int request(gxObjectService &s, const char *db, uint64_t a, std::vector<unsigned char> &data)
{
  std::vector<unsigned char> b, r; std::string e;
  p64(b, a);
  data.clear();
  if(req(s, db, false, gxOBJ_REQUEST, b, r, e) != 0) return -1;
  if(r.size() < 8 || r.size() - 8 != g64(r, 0)) return -1;
  data.assign(r.begin() + 8, r.end());
  return 0;
}

static int change(gxObjectService &s, const char *db, bool write, uint64_t a, const std::vector<unsigned char> &obj)
{
  std::vector<unsigned char> b, r; std::string e;
  p64(b, a); p64(b, obj.size()); b.insert(b.end(), obj.begin(), obj.end());
  return req(s, db, write, gxOBJ_CHANGE, b, r, e);
}

static int del(gxObjectService &s, const char *db, bool write, uint64_t a)
{
  std::vector<unsigned char> b, r; std::string e;
  p64(b, a);
  return req(s, db, write, gxOBJ_DELETE, b, r, e);
}

int main()
{
  setvbuf(stdout, 0, _IONBF, 0);
  remove("objtest.gxd"); remove("objtest.gxd.jnl");
  std::vector<gxObjectDbConfig> cfg(2);
  cfg[0].name = "w"; cfg[0].path = "objtest.gxd"; cfg[0].access = "write";
  cfg[1].name = "r"; cfg[1].path = "objtest.gxd"; cfg[1].access = "read";

  uint64_t X = 0, A = 0, C = 0, D = 0, inner = 0;
  size_t bhs = 0;
  {
    gxObjectService s(cfg);
    // normal: a real object X gives the header that is copied into A
    std::vector<unsigned char> x(400, 'X'), got;
    X = add(s, x);
    bool ok = X != 0 && request(s, "w", X, got) == 0 && got == x;
    std::vector<unsigned char> x2(400, 'Y');
    ok = ok && change(s, "w", true, X, x2) == 0 && request(s, "w", X, got) == 0 && got == x2;
    result("normal_add_request_change", ok);

    // the header in front of X, as the file holds it
    std::vector<unsigned char> hdr;
    {
      gxDatabase raw;
      if(raw.Open("objtest.gxd", gxDBASE_READONLY) == gxDBASE_NO_ERROR) {
	bhs = raw.BlockHeaderSize();
	hdr.resize(bhs);
	if(raw.Read(hdr.data(), (__ULWORD__)bhs, (FAU_t)(X - bhs)) != gxDBASE_NO_ERROR) hdr.clear();
	raw.Close();
      }
    }
    result("header_copied", hdr.size() == bhs && bhs > 0);

    // A holds a copy of X's header at offset 64: the address after it
    // looks like a 400-byte object that runs past A into C
    std::vector<unsigned char> a(200, 'a');
    if(!hdr.empty()) memcpy(a.data() + 64, hdr.data(), hdr.size());
    A = add(s, a);
    std::vector<unsigned char> c(100, 'C');
    C = add(s, c);
    inner = A + 64 + bhs;
    std::vector<unsigned char> z(400, 'Z');
    result("fake_header_request_refused", request(s, "w", inner, got) != 0);
    result("fake_header_change_refused", change(s, "w", true, inner, z) != 0);
    result("fake_header_delete_refused", del(s, "w", true, inner) != 0);
    result("fake_header_next_object_intact", request(s, "w", C, got) == 0 && got == c &&
	   request(s, "w", A, got) == 0 && got == a);

    // deleted
    std::vector<unsigned char> dv(50, 'D');
    D = add(s, dv);
    ok = D != 0 && del(s, "w", true, D) == 0;
    result("deleted_request_refused", ok && request(s, "w", D, got) != 0);
    result("deleted_change_refused", ok && change(s, "w", true, D, dv) != 0);
    result("deleted_delete_refused", ok && del(s, "w", true, D) != 0);

    // bounds
    result("bounds_zero", request(s, "w", 0, got) != 0);
    result("bounds_file_header", request(s, "w", 8, got) != 0);
    result("bounds_past_eof", request(s, "w", C + 1000000, got) != 0);
    result("bounds_past_32_bits", request(s, "w", 0x100000000ULL + C, got) != 0);
  }
  {
    // reopen: the live list is made at open
    gxObjectService s(cfg);
    std::vector<unsigned char> got;
    result("reopen_objects_known", request(s, "w", X, got) == 0 && got == std::vector<unsigned char>(400, 'Y') &&
	   request(s, "w", C, got) == 0);
    std::vector<unsigned char> z(400, 'Z');
    result("reopen_fake_header_change_refused", change(s, "w", true, inner, z) != 0 &&
	   request(s, "w", C, got) == 0 && got == std::vector<unsigned char>(100, 'C'));
    result("reopen_deleted_refused", request(s, "w", D, got) != 0);
    // read only: a deleted object and every change are refused
    result("read_only_object", request(s, "r", C, got) == 0 && got == std::vector<unsigned char>(100, 'C'));
    result("read_only_deleted_refused", request(s, "r", D, got) != 0);
    result("read_only_change_refused", change(s, "r", false, C, std::vector<unsigned char>(100, 'c')) != 0 &&
	   del(s, "r", false, C) != 0);
  }
  remove("objtest.gxd"); remove("objtest.gxd.jnl");
  if(failures) printf("%d test(s) failed\n", failures);
  return failures ? 1 : 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
