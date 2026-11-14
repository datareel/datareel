// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxdstage.cpp
// Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/14/2026
// Date Last Modified: 11/14/2026
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
License along with this library; if not, see
<https://www.gnu.org/licenses/>.

Staged writes (gxdstage.h).
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#if !defined (__WIN32__)
#include <unistd.h>
#else
// 11/14/2026 (Windows): the CRT's file calls for the one place that reads a file back (Forget()). A staged scope
// is only made by gxrepl's incremental derived apply, which is POSIX only, so on Windows this is never used in
// practice; it is kept correct, not fast.
#include <io.h>
#endif
#include <algorithm>
#include "gxdstage.h"
#include "gxdbase.h"
#include "gxdpcache.h"

namespace datareel {

static thread_local gxdStagedScope *cur_scope = 0;

gxdStagedScope::gxdStagedScope()
{
  prev = cur_scope;
  cur_scope = this;
  lost = 0;
  done = 0;
}

gxdStagedScope::~gxdStagedScope()
{
  done = 1;                // 11/14/2026: from here a closed file's changes are dropped
  Rollback();
  cur_scope = prev;
}

gxdStagedScope *gxdStagedScope::Current()
{
  return cur_scope;
}

void gxdStagedScope::Add(gxDatabase *f)
{
  if(std::find(files.begin(), files.end(), f) == files.end()) files.push_back(f);
  // 11/14/2026 (review 3): a file closed with changes and opened again in the scope would read the file without
  // them (they are held here, not in the file): its changes cannot be followed
  for(auto &c : closed) if(c.path == f->DatabaseName()) lost = 1;
}

void gxdStagedScope::NoteOpen(const char *path)
{
  for(gxdStagedScope *s = cur_scope; s; s = s->prev)
    for(auto &c : s->closed) if(c.path == path) s->lost = 1;
}

// The changed pages of a staged cache, as Capture() gives them
static void snapshot(gxDatabase *f, gxdStagedFile &sf)
{
  gxdPageCache *pc = f->PageCache();
  std::vector<gxdPage *> v;
  pc->GetDirtyPages(v);
  sf.path = f->DatabaseName();
  sf.size = (unsigned long long)pc->FileSize();
  sf.page_no.reserve(v.size());
  sf.data.resize(v.size() * gxdPageSize);
  for(size_t i = 0; i < v.size(); i++) {
    sf.page_no.push_back((unsigned long long)v[i]->page_no);
    memcpy(sf.data.data() + i * gxdPageSize, v[i]->data, gxdPageSize);
  }
}

void gxdStagedScope::Forget(gxDatabase *f)
// A staged cache going away (its file closed). 11/14/2026 (review 3): its changes are kept by the scope for
// Capture() (a file closed after its last write, e.g. a B-tree writing its header at Close). Only once the scope
// has ended are they dropped.
{
  for(gxdStagedScope *s = cur_scope; s; s = s->prev) {
    auto it = std::find(s->files.begin(), s->files.end(), f);
    if(it == s->files.end()) continue;
    gxdPageCache *pc = f->PageCache();
    if(pc && pc->DirtyPages() && !s->done) {
      gxdStagedFile sf;
      snapshot(f, sf);
      // pages a close rewrote as they are in the file (a header written again at Close) are no change
#if !defined (__WIN32__)
      int fd = open(sf.path.c_str(), O_RDONLY | O_CLOEXEC);
      if(fd >= 0) {
	struct stat st;
	bool same_size = fstat(fd, &st) == 0 && (unsigned long long)st.st_size == sf.size;
	std::vector<unsigned char> disk(gxdPageSize);
	gxdStagedFile kept;
	kept.path = sf.path; kept.size = sf.size;
	for(size_t i = 0; i < sf.page_no.size(); i++) {
	  const unsigned char *pg = sf.data.data() + i * gxdPageSize;
	  ssize_t want = (ssize_t)std::min<unsigned long long>(gxdPageSize, sf.size > sf.page_no[i] * gxdPageSize ?
							       sf.size - sf.page_no[i] * gxdPageSize : 0);
	  ssize_t got = want > 0 ? pread(fd, disk.data(), (size_t)want, (off_t)(sf.page_no[i] * gxdPageSize)) : 0;
	  if(same_size && want > 0 && got == want && memcmp(disk.data(), pg, (size_t)want) == 0) continue;
	  kept.page_no.push_back(sf.page_no[i]);
	  kept.data.insert(kept.data.end(), pg, pg + gxdPageSize);
	}
	close(fd);
#else
      int fd = _open(sf.path.c_str(), _O_RDONLY | _O_BINARY);
      if(fd >= 0) {
	struct _stat64 st;
	bool same_size = _fstat64(fd, &st) == 0 && (unsigned long long)st.st_size == sf.size;
	std::vector<unsigned char> disk(gxdPageSize);
	gxdStagedFile kept;
	kept.path = sf.path; kept.size = sf.size;
	for(size_t i = 0; i < sf.page_no.size(); i++) {
	  const unsigned char *pg = sf.data.data() + i * gxdPageSize;
	  long long want = (long long)std::min<unsigned long long>(gxdPageSize, sf.size > sf.page_no[i] * gxdPageSize ?
							       sf.size - sf.page_no[i] * gxdPageSize : 0);
	  long long got = want > 0 && _lseeki64(fd, (long long)(sf.page_no[i] * gxdPageSize), SEEK_SET) >= 0 ?
	    (long long)_read(fd, disk.data(), (unsigned)want) : 0;
	  if(same_size && want > 0 && got == want && memcmp(disk.data(), pg, (size_t)want) == 0) continue;
	  kept.page_no.push_back(sf.page_no[i]);
	  kept.data.insert(kept.data.end(), pg, pg + gxdPageSize);
	}
	_close(fd);
#endif
	if(kept.page_no.empty() && same_size) { s->files.erase(it); continue; }
	sf = std::move(kept);
      }
      s->closed.push_back(std::move(sf));
    }
    s->files.erase(it);
  }
}

unsigned long long gxdStagedScope::StagedBytes() const
{
  unsigned long long n = 0;
  for(auto f : files) {
    gxdPageCache *pc = f->PageCache();
    if(pc) n += (unsigned long long)pc->DirtyPages() * gxdPageSize;
  }
  return n;
}

int gxdStagedScope::Capture(std::vector<gxdStagedFile> &out)
{
  out.clear();
  if(lost) return -1;
  for(auto &c : closed) out.push_back(c);           // files closed with changes (each path once: see Add())
  for(auto f : files) {
    gxdPageCache *pc = f->PageCache();
    if(!pc || !pc->DirtyPages()) continue;
    gxdStagedFile sf;
    snapshot(f, sf);
    out.push_back(std::move(sf));
  }
  return 0;
}

void gxdStagedScope::Rollback()
{
  for(auto f : files) {
    gxdPageCache *pc = f->PageCache();
    if(!pc) continue;
    if(pc->InTxn()) pc->RollbackTxn();
    f->NoteFileChanged();
    pc->BeginTxn();        // Still staged: a later write stays in memory
  }
  // 11/14/2026 (review 3): the scope goes on (the next index of a folder): a file closed with changes after this
  // is still caught (before, Rollback() set done and a later close's changes were dropped without a word)
  closed.clear();
  lost = 0;
}

} // namespace datareel
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
