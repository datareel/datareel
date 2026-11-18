// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxdpcache.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/31/2026
// Date Last Modified: 10/31/2026
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

Page cache for gxDatabase files. See gxdpcache.h.

Clean pages are kept in a least recently used list and dropped when
the cache is full. Dirty and logged pages are pinned: they are not in
the list and are never dropped until a commit or checkpoint.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <string.h>
#include <algorithm>
#include "gxdpcache.h"


gxdPageCache::gxdPageCache(gxdFPTR *f, FAU_t size, size_t max_bytes,
			   int use_map)
{
  fp = f;
  file_size = begin_size = size;
  this->use_map = use_map;
  map = 0;
  map_len = (FAU_t)0;
  disk_size = size;
  map_handle = 0;
  max_pages = max_bytes / gxdPageSize;
  if(max_pages < 16) max_pages = 16;
  dirty_count = logged_count = pinned_count = 0;
  in_txn = 0;
  head = tail = 0;
  hits = misses = 0;
}

gxdPageCache::~gxdPageCache()
{
  Clear();
  Unmap();
}

void gxdPageCache::Unmap()
{
  if(map) gxdFPTRUnmap((void *)map, map_len, map_handle);
  map = 0;
  map_len = (FAU_t)0;
  map_handle = 0;
}

void gxdPageCache::Remap()
// Map the whole file as it is on disk now
{
  Unmap();
  if(!use_map || disk_size <= (FAU_t)0) return;
  map = (const unsigned char *)gxdFPTRMap(fp, disk_size, &map_handle);
  if(map) map_len = disk_size;
  else use_map = 0; // Not supported: read pages instead
}

void gxdPageCache::Clear()
{
  for(auto &e : pages) FreePage(e.second);
  pages.clear();
  dirty_list.clear();
  logged_list.clear();
  head = tail = 0;
  dirty_count = logged_count = pinned_count = 0;
}

void gxdPageCache::FreePage(gxdPage *p)
{
  if(p->orig) delete[] p->orig;
  delete p;
}

void gxdPageCache::LinkFront(gxdPage *p)
{
  p->prev = 0;
  p->next = head;
  if(head) head->prev = p;
  head = p;
  if(!tail) tail = p;
}

void gxdPageCache::Unlink(gxdPage *p)
{
  if(p->prev) p->prev->next = p->next; else head = p->next;
  if(p->next) p->next->prev = p->prev; else tail = p->prev;
  p->prev = p->next = 0;
}

void gxdPageCache::DropPage(gxdPage *p)
// Remove a pinned page from the cache
{
  pages.erase(p->page_no);
  FreePage(p);
}

void gxdPageCache::EvictClean()
// Drop least recently used clean pages until there is room for one
// more clean page.
{
  while(tail && (pages.size() - pinned_count >= max_pages)) {
    gxdPage *p = tail;
    Unlink(p);
    pages.erase(p->page_no);
    FreePage(p);
  }
}

gxdPage *gxdPageCache::GetPage(FAU_t page_no)
// Return the cached page, loading it from the file if needed.
{
  auto it = pages.find(page_no);
  if(it != pages.end()) {
    gxdPage *p = it->second;
    if(!p->dirty && !p->logged && (p != head)) { Unlink(p); LinkFront(p); }
    hits++;
    return p;
  }
  misses++;
  if(pages.size() - pinned_count >= max_pages) EvictClean();
  gxdPage *p = new gxdPage;
  p->page_no = page_no;
  p->prev = p->next = 0;
  p->orig = 0;
  p->dirty = p->logged = 0;
  __ULWORD__ n = 0;
  FAU_t offset = page_no * (FAU_t)gxdPageSize;
  if(offset < file_size) {
    n = gxdFPTRReadAt(fp, p->data, gxdPageSize, offset);
    if(n > gxdPageSize) n = 0;
  }
  if(n < gxdPageSize) memset(p->data + n, 0, gxdPageSize - n);
  pages[page_no] = p;
  LinkFront(p);
  return p;
}

