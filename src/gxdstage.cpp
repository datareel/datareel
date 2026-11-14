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
  if(!done) Rollback();
  cur_scope = prev;
}

gxdStagedScope *gxdStagedScope::Current()
{
  return cur_scope;
}

void gxdStagedScope::Add(gxDatabase *f)
{
  if(std::find(files.begin(), files.end(), f) == files.end()) files.push_back(f);
}

void gxdStagedScope::Forget(gxDatabase *f)
// A staged cache going away: its changes are lost (never written)
{
  for(gxdStagedScope *s = cur_scope; s; s = s->prev) {
    auto it = std::find(s->files.begin(), s->files.end(), f);
    if(it == s->files.end()) continue;
    gxdPageCache *pc = f->PageCache();
    if(pc && pc->DirtyPages() && !s->done) s->lost = 1;
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
  for(auto f : files) {
    gxdPageCache *pc = f->PageCache();
    if(!pc || !pc->DirtyPages()) continue;
    std::vector<gxdPage *> v;
    pc->GetDirtyPages(v);
    gxdStagedFile sf;
    sf.path = f->DatabaseName();
    sf.size = (unsigned long long)pc->FileSize();
    sf.page_no.reserve(v.size());
    sf.data.resize(v.size() * gxdPageSize);
    for(size_t i = 0; i < v.size(); i++) {
      sf.page_no.push_back((unsigned long long)v[i]->page_no);
      memcpy(sf.data.data() + i * gxdPageSize, v[i]->data, gxdPageSize);
    }
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
  done = 1;
}

} // namespace datareel
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
