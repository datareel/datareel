// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxdstage.h
// Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/14/2026
// Date Last Modified: 11/14/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ---------- Include File Description and Details  ---------- //
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
License along with this library; if not, see
<https://www.gnu.org/licenses/>.

Staged writes (DataReel 7.4, per-object derived apply). While a
gxdStagedScope is alive in a thread, every gxDatabase of that thread
that enables a page cache, or writes without one, gets a staged cache
(gxdpcache.h) inside a transaction: its changes stay in memory and
nothing can reach the file, not even when the object is closed. The
owner of the scope takes the changed pages with Capture() (to write
them later as part of one atomic batch, gxdJournal::ApplyPages()) and
then drops them with Rollback(). A kill at any time leaves every file
as it was.

Rules for code that runs inside a scope (a derived index extractor,
gxrextract.h): a file may be closed before Capture() (its changes are
kept by the scope, 11/14/2026), but never opened again in the same
scope after it was closed with changes (Capture() then fails: its reads
would miss them). Write every header the file needs before Capture().

Changes:
==============================================================
11/14/2026: New in DataReel 7.4.
==============================================================
11/14/2026: Review 3: Rollback() no longer ends the scope; a file closed
with changes is kept for Capture(); one opened again after that is lost.
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_DATABASE_STAGED_SCOPE_HPP__
#define __GX_DATABASE_STAGED_SCOPE_HPP__

#include "gxdlcode.h"

#include <string>
#include <vector>

namespace datareel {

class gxDatabase;

// The changed pages of one file
struct gxdStagedFile {
  std::string path;
  unsigned long long size = 0;                         // File size with the changes
  std::vector<unsigned long long> page_no;
  std::vector<unsigned char> data;                     // page_no.size() pages
};

class GXDLCODE_API gxdStagedScope
{
public:
  gxdStagedScope();
  ~gxdStagedScope();  // Rollback() if not done

private:
  gxdStagedScope(const gxdStagedScope &);
  void operator=(const gxdStagedScope &);

public:
  static gxdStagedScope *Current();     // This thread's scope, or 0
  // Bytes of changed pages held now by every file of the scope
  unsigned long long StagedBytes() const;
  // The changed pages of every file still open in the scope. Returns 0,
  // or -1 if a file of the scope was closed with changes (they are lost:
  // the caller must not use the capture).
  // 11/14/2026 (review 3): a file closed with changes is included (the
  // scope keeps its pages), unless it was opened again in the scope (-1).
  int Capture(std::vector<gxdStagedFile> &out);
  // Drop every change, also of closed files; the caches stay staged and
  // the scope goes on (a later close is caught again)
  void Rollback();
  int Lost() const { return lost; }

public: // Used by gxDatabase
  void Add(gxDatabase *f);
  static void Forget(gxDatabase *f);
  static void NoteOpen(const char *path);   // 11/14/2026: gxDatabase::Open()

private:
  std::vector<gxDatabase *> files;
  std::vector<gxdStagedFile> closed;   // 11/14/2026: closed with changes
  gxdStagedScope *prev;
  int lost;
  int done;
};

} // namespace datareel

#endif // __GX_DATABASE_STAGED_SCOPE_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