__ULWORD__ gxdPageCache::Read(void *buf, __ULWORD__ bytes, FAU_t address)
{
  if(address < (FAU_t)0 || address >= file_size) return (__ULWORD__)0;
  if((FAU_t)bytes > file_size - address) bytes = (__ULWORD__)(file_size - address);
  unsigned char *out = (unsigned char *)buf;
  __ULWORD__ done = 0;
  while(done < bytes) {
    FAU_t a = address + done;
    FAU_t page_no = a / (FAU_t)gxdPageSize;
    __ULWORD__ in_page = (__ULWORD__)(a % (FAU_t)gxdPageSize);
    __ULWORD__ n = gxdPageSize - in_page;
    if(n > bytes - done) n = bytes - done;
    auto it = pages.find(page_no);
    if(it == pages.end() && use_map) {
      // Not in a page, so the file holds the current data. Renew the map
      // when the file has grown by an eighth (at least 1 MB) since.
      if((a + (FAU_t)n > map_len) && (a + (FAU_t)n <= disk_size)) {
	FAU_t grown = disk_size - map_len;
	if(!map || grown >= map_len / 8 || grown >= (FAU_t)(1024 * 1024)) Remap();
      }
      if(map && (a + (FAU_t)n <= map_len)) {
	memcpy(out + done, map + a, n);
	done += n;
	continue;
      }
    }
    gxdPage *p = (it != pages.end()) ? it->second : GetPage(page_no);
    if(it != pages.end()) {
      hits++;
      if(!p->dirty && !p->logged && (p != head)) { Unlink(p); LinkFront(p); }
    }
    memcpy(out + done, p->data + in_page, n);
    done += n;
  }
  return done;
}

__ULWORD__ gxdPageCache::Write(const void *buf, __ULWORD__ bytes,
			       FAU_t address)
{
  if(address < (FAU_t)0) return (__ULWORD__)0;
  const unsigned char *in = (const unsigned char *)buf;

  if(!in_txn) {
    // Write through, then update the pages already in the cache
    __ULWORD__ written = gxdFPTRWriteAt(fp, buf, bytes, address);
    __ULWORD__ done = 0;
    while(done < written) {
      FAU_t a = address + done;
      FAU_t page_no = a / (FAU_t)gxdPageSize;
      __ULWORD__ in_page = (__ULWORD__)(a % (FAU_t)gxdPageSize);
      __ULWORD__ n = gxdPageSize - in_page;
      if(n > written - done) n = written - done;
      auto it = pages.find(page_no);
      if(it != pages.end()) memcpy(it->second->data + in_page, in + done, n);
      done += n;
    }
    if(address + (FAU_t)written > file_size) file_size = address + written;
    if(address + (FAU_t)written > disk_size) disk_size = address + written;
    return written;
  }

  __ULWORD__ done = 0;
  while(done < bytes) {
    FAU_t a = address + done;
    FAU_t page_no = a / (FAU_t)gxdPageSize;
    __ULWORD__ in_page = (__ULWORD__)(a % (FAU_t)gxdPageSize);
    __ULWORD__ n = gxdPageSize - in_page;
    if(n > bytes - done) n = bytes - done;
    gxdPage *p = GetPage(page_no);
    if(!p->dirty) {
      if(p->logged) {
	// Keep the committed contents in case the transaction rolls back
	if(!p->orig) p->orig = new unsigned char[gxdPageSize];
	memcpy(p->orig, p->data, gxdPageSize);
      }
      else {
	Unlink(p); // Pinned until the transaction ends
	pinned_count++;
      }
      p->dirty = 1;
      dirty_count++;
      dirty_list.push_back(p);
    }
    memcpy(p->data + in_page, in + done, n);
    done += n;
  }
  if(address + (FAU_t)bytes > file_size) file_size = address + bytes;
  return bytes;
}

void gxdPageCache::BeginTxn()
{
  in_txn = 1;
  begin_size = file_size;
}

