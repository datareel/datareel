// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxdpcache.h
// Compiler Used: MSVC, GCC
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

The gxdPageCache class keeps 4 KB pages of one gxDatabase file in
memory. It is turned on with gxDatabase::EnablePageCache() and is
used by every gxDatabase read and write, so the B-tree nodes, block
headers, file header and objects of a file are read from memory once
they are loaded.

Outside a transaction the cache writes through: each write goes to
the file at once and also updates any cached page, so the file on
disk is the same as without the cache.

Inside a gxdJournal transaction writes stay in memory as dirty pages.
When the transaction commits the journal appends the dirty pages to
its log and they become logged pages: they stay in memory, pinned,
until the journal's next checkpoint writes them to the file. Rolling
back a transaction drops its dirty pages (a logged page that was
changed again gets its committed contents back).

The cache does not see writes made by other processes or by another
gxDatabase object opened on the same file. Enable it only for files
that one gxDatabase object owns while the cache is on.

Changes:
==============================================================
10/31/2026: New in DataReel 7.2.
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_DATABASE_PAGE_CACHE_HPP__
#define __GX_DATABASE_PAGE_CACHE_HPP__

#include "gxdlcode.h"

#include <unordered_map>
#include <vector>
#include "gxdfptr.h"

namespace datareel {

const __ULWORD__ gxdPageSize = 4096;  // Bytes per cache page
const size_t gxdDefaultPageCacheSize = 16 * 1024 * 1024;

struct GXDLCODE_API gxdPage {
  FAU_t page_no;       // File offset / gxdPageSize
  gxdPage *prev;       // Previous page in the LRU list
  gxdPage *next;       // Next page in the LRU list
  unsigned char *orig; // Committed contents of a logged page changed
                       // again in the open transaction
  int dirty;           // Changed in the open transaction
  int logged;          // Committed to the journal, not yet in the file
  unsigned char data[gxdPageSize];
};

class GXDLCODE_API gxdPageCache
{
public:
  gxdPageCache(gxdFPTR *f, FAU_t size, size_t max_bytes);
  ~gxdPageCache();

private:
  gxdPageCache(const gxdPageCache &) { }
  void operator=(const gxdPageCache &) { }

public:
  // Read up to "bytes" at "address". Returns the number of bytes read,
  // which is less than "bytes" only at the end of the file.
  __ULWORD__ Read(void *buf, __ULWORD__ bytes, FAU_t address);

  // Write "bytes" at "address". Returns the number of bytes written.
  __ULWORD__ Write(const void *buf, __ULWORD__ bytes, FAU_t address);

  FAU_t FileSize() const { return file_size; }
  size_t MaxBytes() const { return max_pages * gxdPageSize; }
  size_t CachedPages() const { return pages.size(); }
  void Clear(); // Drop every page, including dirty and logged pages

public: // Transaction functions used by gxdJournal
  void BeginTxn();
  void CommitTxn();   // Dirty pages become logged pages
  void RollbackTxn(); // Drop the open transaction's changes
  int InTxn() const { return in_txn; }
  size_t DirtyPages() const { return dirty_count; }
  size_t LoggedPages() const { return logged_count; }
  void GetDirtyPages(std::vector<gxdPage *> &v);
  int WriteLoggedPages(); // Checkpoint: write logged pages to the file
  void SetFileSize(FAU_t size) { file_size = size; }

public: // Statistics
  unsigned long long hits;
  unsigned long long misses;

private:
  gxdPage *GetPage(FAU_t page_no);
  void LinkFront(gxdPage *p);
  void Unlink(gxdPage *p);
  void FreePage(gxdPage *p);
  void DropPage(gxdPage *p);
  void EvictClean();
  void GetPages(std::vector<gxdPage *> &v, int logged);

private:
  gxdFPTR *fp;
  FAU_t file_size;  // Logical file size, including dirty pages
  FAU_t begin_size; // File size when the transaction began
  size_t max_pages;
  size_t dirty_count;
  size_t logged_count;
  size_t pinned_count; // Dirty or logged pages, kept out of the LRU list
  int in_txn;
  std::unordered_map<FAU_t, gxdPage *> pages;
  std::vector<gxdPage *> dirty_list;  // Pages changed in the transaction
  std::vector<gxdPage *> logged_list; // Logged pages, some may be dirty
  gxdPage *head; // Most recently used clean page
  gxdPage *tail; // Least recently used clean page
};

} // namespace datareel

#endif // __GX_DATABASE_PAGE_CACHE_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
