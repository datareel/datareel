// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxvset.cpp
// Compiler Used: GCC, MSVC
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

Vector index sets (gxvset.h).
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <algorithm>
#include <atomic>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#if !defined (__WIN32__) && !defined (_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif
#include "gxvset.h"
#include "gxcrc32.h"

namespace datareel {

static const char *elem_name(gxvElementType t)
{
  return t == gxvFLOAT16 ? "f16" : t == gxvINT8 ? "i8" : "f32";
}

static const char *metric_name(gxvMetric m)
{
  return m == gxvCOSINE ? "cosine" : m == gxvIP ? "ip" : "l2";
}

static std::string file_id(const std::string &path)
{
  struct stat st;
  if(stat(path.c_str(), &st) != 0) return std::string();
#if defined (__WIN32__) || defined (_WIN32)
  return std::to_string((unsigned long long)st.st_size) + ":" + std::to_string((long long)st.st_mtime);
#else
  return std::to_string((unsigned long long)st.st_size) + ":" + std::to_string((long long)st.st_mtim.tv_sec) + "." +
    std::to_string((long long)st.st_mtim.tv_nsec);
#endif
}

gxVectorSet::gxVectorSet() { }

gxVectorSet::~gxVectorSet() { Close(); }

void gxVectorSet::Close()
{
  std::lock_guard<std::mutex> g(cache_mu);
  cache.clear();
  lru.clear();
  attached.clear();
  files.clear();
  is_open = 0;
}

unsigned gxVectorSet::HeaderCrc(const gxVectorIndex &vix)
{
  // The header fields that never change, in a fixed order
  std::string b;
  char t[160];
  snprintf(t, sizeof(t), "v%u d%u e%u m%u M%u c%u l%u r%u s", gxvVersion, vix.Dim(), (unsigned)vix.ElementType(),
	   (unsigned)vix.Metric(), vix.M(), vix.Codec(), vix.GraphLayout(), vix.FeaturesRequired());
  b = t;
  b += vix.Spec();
  return (unsigned)calcCRC32(b.data(), (unsigned)b.size());
}

int gxVectorSet::SafeRelPath(const std::string &rel)
{
  if(rel.empty() || rel[0] == '/' || rel.find('\\') != std::string::npos || rel.find(':') != std::string::npos) return 0;
  for(unsigned char c : rel) if(c < 0x20 || c == 0x7f) return 0;
  size_t a = 0;
  while(a <= rel.size()) {
    size_t b = rel.find('/', a);
    if(b == std::string::npos) b = rel.size();
    std::string part = rel.substr(a, b - a);
    if(part.empty() || part == "." || part == "..") return 0;
    a = b + 1;
  }
  return 1;
}

std::string gxVectorSet::FullPath(unsigned file) const
{
  if(file >= files.size()) return std::string();
  return dirpath.empty() ? files[file].path : dirpath + "/" + files[file].path;
}

gxvError gxVectorSet::Create(const char *path, const char *nm, unsigned d, gxvElementType type, gxvMetric met,
			     const char *s)
{
  Close();
  error.clear();
  if(!path || !nm || !*nm || d == 0 || d > gxvMaxDim) return gxv_BAD_ARG;
  if(strchr(nm, '\n') || (s && (strchr(s, '\n') || strlen(s) >= gxvSpecSize))) return gxv_BAD_ARG;
  if(type != gxvFLOAT32 && type != gxvFLOAT16 && type != gxvINT8) return gxv_BAD_ARG;
  if(met != gxvL2 && met != gxvCOSINE && met != gxvIP) return gxv_BAD_ARG;
  mpath = path;
  size_t k = mpath.find_last_of('/');
  dirpath = k == std::string::npos ? std::string() : mpath.substr(0, k);
  name = nm;
  spec = s ? s : "";
  dim = d; elem_type = type; metric = met; codec = 0;
  max_open = 16;
  is_open = 1;
  return Write();
}

gxvError gxVectorSet::Parse(const std::string &text)
{
  std::istringstream in(text);
  std::string line;
  int have_set = 0, have_version = 0, have_spec = 0, have_dim = 0, have_elem = 0, have_metric = 0, ln = 0;
  files.clear();
  while(std::getline(in, line)) {
    ln++;
    if(!line.empty() && line.back() == '\r') line.pop_back();
    if(line.empty() || line[0] == '#') continue;
    size_t sp = line.find(' ');
    std::string key = line.substr(0, sp), rest = sp == std::string::npos ? std::string() : line.substr(sp + 1);
    auto bad = [&](const std::string &why) { error = mpath + ":" + std::to_string(ln) + ": " + why; return gxv_BAD_FILE; };
    if(key == "set") { name = rest; have_set = 1; }
    else if(key == "version") {
      if(rest != "1") return rest.empty() ? bad("no version") : gxv_BAD_VERSION;
      have_version = 1;
    }
    else if(key == "spec") { spec = rest; have_spec = 1; if(spec.size() >= gxvSpecSize) return bad("spec too long"); }
    else if(key == "dim") { dim = (unsigned)strtoul(rest.c_str(), 0, 10); have_dim = 1; if(dim == 0 || dim > gxvMaxDim) return bad("bad dim"); }
    else if(key == "element") {
      if(rest == "f32") elem_type = gxvFLOAT32; else if(rest == "f16") elem_type = gxvFLOAT16;
      else if(rest == "i8") elem_type = gxvINT8; else { error = mpath + ": element " + rest; return gxv_UNSUPPORTED; }
      have_elem = 1;
    }
    else if(key == "metric") {
      if(rest == "l2") metric = gxvL2; else if(rest == "cosine") metric = gxvCOSINE;
      else if(rest == "ip") metric = gxvIP; else return bad("bad metric " + rest);
      have_metric = 1;
    }
    else if(key == "codec") {
      codec = (unsigned)strtoul(rest.c_str(), 0, 10);
      if(codec != gxvCodecPlain) { error = mpath + ": codec " + rest; return gxv_UNSUPPORTED; }
    }
    else if(key == "file") {
      size_t at = rest.rfind(" nodes=");
      if(at == std::string::npos) return bad("a file line without nodes=");
      gxvSetFile f;
      f.path = rest.substr(0, at);
      if(!SafeRelPath(f.path)) return bad("a path that is not relative to the set: " + f.path);
      std::istringstream fs(rest.substr(at + 1));
      std::string kv;
      int got = 0;
      while(fs >> kv) {
	size_t eq = kv.find('=');
	if(eq == std::string::npos) return bad("bad field " + kv);
	std::string k2 = kv.substr(0, eq), v = kv.substr(eq + 1);
	if(k2 == "nodes") { f.nodes = strtoull(v.c_str(), 0, 10); got |= 1; }
	else if(k2 == "deleted") { f.deleted = strtoull(v.c_str(), 0, 10); got |= 2; }
	else if(k2 == "tag2") {
	  if(v == "none") f.has_tag2 = 0;
	  else {
	    size_t dd = v.find("..");
	    if(dd == std::string::npos) return bad("bad tag2 " + v);
	    f.tag2_min = (unsigned)strtoul(v.substr(0, dd).c_str(), 0, 10);
	    f.tag2_max = (unsigned)strtoul(v.substr(dd + 2).c_str(), 0, 10);
	    if(f.tag2_min > f.tag2_max) return bad("bad tag2 " + v);
	    f.has_tag2 = 1;
	  }
	  got |= 4;
	}
	else if(k2 == "crc") { f.crc = (unsigned)strtoul(v.c_str(), 0, 16); got |= 8; }
	else return bad("unknown field " + k2);
      }
      if(got != 15) return bad("a file line needs nodes, deleted, tag2 and crc");
      for(auto &o : files) if(o.path == f.path) return bad("listed twice: " + f.path);
      files.push_back(f);
    }
    else return bad("unknown line " + key);
  }
  if(!have_set || !have_version || !have_spec || !have_dim || !have_elem || !have_metric) {
    error = mpath + ": not a whole manifest (set, version, spec, dim, element and metric are needed)";
    return gxv_BAD_FILE;
  }
  return gxv_NO_ERROR;
}

gxvError gxVectorSet::Open(const char *path, const char *s, unsigned mo)
{
  Close();
  error.clear();
  if(!path) return gxv_BAD_ARG;
  mpath = path;
  size_t k = mpath.find_last_of('/');
  dirpath = k == std::string::npos ? std::string() : mpath.substr(0, k);
  std::ifstream in(mpath, std::ios::binary);
  if(!in) { error = "cannot open " + mpath; return gxv_NOT_OPEN; }
  std::stringstream ss;
  ss << in.rdbuf();
  gxvError err = Parse(ss.str());
  if(err != gxv_NO_ERROR) { files.clear(); return err; }
  if(s && spec != s) { files.clear(); return gxv_SPEC_MISMATCH; }
  max_open = mo ? mo : 1;
  is_open = 1;
  return gxv_NO_ERROR;
}

gxvError gxVectorSet::Write()
{
  std::string t = "# gxVectorSet manifest (DataReel 7.4, gxvset.h)\n";
  t += "set " + name + "\n";
  t += "version " + std::to_string(gxvSetVersion) + "\n";
  t += "spec " + spec + "\n";
  t += "dim " + std::to_string(dim) + "\n";
  t += std::string("element ") + elem_name(elem_type) + "\n";
  t += std::string("metric ") + metric_name(metric) + "\n";
  t += "codec " + std::to_string(codec) + "\n";
  char b[160];
  for(auto &f : files) {
    std::string tg = f.has_tag2 ? std::to_string(f.tag2_min) + ".." + std::to_string(f.tag2_max) : std::string("none");
    snprintf(b, sizeof(b), " nodes=%llu deleted=%llu tag2=%s crc=%08x\n", f.nodes, f.deleted, tg.c_str(), f.crc);
    t += "file " + f.path + b;
  }
  std::string tmp = mpath + ".tmp";
  FILE *fp = fopen(tmp.c_str(), "wb");
  if(!fp) { error = "cannot write " + tmp; return gxv_DB_ERROR; }
  int ok = fwrite(t.data(), 1, t.size(), fp) == t.size();
  if(fflush(fp) != 0) ok = 0;
#if !defined (__WIN32__) && !defined (_WIN32)
  if(ok && fsync(fileno(fp)) != 0) ok = 0;
#endif
  fclose(fp);
  if(!ok) { remove(tmp.c_str()); error = "cannot write " + tmp; return gxv_DB_ERROR; }
#if defined (__WIN32__) || defined (_WIN32)
  remove(mpath.c_str());
#endif
  if(rename(tmp.c_str(), mpath.c_str()) != 0) { remove(tmp.c_str()); error = "cannot rename " + tmp; return gxv_DB_ERROR; }
#if !defined (__WIN32__) && !defined (_WIN32)
  int dfd = open(dirpath.empty() ? "." : dirpath.c_str(), O_RDONLY);
  if(dfd >= 0) { fsync(dfd); close(dfd); }
#endif
  return gxv_NO_ERROR;
}

gxvError gxVectorSet::Check(const gxVectorIndex &vix, const std::string &rel) const
{
  if(vix.Dim() != dim || vix.ElementType() != elem_type || vix.Metric() != metric) {
    error = rel + ": dimension, element type or metric differs from the set";
    return gxv_BAD_ARG;
  }
  if(spec != vix.Spec()) { error = rel + ": spec differs from the set"; return gxv_SPEC_MISMATCH; }
  return gxv_NO_ERROR;
}

static void fill_from(gxvSetFile &f, const gxVectorIndex &vix)
{
  f.nodes = vix.Count() + vix.Deleted();
  f.deleted = vix.Deleted();
  unsigned lo, hi;
  f.has_tag2 = vix.TagRange(2, lo, hi);
  f.tag2_min = f.has_tag2 ? lo : 0;
  f.tag2_max = f.has_tag2 ? hi : 0;
  if(!f.has_tag2 && (vix.FeaturesOptional() & gxvOptTagRanges) && vix.Count() + vix.Deleted() == 0) {
    // an empty file with ranges kept: nothing to find (min > max): listed as an empty range
    f.has_tag2 = 1; f.tag2_min = 0xFFFFFFFFu; f.tag2_max = 0xFFFFFFFFu;
  }
}

gxvError gxVectorSet::AddFile(const char *rel)
{
  if(!is_open) return gxv_NOT_OPEN;
  if(!rel || !SafeRelPath(rel)) { error = std::string("not a relative path: ") + (rel ? rel : ""); return gxv_BAD_ARG; }
  for(auto &f : files) if(f.path == rel) return gxv_DUPLICATE_ID;
  gxvSetFile f;
  f.path = rel;
  std::string full = dirpath.empty() ? f.path : dirpath + "/" + f.path;
  gxVectorIndex vix;
  gxvError err = vix.Open(full.c_str(), gxDBASE_READONLY);
  if(err != gxv_NO_ERROR) { error = f.path + ": " + gxVectorIndex::ErrorMessage(err); return err; }
  err = Check(vix, f.path);
  if(err != gxv_NO_ERROR) return err;
  f.crc = HeaderCrc(vix);
  fill_from(f, vix);
  vix.Close();
  files.push_back(f);
  err = Write();
  if(err != gxv_NO_ERROR) files.pop_back();
  return err;
}

gxvError gxVectorSet::RemoveFile(const char *rel)
{
  if(!is_open) return gxv_NOT_OPEN;
  if(!rel) return gxv_BAD_ARG;
  for(size_t i = 0; i < files.size(); i++) {
    if(files[i].path != rel) continue;
    {
      std::lock_guard<std::mutex> g(cache_mu);
      // file numbers after it move down: the open files and attachments are dropped
      cache.clear();
      lru.clear();
      attached.clear();
    }
    files.erase(files.begin() + (long)i);
    return Write();
  }
  return gxv_ID_NOT_FOUND;
}

gxvError gxVectorSet::Refresh()
{
  if(!is_open) return gxv_NOT_OPEN;
  for(size_t i = 0; i < files.size(); i++) {
    gxvSetFile &f = files[i];
    gxVectorIndex own;
    gxVectorIndex *v = 0;
    auto at = attached.find((unsigned)i);
    if(at != attached.end()) v = at->second;
    else {
      gxvError err = own.Open(FullPath((unsigned)i).c_str(), gxDBASE_READONLY);
      if(err != gxv_NO_ERROR) { error = f.path + ": " + gxVectorIndex::ErrorMessage(err); return err; }
      v = &own;
    }
    if(HeaderCrc(*v) != f.crc) { error = f.path + ": the file no longer matches its crc"; return gxv_SPEC_MISMATCH; }
    fill_from(f, *v);
  }
  return Write();
}

gxvError gxVectorSet::AttachOpen(const char *rel, gxVectorIndex *vix)
{
  if(!is_open) return gxv_NOT_OPEN;
  if(!rel) return gxv_BAD_ARG;
  for(size_t i = 0; i < files.size(); i++) {
    if(files[i].path != rel) continue;
    std::lock_guard<std::mutex> g(cache_mu);
    auto c = cache.find((unsigned)i);
    if(c != cache.end()) { lru.erase(c->second.pos); cache.erase(c); }
    if(!vix) { attached.erase((unsigned)i); return gxv_NO_ERROR; }
    if(!vix->IsOpen()) return gxv_NOT_OPEN;
    gxvError err = Check(*vix, files[i].path);
    if(err != gxv_NO_ERROR) return err;
    if(HeaderCrc(*vix) != files[i].crc) { error = files[i].path + ": does not match its crc"; return gxv_SPEC_MISMATCH; }
    attached[(unsigned)i] = vix;
    return gxv_NO_ERROR;
  }
  return gxv_ID_NOT_FOUND;
}

std::shared_ptr<gxVectorIndex> gxVectorSet::Get(unsigned file, gxvError &err, int &is_missing) const
{
  err = gxv_NO_ERROR;
  is_missing = 0;
  std::string full = FullPath(file);
  std::shared_ptr<gxVectorIndex> vix;
  std::shared_ptr<std::mutex> rmu;
  std::string had;
  {
    std::lock_guard<std::mutex> g(cache_mu);
    auto at = attached.find(file);
    if(at != attached.end())   // the writer's own index: as it is, never owned here
      return std::shared_ptr<gxVectorIndex>(at->second, [](gxVectorIndex *) { });
    auto c = cache.find(file);
    if(c != cache.end()) {
      lru.erase(c->second.pos);
      lru.push_front(file);
      c->second.pos = lru.begin();
      vix = c->second.vix;
      rmu = c->second.reload_mu;
      had = c->second.id;
    }
  }
  if(vix) {
    // the live file: changed since it was read? (size and modification time)
    std::string now = file_id(full);
    if(now.empty()) { is_missing = 1; err = gxv_NOT_OPEN; return 0; }
    if(now == had) return vix;
    std::lock_guard<std::mutex> rg(*rmu);
    {
      std::lock_guard<std::mutex> g(cache_mu);
      auto c = cache.find(file);
      if(c != cache.end() && c->second.id == now) return vix;   // another search read it again first
    }
    // (a writer in the middle of a change can leave the file unreadable for a moment: tried again)
    for(int tries = 0; tries < 5; tries++) {
      err = vix->Reload();
      if(err == gxv_NO_ERROR) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      now = file_id(full);
    }
    if(err != gxv_NO_ERROR) { error = files[file].path + ": " + gxVectorIndex::ErrorMessage(err); return 0; }
    std::lock_guard<std::mutex> g(cache_mu);
    auto c = cache.find(file);
    if(c != cache.end()) c->second.id = now;
    reloaded++;
    return vix;
  }
  std::string id0 = file_id(full);
  if(id0.empty()) { is_missing = 1; err = gxv_NOT_OPEN; return 0; }
  std::shared_ptr<gxVectorIndex> nv(new gxVectorIndex);
  err = nv->Open(full.c_str(), gxDBASE_READONLY);
  if(err != gxv_NO_ERROR) { error = files[file].path + ": " + gxVectorIndex::ErrorMessage(err); return 0; }
  err = Check(*nv, files[file].path);
  if(err == gxv_NO_ERROR && HeaderCrc(*nv) != files[file].crc) {
    error = files[file].path + ": the file does not match its crc (replaced?)";
    err = gxv_SPEC_MISMATCH;
  }
  if(err != gxv_NO_ERROR) return 0;
  std::lock_guard<std::mutex> g(cache_mu);
  auto c = cache.find(file);
  if(c != cache.end()) return c->second.vix;     // opened by another search meanwhile
  opened++;
  lru.push_front(file);
  Entry e;
  e.vix = nv;
  e.pos = lru.begin();
  e.id = id0;
  e.reload_mu = std::make_shared<std::mutex>();
  cache[file] = e;
  while(cache.size() > max_open && !lru.empty()) {    // a search holding one keeps it alive
    unsigned old = lru.back();
    lru.pop_back();
    cache.erase(old);
  }
  return nv;
}

gxvError gxVectorSet::Search(const float *query, const gxvSetSearch &opt, std::vector<gxvSetResult> &results) const
{
  results.clear();
  if(!is_open) return gxv_NOT_OPEN;
  if(!query || opt.k == 0) return gxv_BAD_ARG;
  {
    std::lock_guard<std::mutex> g(cache_mu);
    skipped = 0; missing = 0; reloaded = 0;
    error.clear();
  }
  gxvFilter f;
  if(opt.filter) f = *opt.filter;
  if(opt.day_range) {
    unsigned lo = opt.day_lo, hi = opt.day_hi;
    if(f.tag_mask & 4u) { lo = std::max(lo, f.lo[2]); hi = std::min(hi, f.hi[2]); }
    if(lo > hi) return gxv_NO_ERROR;           // nothing can match
    f.Range(2, lo, hi);
  }
  // The files to search: a file whose manifest tag 2 range is outside the
  // day range is skipped unopened
  std::vector<unsigned> todo;
  unsigned long long skip = 0;
  for(unsigned i = 0; i < files.size(); i++) {
    const gxvSetFile &sf = files[i];
    if((f.tag_mask & 4u) && sf.has_tag2 && (sf.tag2_max < f.lo[2] || sf.tag2_min > f.hi[2])) { skip++; continue; }
    todo.push_back(i);
  }
  std::vector<std::vector<gxvSetResult> > per(todo.size());
  std::vector<gxvError> errs(todo.size(), gxv_NO_ERROR);
  std::vector<int> miss(todo.size(), 0);
  std::atomic<size_t> next(0);
  auto work = [&]() {
    for(size_t j; (j = next.fetch_add(1)) < todo.size(); ) {
      unsigned fi = todo[j];
      int is_missing = 0;
      gxvError e;
      std::shared_ptr<gxVectorIndex> vix = Get(fi, e, is_missing);
      if(!vix) { errs[j] = e; miss[j] = is_missing; continue; }
      // a file whose own ranges rule out the filter is skipped once open
      int out = 0;
      for(unsigned t = 0; t < gxvTagCount; t++)
	if((f.tag_mask & (1u << t)) && vix->TagRangeExcludes(t, f.lo[t], f.hi[t])) out = 1;
      if(out) continue;
      std::vector<gxvResult> r;
      e = vix->Search(query, opt.k, r, opt.ef, (f.tag_mask || f.accept) ? &f : 0);
      if(e != gxv_NO_ERROR) { errs[j] = e; continue; }
      per[j].reserve(r.size());
      for(auto &h : r) {
	gxvSetResult s;
	s.id = h.id; s.distance = h.distance; s.file = fi;
	memcpy(s.tags, h.tags, sizeof(s.tags));
	per[j].push_back(s);
      }
    }
  };
  unsigned nt = opt.threads ? opt.threads : 1;
  if(nt > todo.size()) nt = (unsigned)todo.size();
  if(nt <= 1) work();
  else {
    std::vector<std::thread> th;
    for(unsigned t = 0; t < nt; t++) th.emplace_back(work);
    for(auto &t : th) t.join();
  }
  unsigned long long nmiss = 0;
  gxvError first = gxv_NO_ERROR;
  for(size_t j = 0; j < todo.size(); j++) {
    if(miss[j]) {
      nmiss++;
      if(opt.strict && first == gxv_NO_ERROR) { first = gxv_NOT_OPEN; error = files[todo[j]].path + ": missing"; }
      continue;
    }
    if(errs[j] != gxv_NO_ERROR && first == gxv_NO_ERROR) first = errs[j];
  }
  {
    std::lock_guard<std::mutex> g(cache_mu);
    skipped = skip;
    missing = nmiss;
  }
  if(first != gxv_NO_ERROR) return first;
  for(auto &p : per) results.insert(results.end(), p.begin(), p.end());
  std::sort(results.begin(), results.end(), [](const gxvSetResult &a, const gxvSetResult &b) {
      if(a.distance != b.distance) return a.distance < b.distance;
      if(a.file != b.file) return a.file < b.file;
      return a.id < b.id;
    });
  if(results.size() > opt.k) results.resize(opt.k);
  return gxv_NO_ERROR;
}

} // namespace datareel
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