void gxdPageCache::GetPages(std::vector<gxdPage *> &v, int logged)
// 10/31/2026: From the dirty or logged list, not a walk of every page
{
  v.clear();
  const std::vector<gxdPage *> &from = logged ? logged_list : dirty_list;
  for(size_t i = 0; i < from.size(); i++) {
    gxdPage *p = from[i];
    if(logged ? (p->logged && !p->dirty) : p->dirty) v.push_back(p);
  }
  std::sort(v.begin(), v.end(),
	    [](const gxdPage *a, const gxdPage *b) {
	      return a->page_no < b->page_no; });
}

void gxdPageCache::GetDirtyPages(std::vector<gxdPage *> &v)
{
  v.clear();
  if(dirty_count == 0) return;
  GetPages(v, 0);
}

void gxdPageCache::CommitTxn()
{
  std::vector<gxdPage *> v;
  GetDirtyPages(v);
  for(size_t i = 0; i < v.size(); i++) {
    gxdPage *p = v[i];
    p->dirty = 0;
    dirty_count--;
    if(!p->logged) {
      p->logged = 1;
      logged_count++;
      logged_list.push_back(p);
    }
    if(p->orig) {
      delete[] p->orig;
      p->orig = 0;
    }
  }
  dirty_list.clear();
  in_txn = 0;
}

void gxdPageCache::RollbackTxn()
{
  std::vector<gxdPage *> v;
  GetDirtyPages(v);
  for(size_t i = 0; i < v.size(); i++) {
    gxdPage *p = v[i];
    dirty_count--;
    if(p->logged) { // Back to the committed contents
      p->dirty = 0;
      if(p->orig) {
	memcpy(p->data, p->orig, gxdPageSize);
	delete[] p->orig;
	p->orig = 0;
      }
    }
    else { // The file still holds the page as it was
      pinned_count--;
      DropPage(p);
    }
  }
  dirty_list.clear();
  file_size = begin_size;
  in_txn = 0;
}

int gxdPageCache::WriteLoggedPages()
// Write the logged pages to the file, joining neighbouring pages into
// one write. The last page of the file is written only up to the end
// of the file. Returns zero if successful or a non-zero value if a
// write failed.
{
  if(logged_count == 0) return 0;
  std::vector<gxdPage *> v;
  GetPages(v, 1);
  const size_t max_run = 256; // Pages per write (1 MB)
  // A buffer sized for this write: a 1 MB buffer for every commit cost
  // more than the commit's few pages
  std::vector<unsigned char> buf((std::min)(v.size(), max_run) * gxdPageSize);
  size_t i = 0;
  while(i < v.size()) {
    size_t j = i + 1;
    while((j < v.size()) && (j - i < max_run) &&
	  (v[j]->page_no == v[j-1]->page_no + 1)) j++;
    FAU_t offset = v[i]->page_no * (FAU_t)gxdPageSize;
    __ULWORD__ len = 0;
    for(size_t k = i; k < j; k++) {
      memcpy(buf.data() + len, v[k]->data, gxdPageSize);
      len += gxdPageSize;
    }
    if(offset >= file_size) len = 0;
    else if(offset + (FAU_t)len > file_size) len = (__ULWORD__)(file_size - offset);
    if(len && (gxdFPTRWriteAt(fp, buf.data(), len, offset) != len)) return 1;
    if(offset + (FAU_t)len > disk_size) disk_size = offset + len;
    for(size_t k = i; k < j; k++) {
      v[k]->logged = 0;
      logged_count--;
      pinned_count--;
      LinkFront(v[k]);
    }
    i = j;
  }
  // Keep only logged pages still dirty (changed again in a transaction)
  std::vector<gxdPage *> keep;
  for(size_t k = 0; k < logged_list.size(); k++)
    if(logged_list[k]->logged) keep.push_back(logged_list[k]);
  logged_list.swap(keep);
  if(pages.size() - pinned_count > max_pages) EvictClean();
  return 0;
}

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
