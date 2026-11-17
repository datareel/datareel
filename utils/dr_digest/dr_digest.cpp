// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: dr_digest.cpp
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

What a database file holds (gxddigest.h), printed as
"HASH COUNT FILE": the live objects of a .gxd, the keys of a .btx in
order, or the bytes. The same as "gxrepl digest" of the 7.4 betas.

Usage: dr_digest FILE [--kind btree|bytes|objects]
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <string>
#include "gxddigest.h"

int main(int argc, char **argv)
{
  if(argc != 2 && !(argc == 4 && std::string(argv[2]) == "--kind")) {
    fprintf(stderr, "Usage: dr_digest FILE [--kind btree|bytes|objects]\n");
    return 2;
  }
  uint64_t h = 0, n = 0;
  std::string err;
  if(datareel::gxDatabaseDigest(argv[1], argc == 4 ? argv[3] : "", h, n, err) != 0) {
    fprintf(stderr, "dr_digest: %s\n", err.c_str());
    return 1;
  }
  printf("%016llx %llu %s\n", (unsigned long long)h, (unsigned long long)n, argv[1]);
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
