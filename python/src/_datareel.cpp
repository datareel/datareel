// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: _datareel.cpp
// Compiler Used: GCC, MSVC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/20/2026
// Date Last Modified: 11/20/2026
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

DataReel 7.5 beta: the "datareel" Python module (the _datareel
extension; datareel/__init__.py is the public face). Plain CPython C
API, no third-party packages. Read only in this beta:

  Reader       gxObjReplReader: a gxObjRepl node folder, read while the
               node runs in another process (whole commits only)
  VectorIndex  gxVectorIndex opened read only: a .vix file no process
               is writing (a day file, a merged month)
  BlockFile    gxDatabase opened read only: walk a .gxd file's objects
               and read them by address (a file no process is writing)

Every call that reads files releases the GIL. Objects are not thread
safe: use one per thread.
*/
// ----------------------------------------------------------- //
#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <string>
#include <vector>
#include <string.h>
#include <algorithm>
#include "gxdlcode.h"
#include "gxobjrepl.h"
#include "gxvindex.h"
#include "gxdbase.h"
#include "gxbtree.h"
#include "gxdjrnl.h"
#include "gxderror.h"
#include "gxstream.h"
#include "gxdbmux.h"
#include "gxdbobject.h"

using namespace datareel;

static PyObject *DataReelError;   // datareel.Error

static PyObject *raise(const std::string &what)
{
  PyErr_SetString(DataReelError, what.c_str());
  return 0;
}

// a query vector from any object with the buffer protocol holding float32 (array('f'), numpy float32) or any
// sequence of numbers. Returns false with a Python error set
static bool floats(PyObject *o, std::vector<float> &v)
{
  v.clear();
  if(PyObject_CheckBuffer(o)) {
    Py_buffer b;
    if(PyObject_GetBuffer(o, &b, PyBUF_FORMAT | PyBUF_C_CONTIGUOUS) == 0) {
      bool f32 = b.itemsize == 4 && b.format && (strcmp(b.format, "f") == 0 || strcmp(b.format, "<f") == 0 || strcmp(b.format, "=f") == 0);
      if(f32) { v.resize((size_t)(b.len / 4)); memcpy(v.data(), b.buf, (size_t)b.len); }
      PyBuffer_Release(&b);
      if(f32) return true;
    }
    else PyErr_Clear();
  }
  PyObject *seq = PySequence_Fast(o, "a vector must be a sequence of numbers or a float32 buffer");
  if(!seq) return false;
  Py_ssize_t n = PySequence_Fast_GET_SIZE(seq);
  v.resize((size_t)n);
  for(Py_ssize_t i = 0; i < n; i++) {
    double d = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(seq, i));
    if(d == -1.0 && PyErr_Occurred()) { Py_DECREF(seq); return false; }
    v[(size_t)i] = (float)d;
  }
  Py_DECREF(seq);
  return true;
}

static PyObject *id_list(const std::vector<uint64_t> &ids)
{
  PyObject *l = PyList_New((Py_ssize_t)ids.size());
  if(!l) return 0;
  for(size_t i = 0; i < ids.size(); i++) {
    PyObject *x = PyLong_FromUnsignedLongLong(ids[i]);
    if(!x) { Py_DECREF(l); return 0; }
    PyList_SET_ITEM(l, (Py_ssize_t)i, x);
  }
  return l;
}

// ---------- Reader ----------
struct ReaderObject {
  PyObject_HEAD
  gxObjReplReader *r;
};

static void Reader_dealloc(ReaderObject *self)
{
  delete self->r;
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Reader_new(PyTypeObject *type, PyObject *, PyObject *)
{
  ReaderObject *self = (ReaderObject *)type->tp_alloc(type, 0);
  if(self) self->r = new gxObjReplReader;
  return (PyObject *)self;
}

static int Reader_init(ReaderObject *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"root", "index", 0};
  PyObject *root_o = 0; const char *index = "";
  if(!PyArg_ParseTupleAndKeywords(args, kw, "O&|s", (char **)kwlist, PyUnicode_FSConverter, &root_o, &index)) return -1;
  std::string root(PyBytes_AS_STRING(root_o)), ix(index);
  Py_DECREF(root_o);
  int rc;
  Py_BEGIN_ALLOW_THREADS
  rc = self->r->Open(root, ix);
  Py_END_ALLOW_THREADS
  if(rc != 0) { raise(self->r->Error()); return -1; }
  return 0;
}

#define READER_OPEN(self) if(!(self)->r->IsOpen()) return raise("the reader is closed")

static PyObject *Reader_get(ReaderObject *self, PyObject *args)
{
  const char *db; unsigned long long id;
  if(!PyArg_ParseTuple(args, "sK", &db, &id)) return 0;
  READER_OPEN(self);
  std::vector<unsigned char> b; int rc; std::string d(db);
  Py_BEGIN_ALLOW_THREADS
  rc = self->r->Get(d, id, b);
  Py_END_ALLOW_THREADS
  if(rc != 0) {
    std::string e = self->r->Error();
    if(e.empty()) Py_RETURN_NONE;                              // not live
    return raise(e);
  }
  return PyBytes_FromStringAndSize((const char *)b.data(), (Py_ssize_t)b.size());
}

static PyObject *Reader_is_live(ReaderObject *self, PyObject *args)
{
  const char *db; unsigned long long id;
  if(!PyArg_ParseTuple(args, "sK", &db, &id)) return 0;
  READER_OPEN(self);
  int rc; std::string d(db);
  Py_BEGIN_ALLOW_THREADS
  rc = self->r->IsLive(d, id);
  Py_END_ALLOW_THREADS
  if(rc < 0) return raise(self->r->Error());
  return PyBool_FromLong(rc);
}

static PyObject *Reader_ids(ReaderObject *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"db", "tombstones", "after", "limit", 0};
  const char *db; int tombs = 0; unsigned long long after = 0, limit = 0;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "s|pKK", (char **)kwlist, &db, &tombs, &after, &limit)) return 0;
  READER_OPEN(self);
  std::vector<uint64_t> ids; int rc; std::string d(db);
  gxObjRepl::Which w = tombs ? gxObjRepl::TOMBSTONES : gxObjRepl::LIVE;
  Py_BEGIN_ALLOW_THREADS
  // 7.5 beta 2: AFTER or LIMIT: one page from the id index (a short lock); otherwise every id
  rc = (after || limit) ? self->r->IdsAfter(d, after, (size_t)limit, ids, w) : self->r->Ids(d, ids, w);
  Py_END_ALLOW_THREADS
  if(rc != 0) return raise(self->r->Error());
  return id_list(ids);
}

static PyObject *Reader_databases(ReaderObject *self, PyObject *)
{
  READER_OPEN(self);
  std::vector<std::string> v; int rc;
  Py_BEGIN_ALLOW_THREADS
  rc = self->r->Databases(v);
  Py_END_ALLOW_THREADS
  if(rc != 0) return raise(self->r->Error());
  PyObject *l = PyList_New((Py_ssize_t)v.size());
  if(!l) return 0;
  for(size_t i = 0; i < v.size(); i++) {
    PyObject *s = PyUnicode_FromString(v[i].c_str());
    if(!s) { Py_DECREF(l); return 0; }
    PyList_SET_ITEM(l, (Py_ssize_t)i, s);
  }
  return l;
}

static PyObject *Reader_find_key(ReaderObject *self, PyObject *args)
{
  const char *index; Py_buffer key;
  if(!PyArg_ParseTuple(args, "sy*", &index, &key)) return 0;
  READER_OPEN(self);
  if(key.len > 32) { PyBuffer_Release(&key); return raise("a key is at most 32 bytes"); }
  std::vector<unsigned char> k((const unsigned char *)key.buf, (const unsigned char *)key.buf + key.len);
  PyBuffer_Release(&key);
  std::vector<uint64_t> ids; int rc; std::string ix(index);
  Py_BEGIN_ALLOW_THREADS
  rc = self->r->FindKey(ix, k.data(), (unsigned)k.size(), ids);
  Py_END_ALLOW_THREADS
  if(rc != 0) return raise(self->r->Error());
  return id_list(ids);
}

// find_range(index, lo, hi=None, limit=0) -> ids with lo <= key < hi, in key order
static PyObject *Reader_find_range(ReaderObject *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"index", "lo", "hi", "limit", 0};
  const char *index; Py_buffer lo; PyObject *hi_o = Py_None; unsigned long long limit = 0;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "sy*|OK", (char **)kwlist, &index, &lo, &hi_o, &limit)) return 0;
  std::vector<unsigned char> l((const unsigned char *)lo.buf, (const unsigned char *)lo.buf + lo.len), h;
  PyBuffer_Release(&lo);
  if(hi_o != Py_None) { char *p; Py_ssize_t n; if(PyBytes_AsStringAndSize(hi_o, &p, &n) < 0) return 0; h.assign(p, p + n); }
  READER_OPEN(self);
  std::vector<uint64_t> ids; int rc; std::string ix(index);
  Py_BEGIN_ALLOW_THREADS
  rc = self->r->FindRange(ix, l.data(), (unsigned)l.size(), h.data(), (unsigned)h.size(), ids, (size_t)limit);
  Py_END_ALLOW_THREADS
  if(rc != 0) return raise(self->r->Error());
  return id_list(ids);
}

static PyObject *Reader_nearest(ReaderObject *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"index", "query", "k", 0};
  const char *index; PyObject *q; unsigned k = 10;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "sO|I", (char **)kwlist, &index, &q, &k)) return 0;
  READER_OPEN(self);
  std::vector<float> v;
  if(!floats(q, v)) return 0;
  if(v.empty()) return raise("an empty query vector");
  std::vector<gxObjReplHit> hits; int rc; std::string ix(index);
  Py_BEGIN_ALLOW_THREADS
  rc = self->r->Nearest(ix, v.data(), k, hits);
  Py_END_ALLOW_THREADS
  if(rc != 0) return raise(self->r->Error());
  PyObject *l = PyList_New((Py_ssize_t)hits.size());
  if(!l) return 0;
  for(size_t i = 0; i < hits.size(); i++) {
    PyObject *t = Py_BuildValue("(Kd)", (unsigned long long)hits[i].id, (double)hits[i].distance);
    if(!t) { Py_DECREF(l); return 0; }
    PyList_SET_ITEM(l, (Py_ssize_t)i, t);
  }
  return l;
}

static PyObject *Reader_refresh(ReaderObject *self, PyObject *)
{
  READER_OPEN(self);
  int rc;
  Py_BEGIN_ALLOW_THREADS
  rc = self->r->Refresh();
  Py_END_ALLOW_THREADS
  if(rc != 0) return raise(self->r->Error());
  Py_RETURN_NONE;
}

static PyObject *Reader_release_vectors(ReaderObject *self, PyObject *) { self->r->ReleaseVectors(); Py_RETURN_NONE; }
static PyObject *Reader_close(ReaderObject *self, PyObject *) { self->r->Close(); Py_RETURN_NONE; }
static PyObject *Reader_enter(ReaderObject *self, PyObject *) { Py_INCREF(self); return (PyObject *)self; }
static PyObject *Reader_exit(ReaderObject *self, PyObject *) { self->r->Close(); Py_RETURN_FALSE; }

static PyObject *Reader_seq(ReaderObject *self, void *) { return PyLong_FromUnsignedLongLong(self->r->Seq()); }
static PyObject *Reader_epoch(ReaderObject *self, void *) { return PyLong_FromUnsignedLongLong(self->r->Epoch()); }
static PyObject *Reader_role(ReaderObject *self, void *)
{
  int r = self->r->Role();
  return PyUnicode_FromString(r == gxObjRepl::PRIMARY ? "primary" : r == gxObjRepl::COPY ? "copy" : "none");
}
static PyObject *Reader_is_open(ReaderObject *self, void *) { return PyBool_FromLong(self->r->IsOpen()); }
static PyObject *Reader_index_spec(ReaderObject *self, void *) { return PyUnicode_FromString(self->r->IndexSpec().c_str()); }

static PyMethodDef Reader_methods[] = {
  {"get", (PyCFunction)Reader_get, METH_VARARGS, "get(db, id) -> bytes, or None when the object is not live"},
  {"is_live", (PyCFunction)Reader_is_live, METH_VARARGS, "is_live(db, id) -> bool"},
  {"ids", (PyCFunction)(void (*)(void))Reader_ids, METH_VARARGS | METH_KEYWORDS, "ids(db, tombstones=False, after=0, limit=0) -> sorted ids; with AFTER or LIMIT one page (a short read)"},
  {"databases", (PyCFunction)Reader_databases, METH_NOARGS, "databases() -> list of database names"},
  {"find_key", (PyCFunction)Reader_find_key, METH_VARARGS, "find_key(index, key: bytes) -> ids whose B-tree key starts with key"},
  {"find_range", (PyCFunction)(void (*)(void))Reader_find_range, METH_VARARGS | METH_KEYWORDS, "find_range(index, lo, hi=None, limit=0) -> ids with lo <= key < hi, in key order"},
  {"nearest", (PyCFunction)(void (*)(void))Reader_nearest, METH_VARARGS | METH_KEYWORDS, "nearest(index, query, k=10) -> [(id, distance)], closest first"},
  {"refresh", (PyCFunction)Reader_refresh, METH_NOARGS, "refresh(): read the node's state now"},
  {"release_vectors", (PyCFunction)Reader_release_vectors, METH_NOARGS, "release_vectors(): close the vector indexes nearest() loaded"},
  {"close", (PyCFunction)Reader_close, METH_NOARGS, "close()"},
  {"__enter__", (PyCFunction)Reader_enter, METH_NOARGS, ""},
  {"__exit__", (PyCFunction)Reader_exit, METH_VARARGS, ""},
  {0, 0, 0, 0}
};

static PyGetSetDef Reader_getset[] = {
  {"seq", (getter)Reader_seq, 0, "the commit the last call read (a primary's last commit, a copy's applied one)", 0},
  {"epoch", (getter)Reader_epoch, 0, "the node's epoch as of the last call", 0},
  {"role", (getter)Reader_role, 0, "'primary', 'copy' or 'none'", 0},
  {"is_open", (getter)Reader_is_open, 0, "whether the reader is open", 0},
  {"index_spec", (getter)Reader_index_spec, 0, "the index spec in use (read from the node's .def files when none was given)", 0},
  {0, 0, 0, 0, 0}
};

static PyTypeObject ReaderType = { PyVarObject_HEAD_INIT(0, 0) };

// ---------- VectorIndex ----------
struct VixObject {
  PyObject_HEAD
  gxVectorIndex *vx;
  int attached;                    // 7.5 beta 2: attached to an open Journal
  int writable;
};

static void Vix_dealloc(VixObject *self)
{
  if(self->vx) { self->vx->Close(); delete self->vx; }
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Vix_new(PyTypeObject *type, PyObject *, PyObject *)
{
  VixObject *self = (VixObject *)type->tp_alloc(type, 0);
  if(self) self->vx = new gxVectorIndex;
  return (PyObject *)self;
}

static int Vix_init(VixObject *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"path", "spec", "write", 0};
  PyObject *p = 0; const char *spec = 0; int write = 0;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "O&|zp", (char **)kwlist, PyUnicode_FSConverter, &p, &spec, &write)) return -1;
  std::string path(PyBytes_AS_STRING(p)), sp(spec ? spec : "");
  Py_DECREF(p);
  self->writable = write;
  gxvError e;
  Py_BEGIN_ALLOW_THREADS
  e = self->vx->Open(path.c_str(), write ? gxDBASE_READWRITE : gxDBASE_READONLY, spec ? sp.c_str() : 0);
  Py_END_ALLOW_THREADS
  if(e != gxv_NO_ERROR) {
    raise("cannot open " + path + (e == gxv_SPEC_MISMATCH ? ": the spec differs" : "") + " (gxvError " + std::to_string((int)e) + ")");
    return -1;
  }
  return 0;
}

#define VIX_OPEN(self) if(!(self)->vx->IsOpen()) return raise("the vector index is closed")

static PyObject *Vix_search(VixObject *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"query", "k", "ef", "exact", "tags", 0};
  PyObject *q, *tags = 0; unsigned k = 10, ef = 64; int exact = 0;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "O|IIpO", (char **)kwlist, &q, &k, &ef, &exact, &tags)) return 0;
  VIX_OPEN(self);
  std::vector<float> v;
  if(!floats(q, v)) return 0;
  if(v.size() != self->vx->Dim()) return raise("the query has " + std::to_string(v.size()) + " values, the index " + std::to_string(self->vx->Dim()));
  // tags: {i: value} or {i: (lo, hi)}, i in 0..3
  gxvFilter f; bool use_f = false;
  if(tags && tags != Py_None) {
    if(!PyDict_Check(tags)) return raise("tags is a dict {tag: value or (lo, hi)}");
    PyObject *key, *val; Py_ssize_t pos = 0;
    while(PyDict_Next(tags, &pos, &key, &val)) {
      long i = PyLong_AsLong(key);
      if(i < 0 || i >= (long)gxvTagCount) { if(!PyErr_Occurred()) raise("a tag number is 0 to 3"); return 0; }
      unsigned long lo, hi;
      if(PyTuple_Check(val) && PyTuple_GET_SIZE(val) == 2) {
	lo = PyLong_AsUnsignedLong(PyTuple_GET_ITEM(val, 0)); hi = PyLong_AsUnsignedLong(PyTuple_GET_ITEM(val, 1));
      } else lo = hi = PyLong_AsUnsignedLong(val);
      if(PyErr_Occurred()) return 0;
      f.Range((unsigned)i, (unsigned)lo, (unsigned)hi); use_f = true;
    }
  }
  std::vector<gxvResult> r; gxvError e;
  Py_BEGIN_ALLOW_THREADS
  e = exact ? self->vx->Exact(v.data(), k, r, use_f ? &f : 0) : self->vx->Search(v.data(), k, r, ef, use_f ? &f : 0);
  Py_END_ALLOW_THREADS
  if(e != gxv_NO_ERROR) return raise("the search failed (gxvError " + std::to_string((int)e) + ")");
  PyObject *l = PyList_New((Py_ssize_t)r.size());
  if(!l) return 0;
  for(size_t i = 0; i < r.size(); i++) {
    PyObject *t = Py_BuildValue("(Ld(IIII))", r[i].id, (double)r[i].distance, r[i].tags[0], r[i].tags[1], r[i].tags[2], r[i].tags[3]);
    if(!t) { Py_DECREF(l); return 0; }
    PyList_SET_ITEM(l, (Py_ssize_t)i, t);
  }
  return l;
}

static PyObject *Vix_get(VixObject *self, PyObject *args)
{
  long long id;
  if(!PyArg_ParseTuple(args, "L", &id)) return 0;
  VIX_OPEN(self);
  std::vector<float> v(self->vx->Dim()); unsigned tags[gxvTagCount] = {0, 0, 0, 0};
  if(self->vx->Get(id, v.data(), tags) != gxv_NO_ERROR) Py_RETURN_NONE;
  PyObject *l = PyList_New((Py_ssize_t)v.size());
  if(!l) return 0;
  for(size_t i = 0; i < v.size(); i++) PyList_SET_ITEM(l, (Py_ssize_t)i, PyFloat_FromDouble(v[i]));
  return Py_BuildValue("(N(IIII))", l, tags[0], tags[1], tags[2], tags[3]);
}

// 7.5 beta 2: writes (VectorIndex(path, write=True))
static bool vec_arg(VixObject *self, PyObject *q, std::vector<float> &v)
{
  if(!floats(q, v)) return false;
  if(v.size() != self->vx->Dim()) { raise("the vector has " + std::to_string(v.size()) + " values, the index " + std::to_string(self->vx->Dim())); return false; }
  return true;
}
static bool tags_arg(PyObject *t, unsigned tags[gxvTagCount], bool &given)
{
  given = false; for(unsigned i = 0; i < gxvTagCount; i++) tags[i] = 0;
  if(!t || t == Py_None) return true;
  PyObject *seq = PySequence_Fast(t, "tags is a sequence of up to 4 unsigned integers");
  if(!seq) return false;
  Py_ssize_t n = PySequence_Fast_GET_SIZE(seq);
  if(n > (Py_ssize_t)gxvTagCount) { Py_DECREF(seq); raise("at most 4 tags"); return false; }
  for(Py_ssize_t i = 0; i < n; i++) { tags[i] = (unsigned)PyLong_AsUnsignedLong(PySequence_Fast_GET_ITEM(seq, i)); if(PyErr_Occurred()) { Py_DECREF(seq); return false; } }
  Py_DECREF(seq); given = true; return true;
}
static PyObject *vix_err(const char *what, gxvError e)
{
  static const char *names[] = {"", "not open", "database error", "bad file", "bad version", "bad argument", "unsupported",
				"spec mismatch", "duplicate id", "id not found", "not finite", "read only", "out of range", "index full"};
  std::string n = (int)e >= 0 && (int)e < (int)(sizeof(names) / sizeof(names[0])) ? names[(int)e] : "";
  return raise(std::string(what) + " failed: gxvError " + std::to_string((int)e) + (n.empty() ? "" : " (" + n + ")"));
}
static PyObject *Vix_insert_or_update(VixObject *self, PyObject *args, PyObject *kw, bool update)
{
  static const char *kwlist[] = {"id", "vector", "tags", 0};
  long long id; PyObject *q, *t = 0;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "LO|O", (char **)kwlist, &id, &q, &t)) return 0;
  VIX_OPEN(self);
  if(!self->writable) return raise("the vector index is open read only (VectorIndex(path, write=True))");
  std::vector<float> v; unsigned tags[gxvTagCount]; bool given;
  if(!vec_arg(self, q, v) || !tags_arg(t, tags, given)) return 0;
  gxvError e;
  Py_BEGIN_ALLOW_THREADS
  e = update ? self->vx->Update(id, v.data(), given ? tags : 0) : self->vx->Insert(id, v.data(), given ? tags : 0);
  Py_END_ALLOW_THREADS
  if(e != gxv_NO_ERROR) return vix_err(update ? "update" : "insert", e);
  Py_RETURN_NONE;
}
static PyObject *Vix_insert(VixObject *self, PyObject *args, PyObject *kw) { return Vix_insert_or_update(self, args, kw, false); }
static PyObject *Vix_update(VixObject *self, PyObject *args, PyObject *kw) { return Vix_insert_or_update(self, args, kw, true); }
static PyObject *Vix_delete(VixObject *self, PyObject *args)
{
  long long id; if(!PyArg_ParseTuple(args, "L", &id)) return 0;
  VIX_OPEN(self);
  if(!self->writable) return raise("the vector index is open read only (VectorIndex(path, write=True))");
  gxvError e;
  Py_BEGIN_ALLOW_THREADS
  e = self->vx->Delete(id);
  Py_END_ALLOW_THREADS
  if(e != gxv_NO_ERROR) return vix_err("delete", e);
  Py_RETURN_NONE;
}
static PyObject *Vix_flush(VixObject *self, PyObject *)
{
  VIX_OPEN(self); gxvError e;
  Py_BEGIN_ALLOW_THREADS
  e = self->vx->Flush();
  Py_END_ALLOW_THREADS
  if(e != gxv_NO_ERROR) return vix_err("flush", e);
  Py_RETURN_NONE;
}
static PyObject *Vix_repair(VixObject *self, PyObject *)
{
  VIX_OPEN(self);
  if(!self->writable) return raise("the vector index is open read only (VectorIndex(path, write=True))");
  gxvError e;
  Py_BEGIN_ALLOW_THREADS
  e = self->vx->Repair();
  Py_END_ALLOW_THREADS
  if(e != gxv_NO_ERROR) return vix_err("repair", e);
  Py_RETURN_NONE;
}
static PyObject *Vix_verify(VixObject *self, PyObject *)
{
  VIX_OPEN(self);
  std::string report; unsigned long n;
  Py_BEGIN_ALLOW_THREADS
  n = self->vx->Verify(0, 0, 0, &report);
  Py_END_ALLOW_THREADS
  return Py_BuildValue("(ks)", n, report.c_str());
}
static PyObject *Vix_close(VixObject *self, PyObject *)
{
  if(self->attached) return raise("the vector index is attached to a journal: close the journal");
  self->vx->Close(); Py_RETURN_NONE;
}
static PyObject *Vix_enter(VixObject *self, PyObject *) { Py_INCREF(self); return (PyObject *)self; }
static PyObject *Vix_exit(VixObject *self, PyObject *) { self->vx->Close(); Py_RETURN_FALSE; }
static PyObject *Vix_dim(VixObject *self, void *) { return PyLong_FromUnsignedLong(self->vx->Dim()); }
static PyObject *Vix_count(VixObject *self, void *) { return PyLong_FromUnsignedLongLong(self->vx->Count()); }
static PyObject *Vix_spec(VixObject *self, void *) { return PyUnicode_FromString(self->vx->IsOpen() ? self->vx->Spec() : ""); }
static PyObject *Vix_metric(VixObject *self, void *)
{
  gxvMetric m = self->vx->Metric();
  return PyUnicode_FromString(m == gxvL2 ? "l2" : m == gxvCOSINE ? "cosine" : m == gxvIP ? "ip" : "?");
}

static PyMethodDef Vix_methods[] = {
  {"search", (PyCFunction)(void (*)(void))Vix_search, METH_VARARGS | METH_KEYWORDS,
   "search(query, k=10, ef=64, exact=False, tags=None) -> [(id, distance, (t0, t1, t2, t3))], closest first. "
   "tags: {tag: value} or {tag: (lo, hi)}"},
  {"get", (PyCFunction)Vix_get, METH_VARARGS, "get(id) -> (vector, tags), or None"},
  {"insert", (PyCFunction)(void (*)(void))Vix_insert, METH_VARARGS | METH_KEYWORDS, "insert(id, vector, tags=None): a new vector (write=True)"},
  {"update", (PyCFunction)(void (*)(void))Vix_update, METH_VARARGS | METH_KEYWORDS, "update(id, vector, tags=None)"},
  {"delete", (PyCFunction)Vix_delete, METH_VARARGS, "delete(id): a tombstone (searches no longer find it)"},
  {"flush", (PyCFunction)Vix_flush, METH_NOARGS, "flush(): the nodes to the file"},
  {"repair", (PyCFunction)Vix_repair, METH_NOARGS, "repair(): rebuild the graph from the live nodes and write it"},
  {"verify", (PyCFunction)Vix_verify, METH_NOARGS, "verify() -> (problems, report): the file against the copy in memory"},
  {"close", (PyCFunction)Vix_close, METH_NOARGS, "close()"},
  {"__enter__", (PyCFunction)Vix_enter, METH_NOARGS, ""},
  {"__exit__", (PyCFunction)Vix_exit, METH_VARARGS, ""},
  {0, 0, 0, 0}
};
static PyGetSetDef Vix_getset[] = {
  {"dim", (getter)Vix_dim, 0, "values per vector", 0},
  {"count", (getter)Vix_count, 0, "live vectors", 0},
  {"spec", (getter)Vix_spec, 0, "the spec text kept in the file", 0},
  {"metric", (getter)Vix_metric, 0, "'l2', 'cosine' or 'ip'", 0},
  {0, 0, 0, 0, 0}
};
static PyTypeObject VixType = { PyVarObject_HEAD_INIT(0, 0) };

// ---------- BlockFile ----------
struct BlockObject {
  PyObject_HEAD
  gxDatabase *db;
};

static void Block_dealloc(BlockObject *self)
{
  if(self->db) { self->db->Close(); delete self->db; }
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Block_new(PyTypeObject *type, PyObject *, PyObject *)
{
  BlockObject *self = (BlockObject *)type->tp_alloc(type, 0);
  if(self) self->db = new gxDatabase;
  return (PyObject *)self;
}

static int Block_init(BlockObject *self, PyObject *args, PyObject *)
{
  PyObject *p = 0;
  if(!PyArg_ParseTuple(args, "O&", PyUnicode_FSConverter, &p)) return -1;
  std::string path(PyBytes_AS_STRING(p));
  Py_DECREF(p);
  gxDatabaseError e;
  Py_BEGIN_ALLOW_THREADS
  e = self->db->Open(path.c_str(), gxDBASE_READONLY);
  Py_END_ALLOW_THREADS
  if(e != gxDBASE_NO_ERROR) { raise("cannot open " + path + ": " + self->db->DatabaseExceptionMessage()); (void)e; return -1; }
  return 0;
}

#define BLOCK_OPEN(self) if(!(self)->db->IsOpen()) return raise("the block file is closed")

static PyObject *Block_addresses(BlockObject *self, PyObject *)
{
  BLOCK_OPEN(self);
  std::vector<unsigned long long> a;
  Py_BEGIN_ALLOW_THREADS
  for(FAU_t x = self->db->FindFirstObject(); x != (FAU_t)0; x = self->db->FindNextObject(x)) a.push_back((unsigned long long)x);
  Py_END_ALLOW_THREADS
  PyObject *l = PyList_New((Py_ssize_t)a.size());
  if(!l) return 0;
  for(size_t i = 0; i < a.size(); i++) PyList_SET_ITEM(l, (Py_ssize_t)i, PyLong_FromUnsignedLongLong(a[i]));
  return l;
}

static PyObject *Block_read(BlockObject *self, PyObject *args)
{
  unsigned long long a;
  if(!PyArg_ParseTuple(args, "K", &a)) return 0;
  BLOCK_OPEN(self);
  std::vector<unsigned char> b; gxDatabaseError e = gxDBASE_NO_ERROR; bool bad = false;
  Py_BEGIN_ALLOW_THREADS
  __ULWORD__ n = self->db->ObjectLength((FAU_t)a);
  if(n == 0 || n > 0x7fffffffUL) bad = true;
  else { b.resize(n); e = self->db->Read(b.data(), n, (FAU_t)a); }
  Py_END_ALLOW_THREADS
  if(bad) return raise("no object at address " + std::to_string(a));
  if(e != gxDBASE_NO_ERROR) return raise("cannot read address " + std::to_string(a) + ": " + self->db->DatabaseExceptionMessage());
  return PyBytes_FromStringAndSize((const char *)b.data(), (Py_ssize_t)b.size());
}

static PyObject *Block_close(BlockObject *self, PyObject *) { self->db->Close(); Py_RETURN_NONE; }
static PyObject *Block_enter(BlockObject *self, PyObject *) { Py_INCREF(self); return (PyObject *)self; }
static PyObject *Block_exit(BlockObject *self, PyObject *) { self->db->Close(); Py_RETURN_FALSE; }

static PyMethodDef Block_methods[] = {
  {"addresses", (PyCFunction)Block_addresses, METH_NOARGS, "addresses() -> the address of every object, in file order"},
  {"read", (PyCFunction)Block_read, METH_VARARGS, "read(address) -> the object's bytes (its whole block)"},
  {"close", (PyCFunction)Block_close, METH_NOARGS, "close()"},
  {"__enter__", (PyCFunction)Block_enter, METH_NOARGS, ""},
  {"__exit__", (PyCFunction)Block_exit, METH_VARARGS, ""},
  {0, 0, 0, 0}
};
static PyTypeObject BlockType = { PyVarObject_HEAD_INIT(0, 0) };

// ---------- Primary (7.5 beta 2): a gxObjRepl primary, written from Python ----------
struct PrimaryObject {
  PyObject_HEAD
  gxObjRepl *r;
};

static void Primary_dealloc(PrimaryObject *self)
{
  if(self->r) {
    Py_BEGIN_ALLOW_THREADS
    self->r->Close();
    Py_END_ALLOW_THREADS
    delete self->r;
  }
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Primary_new(PyTypeObject *type, PyObject *, PyObject *)
{
  PrimaryObject *self = (PrimaryObject *)type->tp_alloc(type, 0);
  if(self) self->r = new gxObjRepl;
  return (PyObject *)self;
}

// Primary(root, config=None, settings=None): CONFIG an objrepl.conf file, SETTINGS a dict of "key": value (the
// gxObjReplConfig keys: copy, ca, cert, key, index, links, name, peer-name, trim, idle-timeout, ack-timeout ...);
// settings win over the file. Messages go to stderr unless quiet=True
static int node_init(PrimaryObject *self, PyObject *args, PyObject *kw, gxObjRepl::Role role)
{
  static const char *kwlist[] = {"root", "config", "settings", "quiet", 0};
  PyObject *root_o = 0, *conf_o = 0, *settings = 0; int quiet = 0;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "O&|O&Op", (char **)kwlist, PyUnicode_FSConverter, &root_o,
				  PyUnicode_FSConverter, &conf_o, &settings, &quiet)) return -1;
  std::string root(PyBytes_AS_STRING(root_o)), conf = conf_o ? std::string(PyBytes_AS_STRING(conf_o)) : std::string();
  Py_DECREF(root_o); Py_XDECREF(conf_o);
  gxObjReplConfig cfg; std::string why;
  cfg.lock_wait = 0;              // 7.5 beta 2: a root another process holds fails at once (settings may change it)
  if(!conf.empty() && !cfg.Load(conf, &why)) { raise(why); return -1; }
  if(settings && settings != Py_None) {
    if(!PyDict_Check(settings)) { raise("settings is a dict of \"key\": value"); return -1; }
    PyObject *k, *v; Py_ssize_t pos = 0;
    while(PyDict_Next(settings, &pos, &k, &v)) {
      PyObject *ks = PyObject_Str(k), *vs = PyBool_Check(v) ? PyUnicode_FromString(v == Py_True ? "1" : "0") : PyObject_Str(v);
      if(!ks || !vs) { Py_XDECREF(ks); Py_XDECREF(vs); return -1; }
      const char *kc = PyUnicode_AsUTF8(ks), *vc = PyUnicode_AsUTF8(vs);
      bool ok = kc && vc && cfg.Set(kc, vc, &why);
      Py_DECREF(ks); Py_DECREF(vs);
      if(!ok) { if(!PyErr_Occurred()) raise(why); return -1; }
    }
  }
  if(quiet) cfg.log = [](const std::string &) {};
  int rc;
  Py_BEGIN_ALLOW_THREADS
  rc = self->r->Open(root, cfg, role);
  Py_END_ALLOW_THREADS
  if(rc == 4 && role == gxObjRepl::PRIMARY) { raise(root + " is a copy: a copy is not written to (see takeover)"); return -1; }
  if(rc == 4) { raise(root + " is a primary: it does not run as a copy (see rejoin)"); return -1; }
  if(rc != 0) { raise(self->r->Error()); return -1; }
  return 0;
}

static int Primary_init(PrimaryObject *self, PyObject *args, PyObject *kw) { return node_init(self, args, kw, gxObjRepl::PRIMARY); }
static int Copy_init(PrimaryObject *self, PyObject *args, PyObject *kw) { return node_init(self, args, kw, gxObjRepl::COPY); }

#define PRIMARY_OPEN(self) if(!(self)->r->IsOpen()) return raise("the node is closed")
// a call that returns 0 or -1 (Error()), made without the GIL
#define PRIMARY_CALL(self, expr) do { int rc_; Py_BEGIN_ALLOW_THREADS rc_ = (expr); Py_END_ALLOW_THREADS \
  if(rc_ != 0) return raise((self)->r->Error()); Py_RETURN_NONE; } while(0)

static PyObject *Primary_open_database(PrimaryObject *self, PyObject *args)
{
  const char *db; if(!PyArg_ParseTuple(args, "s", &db)) return 0;
  PRIMARY_OPEN(self); std::string d(db);
  PRIMARY_CALL(self, self->r->OpenDatabase(d));
}
static PyObject *Primary_begin(PrimaryObject *self, PyObject *) { PRIMARY_OPEN(self); PRIMARY_CALL(self, self->r->Begin()); }
static PyObject *Primary_commit(PrimaryObject *self, PyObject *) { PRIMARY_OPEN(self); PRIMARY_CALL(self, self->r->Commit()); }
static PyObject *Primary_rollback(PrimaryObject *self, PyObject *) { PRIMARY_OPEN(self); PRIMARY_CALL(self, self->r->Rollback()); }

static PyObject *Primary_new_id(PrimaryObject *self, PyObject *)
{
  PRIMARY_OPEN(self);
  uint64_t id;
  Py_BEGIN_ALLOW_THREADS
  id = self->r->NewId();
  Py_END_ALLOW_THREADS
  if(!id) return raise(self->r->Error());
  return PyLong_FromUnsignedLongLong(id);
}

static PyObject *Primary_put(PrimaryObject *self, PyObject *args)
{
  const char *db; unsigned long long id; Py_buffer b;
  if(!PyArg_ParseTuple(args, "sKy*", &db, &id, &b)) return 0;
  if(!self->r->IsOpen()) { PyBuffer_Release(&b); return raise("the primary is closed"); }
  if(b.len > 0xffffffffLL) { PyBuffer_Release(&b); return raise("an object is at most 32 MB"); }
  std::vector<unsigned char> v((const unsigned char *)b.buf, (const unsigned char *)b.buf + b.len);
  PyBuffer_Release(&b);
  std::string d(db);
  PRIMARY_CALL(self, self->r->Put(d, id, v.data(), (uint32_t)v.size()));
}

static PyObject *Primary_remove(PrimaryObject *self, PyObject *args)
{
  const char *db; unsigned long long id; if(!PyArg_ParseTuple(args, "sK", &db, &id)) return 0;
  PRIMARY_OPEN(self); std::string d(db);
  PRIMARY_CALL(self, self->r->Remove(d, id));
}
static PyObject *Primary_purge(PrimaryObject *self, PyObject *args)
{
  const char *db; unsigned long long id; if(!PyArg_ParseTuple(args, "sK", &db, &id)) return 0;
  PRIMARY_OPEN(self); std::string d(db);
  PRIMARY_CALL(self, self->r->Purge(d, id));
}

// pump() -> 0 the copy has every commit, 1 behind or away, 3 refused (an alarm); drain(seconds) -> 0, 2 (not drained in time) or 3
static PyObject *Primary_pump(PrimaryObject *self, PyObject *)
{
  PRIMARY_OPEN(self);
  int rc;
  Py_BEGIN_ALLOW_THREADS
  rc = self->r->Pump();
  Py_END_ALLOW_THREADS
  if(rc < 0) return raise(self->r->Error());
  return PyLong_FromLong(rc);
}
static PyObject *Primary_drain(PrimaryObject *self, PyObject *args)
{
  int seconds = 60; if(!PyArg_ParseTuple(args, "|i", &seconds)) return 0;
  PRIMARY_OPEN(self);
  int rc;
  Py_BEGIN_ALLOW_THREADS
  rc = self->r->Drain(seconds);
  Py_END_ALLOW_THREADS
  if(rc < 0) return raise(self->r->Error());
  return PyLong_FromLong(rc);
}

static PyObject *Primary_get(PrimaryObject *self, PyObject *args)
{
  const char *db; unsigned long long id; if(!PyArg_ParseTuple(args, "sK", &db, &id)) return 0;
  PRIMARY_OPEN(self);
  std::vector<unsigned char> b; int rc; std::string d(db);
  Py_BEGIN_ALLOW_THREADS
  rc = self->r->Get(d, id, b);
  Py_END_ALLOW_THREADS
  if(rc != 0) Py_RETURN_NONE;
  return PyBytes_FromStringAndSize((const char *)b.data(), (Py_ssize_t)b.size());
}
static PyObject *Primary_is_live(PrimaryObject *self, PyObject *args)
{
  const char *db; unsigned long long id; if(!PyArg_ParseTuple(args, "sK", &db, &id)) return 0;
  PRIMARY_OPEN(self); std::string d(db);
  return PyBool_FromLong(self->r->IsLive(d, id));
}
static PyObject *Primary_ids(PrimaryObject *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"db", "tombstones", "after", "limit", 0};
  const char *db; int tombs = 0; unsigned long long after = 0, limit = 0;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "s|pKK", (char **)kwlist, &db, &tombs, &after, &limit)) return 0;
  PRIMARY_OPEN(self);
  std::vector<uint64_t> ids; std::string d(db);
  Py_BEGIN_ALLOW_THREADS
  ids = self->r->Ids(d, tombs ? gxObjRepl::TOMBSTONES : gxObjRepl::LIVE);
  Py_END_ALLOW_THREADS
  std::sort(ids.begin(), ids.end());
  if(after || limit) {
    auto it = std::upper_bound(ids.begin(), ids.end(), (uint64_t)after);
    std::vector<uint64_t> page(it, limit && (size_t)(ids.end() - it) > limit ? it + (ptrdiff_t)limit : ids.end());
    return id_list(page);
  }
  return id_list(ids);
}
static PyObject *Primary_databases(PrimaryObject *self, PyObject *)
{
  PRIMARY_OPEN(self);
  std::vector<std::string> v = self->r->Databases();
  PyObject *l = PyList_New((Py_ssize_t)v.size());
  if(!l) return 0;
  for(size_t i = 0; i < v.size(); i++) PyList_SET_ITEM(l, (Py_ssize_t)i, PyUnicode_FromString(v[i].c_str()));
  return l;
}
static PyObject *Primary_find_key(PrimaryObject *self, PyObject *args)
{
  const char *index; Py_buffer key;
  if(!PyArg_ParseTuple(args, "sy*", &index, &key)) return 0;
  if(!self->r->IsOpen()) { PyBuffer_Release(&key); return raise("the primary is closed"); }
  if(key.len > 32) { PyBuffer_Release(&key); return raise("a key is at most 32 bytes"); }
  std::vector<unsigned char> k((const unsigned char *)key.buf, (const unsigned char *)key.buf + key.len);
  PyBuffer_Release(&key);
  std::vector<uint64_t> ids; int rc; std::string ix(index);
  Py_BEGIN_ALLOW_THREADS
  rc = self->r->FindKey(ix, k.data(), (unsigned)k.size(), ids);
  Py_END_ALLOW_THREADS
  if(rc != 0) return raise(self->r->Error());
  return id_list(ids);
}
static PyObject *Primary_nearest(PrimaryObject *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"index", "query", "k", 0};
  const char *index; PyObject *q; unsigned k = 10;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "sO|I", (char **)kwlist, &index, &q, &k)) return 0;
  PRIMARY_OPEN(self);
  std::vector<float> v;
  if(!floats(q, v)) return 0;
  if(v.empty()) return raise("an empty query vector");
  std::vector<gxObjReplHit> hits; int rc; std::string ix(index);
  Py_BEGIN_ALLOW_THREADS
  rc = self->r->Nearest(ix, v.data(), k, hits);
  Py_END_ALLOW_THREADS
  if(rc != 0) return raise(self->r->Error());
  PyObject *l = PyList_New((Py_ssize_t)hits.size());
  if(!l) return 0;
  for(size_t i = 0; i < hits.size(); i++) PyList_SET_ITEM(l, (Py_ssize_t)i, Py_BuildValue("(Kd)", (unsigned long long)hits[i].id, (double)hits[i].distance));
  return l;
}
static PyObject *Primary_close(PrimaryObject *self, PyObject *)
{
  Py_BEGIN_ALLOW_THREADS
  self->r->Close();
  Py_END_ALLOW_THREADS
  Py_RETURN_NONE;
}
static PyObject *Primary_enter(PrimaryObject *self, PyObject *) { Py_INCREF(self); return (PyObject *)self; }
static PyObject *Primary_exit(PrimaryObject *self, PyObject *)
{
  Py_BEGIN_ALLOW_THREADS
  self->r->Close();
  Py_END_ALLOW_THREADS
  Py_RETURN_FALSE;
}
static PyObject *Primary_last_seq(PrimaryObject *self, void *) { return PyLong_FromUnsignedLongLong(self->r->IsOpen() ? self->r->LastSeq() : 0); }
static PyObject *Primary_epoch(PrimaryObject *self, void *) { return PyLong_FromUnsignedLongLong(self->r->IsOpen() ? self->r->Epoch() : 0); }
static PyObject *Primary_copy_applied(PrimaryObject *self, void *) { return PyLong_FromUnsignedLongLong(self->r->IsOpen() ? self->r->CopyApplied() : 0); }
static PyObject *Primary_is_open(PrimaryObject *self, void *) { return PyBool_FromLong(self->r->IsOpen()); }

// links: an 8-byte link field holds this node's address of the target object
static PyObject *Primary_address_of(PrimaryObject *self, PyObject *args)
{
  const char *db; unsigned long long id; if(!PyArg_ParseTuple(args, "sK", &db, &id)) return 0;
  PRIMARY_OPEN(self); std::string d(db);
  return PyLong_FromUnsignedLongLong((unsigned long long)self->r->AddressOf(d, id));
}
static PyObject *Primary_id_at(PrimaryObject *self, PyObject *args)
{
  const char *db; unsigned long long a; if(!PyArg_ParseTuple(args, "sK", &db, &a)) return 0;
  PRIMARY_OPEN(self); std::string d(db);
  return PyLong_FromUnsignedLongLong(self->r->IdAt(d, (FAU_t)a));
}
// copy: serve() blocks (without the GIL) until stop(), from another thread or a signal handler
static PyObject *Copy_serve(PrimaryObject *self, PyObject *)
{
  PRIMARY_OPEN(self);
  int rc;
  Py_BEGIN_ALLOW_THREADS
  rc = self->r->Serve();
  Py_END_ALLOW_THREADS
  if(rc != 0) return raise(self->r->Error());
  Py_RETURN_NONE;
}
static PyObject *Copy_stop(PrimaryObject *self, PyObject *) { self->r->Stop(); Py_RETURN_NONE; }
static PyObject *Copy_in_snapshot(PrimaryObject *self, void *) { return PyBool_FromLong(self->r->IsOpen() && self->r->InSnapshot()); }
static PyObject *Copy_applied_seq(PrimaryObject *self, void *) { return PyLong_FromUnsignedLongLong(self->r->IsOpen() ? self->r->AppliedSeq() : 0); }
static PyObject *Copy_role(PrimaryObject *, void *) { return PyUnicode_FromString("copy"); }

static PyMethodDef Primary_methods[] = {
  {"address_of", (PyCFunction)Primary_address_of, METH_VARARGS, "address_of(db, id) -> this node's address of the object, for a link field (0 if none)"},
  {"id_at", (PyCFunction)Primary_id_at, METH_VARARGS, "id_at(db, address) -> the object id at this node's address (0 if none)"},
  {"open_database", (PyCFunction)Primary_open_database, METH_VARARGS, "open_database(db): open or create a database (outside a transaction)"},
  {"begin", (PyCFunction)Primary_begin, METH_NOARGS, "begin(): start a transaction"},
  {"new_id", (PyCFunction)Primary_new_id, METH_NOARGS, "new_id() -> an object id no database here has used"},
  {"put", (PyCFunction)Primary_put, METH_VARARGS, "put(db, id, data): add or update an object (bytes-like, at most 32 MB)"},
  {"remove", (PyCFunction)Primary_remove, METH_VARARGS, "remove(db, id): the object becomes a tombstone"},
  {"purge", (PyCFunction)Primary_purge, METH_VARARGS, "purge(db, id): free a tombstone for good"},
  {"commit", (PyCFunction)Primary_commit, METH_NOARGS, "commit(): durable when it returns, with its outbox entry"},
  {"rollback", (PyCFunction)Primary_rollback, METH_NOARGS, "rollback(): drop the open transaction"},
  {"pump", (PyCFunction)Primary_pump, METH_NOARGS, "pump() -> 0 (the copy has everything), 1 (behind or away) or 3 (refused)"},
  {"drain", (PyCFunction)Primary_drain, METH_VARARGS, "drain(seconds=60) -> 0 when the copy has every commit, 2 when it does not after SECONDS, 3 refused"},
  {"get", (PyCFunction)Primary_get, METH_VARARGS, "get(db, id) -> bytes, or None (also sees this transaction's changes)"},
  {"is_live", (PyCFunction)Primary_is_live, METH_VARARGS, "is_live(db, id) -> bool"},
  {"ids", (PyCFunction)(void (*)(void))Primary_ids, METH_VARARGS | METH_KEYWORDS, "ids(db, tombstones=False, after=0, limit=0) -> sorted ids (with AFTER or LIMIT: one page)"},
  {"databases", (PyCFunction)Primary_databases, METH_NOARGS, "databases() -> names"},
  {"find_key", (PyCFunction)Primary_find_key, METH_VARARGS, "find_key(index, key) -> ids"},
  {"nearest", (PyCFunction)(void (*)(void))Primary_nearest, METH_VARARGS | METH_KEYWORDS, "nearest(index, query, k=10) -> [(id, distance)]"},
  {"close", (PyCFunction)Primary_close, METH_NOARGS, "close(): checkpoint and close (an open transaction is rolled back)"},
  {"__enter__", (PyCFunction)Primary_enter, METH_NOARGS, ""},
  {"__exit__", (PyCFunction)Primary_exit, METH_VARARGS, ""},
  {0, 0, 0, 0}
};
static PyGetSetDef Primary_getset[] = {
  {"last_seq", (getter)Primary_last_seq, 0, "the last commit", 0},
  {"epoch", (getter)Primary_epoch, 0, "the node's epoch", 0},
  {"copy_applied", (getter)Primary_copy_applied, 0, "the copy's applied commit, as last heard", 0},
  {"is_open", (getter)Primary_is_open, 0, "whether the primary is open", 0},
  {0, 0, 0, 0, 0}
};
static PyTypeObject PrimaryType = { PyVarObject_HEAD_INIT(0, 0) };

static PyMethodDef Copy_methods[] = {
  {"serve", (PyCFunction)Copy_serve, METH_NOARGS, "serve(): listen and apply what the primary sends until stop() (blocks; run it in a thread)"},
  {"stop", (PyCFunction)Copy_stop, METH_NOARGS, "stop(): serve() returns within half a second (any thread)"},
  {"get", (PyCFunction)Primary_get, METH_VARARGS, "get(db, id) -> bytes, or None"},
  {"is_live", (PyCFunction)Primary_is_live, METH_VARARGS, "is_live(db, id) -> bool"},
  {"ids", (PyCFunction)(void (*)(void))Primary_ids, METH_VARARGS | METH_KEYWORDS, "ids(db, tombstones=False, after=0, limit=0) -> sorted ids (with AFTER or LIMIT: one page)"},
  {"databases", (PyCFunction)Primary_databases, METH_NOARGS, "databases() -> names"},
  {"find_key", (PyCFunction)Primary_find_key, METH_VARARGS, "find_key(index, key) -> ids"},
  {"nearest", (PyCFunction)(void (*)(void))Primary_nearest, METH_VARARGS | METH_KEYWORDS, "nearest(index, query, k=10) -> [(id, distance)]"},
  {"address_of", (PyCFunction)Primary_address_of, METH_VARARGS, "address_of(db, id) -> this node's address (0 if none)"},
  {"id_at", (PyCFunction)Primary_id_at, METH_VARARGS, "id_at(db, address) -> the object id (0 if none)"},
  {"close", (PyCFunction)Primary_close, METH_NOARGS, "close(): stops serve() and closes"},
  {"__enter__", (PyCFunction)Primary_enter, METH_NOARGS, ""},
  {"__exit__", (PyCFunction)Primary_exit, METH_VARARGS, ""},
  {0, 0, 0, 0}
};
static PyGetSetDef Copy_getset[] = {
  {"applied_seq", (getter)Copy_applied_seq, 0, "the last commit applied", 0},
  {"seq", (getter)Copy_applied_seq, 0, "the last commit applied (applied_seq)", 0},
  {"role", (getter)Copy_role, 0, "\"copy\"", 0},
  {"epoch", (getter)Primary_epoch, 0, "the node's epoch", 0},
  {"in_snapshot", (getter)Copy_in_snapshot, 0, "a full send is under way", 0},
  {"is_open", (getter)Primary_is_open, 0, "whether the copy is open", 0},
  {0, 0, 0, 0, 0}
};
static PyTypeObject CopyType = { PyVarObject_HEAD_INIT(0, 0) };

// ---------- 7.5 beta 2: the database layer (gxDatabase, gxBtree with byte keys, gxdJournal) ----------
static gxDatabaseAccessMode access_mode(int readonly) { return readonly ? gxDBASE_READONLY : gxDBASE_READWRITE; }

// Database(path, create=False, readonly=False): a gxDatabase block file
struct DatabaseObject {
  PyObject_HEAD
  gxDatabase *db;
  int attached;                    // attached to an open Journal: closed by the journal's close()
};
static void Database_dealloc(DatabaseObject *self)
{
  if(self->db) { if(!self->attached) self->db->Close(); else self->db = 0; }
  delete self->db;
  Py_TYPE(self)->tp_free((PyObject *)self);
}
static PyObject *Database_new(PyTypeObject *type, PyObject *, PyObject *)
{
  DatabaseObject *self = (DatabaseObject *)type->tp_alloc(type, 0);
  if(self) { self->db = new gxDatabase; self->attached = 0; }
  return (PyObject *)self;
}
static int Database_init(DatabaseObject *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"path", "create", "readonly", 0};
  PyObject *p = 0; int create = 0, ro = 0;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "O&|pp", (char **)kwlist, PyUnicode_FSConverter, &p, &create, &ro)) return -1;
  std::string path(PyBytes_AS_STRING(p)); Py_DECREF(p);
  gxDatabaseError e;
  Py_BEGIN_ALLOW_THREADS
  e = create ? self->db->Create(path.c_str()) : self->db->Open(path.c_str(), access_mode(ro));
  Py_END_ALLOW_THREADS
  if(e != gxDBASE_NO_ERROR) { raise((create ? "cannot create " : "cannot open ") + path + ": " + self->db->DatabaseExceptionMessage()); return -1; }
  return 0;
}
#define DB_OPEN(self) if(!(self)->db || !(self)->db->IsOpen()) return raise("the database is closed")
#define DB_ERR(self, e) if((e) != gxDBASE_NO_ERROR) return raise((self)->db->DatabaseExceptionMessage())

static PyObject *Database_alloc(DatabaseObject *self, PyObject *args)
{
  unsigned long long n; if(!PyArg_ParseTuple(args, "K", &n)) return 0;
  DB_OPEN(self);
  if(n == 0 || n > 0x7fffffffULL) return raise("alloc() size is 1 to 2 GB");
  FAU_t a;
  Py_BEGIN_ALLOW_THREADS
  a = self->db->Alloc((__ULWORD__)n);
  Py_END_ALLOW_THREADS
  if(!a) return raise(std::string("alloc failed: ") + self->db->DatabaseExceptionMessage());
  return PyLong_FromUnsignedLongLong((unsigned long long)a);
}
// write(address, data, offset=0): the bytes at offset into the object (the object must hold them)
static PyObject *Database_write(DatabaseObject *self, PyObject *args)
{
  unsigned long long a, off = 0; Py_buffer b;
  if(!PyArg_ParseTuple(args, "Ky*|K", &a, &b, &off)) return 0;
  if(!self->db || !self->db->IsOpen()) { PyBuffer_Release(&b); return raise("the database is closed"); }
  __ULWORD__ cap = self->db->ObjectLength((FAU_t)a);
  if(cap == 0 || off + (unsigned long long)b.len > cap) { PyBuffer_Release(&b); return raise("write past the object at " + std::to_string(a) + " (it holds " + std::to_string((unsigned long long)cap) + " bytes)"); }
  std::vector<unsigned char> v((const unsigned char *)b.buf, (const unsigned char *)b.buf + b.len);
  PyBuffer_Release(&b);
  gxDatabaseError e;
  Py_BEGIN_ALLOW_THREADS
  e = self->db->Write(v.data(), (__ULWORD__)v.size(), (FAU_t)(a + off));
  Py_END_ALLOW_THREADS
  DB_ERR(self, e);
  Py_RETURN_NONE;
}
static PyObject *Database_read(DatabaseObject *self, PyObject *args)
{
  unsigned long long a; long long n = -1; if(!PyArg_ParseTuple(args, "K|L", &a, &n)) return 0;
  DB_OPEN(self);
  __ULWORD__ cap = self->db->ObjectLength((FAU_t)a);
  if(cap == 0) return raise("no object at address " + std::to_string(a));
  __ULWORD__ len = (n < 0 || (unsigned long long)n > cap) ? cap : (__ULWORD__)n;
  std::vector<unsigned char> v(len); gxDatabaseError e;
  Py_BEGIN_ALLOW_THREADS
  e = self->db->Read(v.data(), len, (FAU_t)a);
  Py_END_ALLOW_THREADS
  DB_ERR(self, e);
  return PyBytes_FromStringAndSize((const char *)v.data(), (Py_ssize_t)v.size());
}
static PyObject *Database_length(DatabaseObject *self, PyObject *args)
{
  unsigned long long a; if(!PyArg_ParseTuple(args, "K", &a)) return 0;
  DB_OPEN(self);
  return PyLong_FromUnsignedLongLong((unsigned long long)self->db->ObjectLength((FAU_t)a));
}
static PyObject *Database_delete(DatabaseObject *self, PyObject *args)
{
  unsigned long long a; if(!PyArg_ParseTuple(args, "K", &a)) return 0;
  DB_OPEN(self);
  int ok;
  Py_BEGIN_ALLOW_THREADS
  ok = self->db->Delete((FAU_t)a);
  Py_END_ALLOW_THREADS
  if(!ok) return raise("cannot delete the object at " + std::to_string(a) + ": " + self->db->DatabaseExceptionMessage());
  Py_RETURN_NONE;
}
static PyObject *Database_addresses(DatabaseObject *self, PyObject *)
{
  DB_OPEN(self);
  std::vector<unsigned long long> a;
  Py_BEGIN_ALLOW_THREADS
  for(FAU_t x = self->db->FindFirstObject(); x != (FAU_t)0; x = self->db->FindNextObject(x)) a.push_back((unsigned long long)x);
  Py_END_ALLOW_THREADS
  PyObject *l = PyList_New((Py_ssize_t)a.size());
  if(!l) return 0;
  for(size_t i = 0; i < a.size(); i++) PyList_SET_ITEM(l, (Py_ssize_t)i, PyLong_FromUnsignedLongLong(a[i]));
  return l;
}
static PyObject *Database_flush(DatabaseObject *self, PyObject *)
{
  DB_OPEN(self); gxDatabaseError e;
  Py_BEGIN_ALLOW_THREADS
  e = self->db->Flush();
  Py_END_ALLOW_THREADS
  DB_ERR(self, e); Py_RETURN_NONE;
}
static PyObject *Database_sync(DatabaseObject *self, PyObject *)
{
  DB_OPEN(self); gxDatabaseError e;
  Py_BEGIN_ALLOW_THREADS
  e = self->db->Sync(1);
  Py_END_ALLOW_THREADS
  DB_ERR(self, e); Py_RETURN_NONE;
}
static PyObject *Database_close(DatabaseObject *self, PyObject *)
{
  if(self->attached) return raise("the database is attached to a journal: close the journal")
  ;
  if(self->db) {
    Py_BEGIN_ALLOW_THREADS
    self->db->Close();
    Py_END_ALLOW_THREADS
  }
  Py_RETURN_NONE;
}
static PyObject *Database_enter(DatabaseObject *self, PyObject *) { Py_INCREF(self); return (PyObject *)self; }
static PyObject *Database_exit(DatabaseObject *self, PyObject *)
{
  if(!self->attached && self->db) { Py_BEGIN_ALLOW_THREADS self->db->Close(); Py_END_ALLOW_THREADS }
  Py_RETURN_FALSE;
}
static PyMethodDef Database_methods[] = {
  {"alloc", (PyCFunction)Database_alloc, METH_VARARGS, "alloc(size) -> the address of a new object of at least SIZE bytes (deleted blocks are reused)"},
  {"write", (PyCFunction)Database_write, METH_VARARGS, "write(address, data, offset=0): bytes into the object at ADDRESS"},
  {"read", (PyCFunction)Database_read, METH_VARARGS, "read(address, size=-1) -> the object's bytes (all of its block by default)"},
  {"length", (PyCFunction)Database_length, METH_VARARGS, "length(address) -> the bytes the object's block holds (0: no object)"},
  {"delete", (PyCFunction)Database_delete, METH_VARARGS, "delete(address): the block is free for reuse"},
  {"addresses", (PyCFunction)Database_addresses, METH_NOARGS, "addresses() -> every object's address, in file order"},
  {"flush", (PyCFunction)Database_flush, METH_NOARGS, "flush(): write the header and buffers"},
  {"sync", (PyCFunction)Database_sync, METH_NOARGS, "sync(): flush and fsync the file"},
  {"close", (PyCFunction)Database_close, METH_NOARGS, "close()"},
  {"__enter__", (PyCFunction)Database_enter, METH_NOARGS, ""},
  {"__exit__", (PyCFunction)Database_exit, METH_VARARGS, ""},
  {0, 0, 0, 0}
};
static PyTypeObject DatabaseType = { PyVarObject_HEAD_INIT(0, 0) };

// a gxBtree key: KEY_SIZE bytes compared as bytes (memcmp), then an 8-byte unsigned value (an address or an id)
// compared as a number. The same layout as the gxObjRepl index files (32 bytes + the object id)
class BytesKey : public DatabaseKeyB {
public:
  BytesKey(size_t n) : DatabaseKeyB(0), n(n), d(n + 8, 0) { db_key = d.data(); }
  size_t KeySize() { return n + 8; }
  int CompareKey(const DatabaseKeyB &o) const {
    const unsigned char *k = (const unsigned char *)o.db_key;
    int c = memcmp(d.data(), k, n);
    if(c) return c < 0 ? -1 : 1;
    uint64_t a = value(), b; memcpy(&b, k + n, 8);
    return a < b ? -1 : a > b ? 1 : 0;
  }
  int operator==(const DatabaseKeyB &o) const { return CompareKey(o) == 0; }
  int operator>(const DatabaseKeyB &o) const { return CompareKey(o) > 0; }
  void set(const unsigned char *k, size_t kn, uint64_t v) { memset(d.data(), 0, n); memcpy(d.data(), k, kn < n ? kn : n); memcpy(d.data() + n, &v, 8); }
  uint64_t value() const { uint64_t v; memcpy(&v, d.data() + n, 8); return v; }
  PyObject *key_bytes() const { return PyBytes_FromStringAndSize((const char *)d.data(), (Py_ssize_t)n); }
  size_t n;
  std::vector<unsigned char> d;
};

static bool bt_is_open(gxBtree *b) { return b && b->gxDatabasePtr() && b->gxDatabasePtr()->IsOpen(); }
struct BTreeObject {
  PyObject_HEAD
  BytesKey *type;
  gxBtree *bt;
  size_t n;
  int attached;
};
static void BTree_dealloc(BTreeObject *self)
{
  if(self->bt && !self->attached) self->bt->Close();
  if(!self->attached) { delete self->bt; delete self->type; }    // an attached tree lives as long as its journal's refs
  Py_TYPE(self)->tp_free((PyObject *)self);
}
static PyObject *BTree_new(PyTypeObject *type, PyObject *, PyObject *)
{
  BTreeObject *self = (BTreeObject *)type->tp_alloc(type, 0);
  if(self) { self->type = 0; self->bt = 0; self->n = 0; self->attached = 0; }
  return (PyObject *)self;
}
// BTree(path, key_size, create=False, order=16, readonly=False)
static int BTree_init(BTreeObject *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"path", "key_size", "create", "order", "readonly", 0};
  PyObject *p = 0; unsigned key_size; int create = 0, order = 16, ro = 0;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "O&I|pip", (char **)kwlist, PyUnicode_FSConverter, &p, &key_size, &create, &order, &ro)) return -1;
  std::string path(PyBytes_AS_STRING(p)); Py_DECREF(p);
  if(key_size < 1 || key_size > 4096) { raise("key_size is 1 to 4096 bytes"); return -1; }
  if(order < 3 || order > 1024) { raise("order is 3 to 1024"); return -1; }
  self->n = key_size;
  self->type = new BytesKey(key_size);
  self->bt = new gxBtree(*self->type, (BtreeNodeOrder_t)order);
  gxDatabaseError e;
  Py_BEGIN_ALLOW_THREADS
  e = create ? self->bt->Create(path.c_str()) : self->bt->Open(path.c_str(), access_mode(ro));
  Py_END_ALLOW_THREADS
  if(e != gxDBASE_NO_ERROR) { raise((create ? "cannot create " : "cannot open ") + path + ": " + gxDatabaseExceptionMessage(e)); return -1; }
  return 0;
}
#define BT_OPEN(self) if(!(self)->bt || !bt_is_open((self)->bt)) return raise("the B-tree is closed")
static bool key_arg(BTreeObject *self, Py_buffer &b, BytesKey &k, uint64_t v)
{
  if((size_t)b.len > self->n) { PyBuffer_Release(&b); raise("the key is longer than key_size (" + std::to_string(self->n) + ")"); return false; }
  k.set((const unsigned char *)b.buf, (size_t)b.len, v);
  PyBuffer_Release(&b);
  return true;
}
static PyObject *BTree_insert(BTreeObject *self, PyObject *args)
{
  Py_buffer b; unsigned long long v = 0; if(!PyArg_ParseTuple(args, "y*|K", &b, &v)) return 0;
  if(!self->bt || !bt_is_open(self->bt)) { PyBuffer_Release(&b); return raise("the B-tree is closed"); }
  BytesKey k(self->n), c(self->n);
  if(!key_arg(self, b, k, v)) return 0;
  int ok;
  Py_BEGIN_ALLOW_THREADS
  ok = self->bt->Insert(k, c);
  Py_END_ALLOW_THREADS
  return PyBool_FromLong(ok != 0);                     // False: that key and value are there already
}
static PyObject *BTree_delete(BTreeObject *self, PyObject *args)
{
  Py_buffer b; unsigned long long v = 0; if(!PyArg_ParseTuple(args, "y*|K", &b, &v)) return 0;
  if(!self->bt || !bt_is_open(self->bt)) { PyBuffer_Release(&b); return raise("the B-tree is closed"); }
  BytesKey k(self->n), c(self->n);
  if(!key_arg(self, b, k, v)) return 0;
  int ok;
  Py_BEGIN_ALLOW_THREADS
  ok = self->bt->Delete(k, c);
  Py_END_ALLOW_THREADS
  return PyBool_FromLong(ok != 0);                     // False: not there
}
static PyObject *BTree_contains(BTreeObject *self, PyObject *args)
{
  Py_buffer b; unsigned long long v = 0; if(!PyArg_ParseTuple(args, "y*|K", &b, &v)) return 0;
  if(!self->bt || !bt_is_open(self->bt)) { PyBuffer_Release(&b); return raise("the B-tree is closed"); }
  BytesKey k(self->n), c(self->n);
  if(!key_arg(self, b, k, v)) return 0;
  return PyBool_FromLong(self->bt->Find(k, c) != 0);
}
// entries from the first one at or past (KEY, FROM_VALUE) while LIMIT and the key still starts with PREFIX
static PyObject *scan(BTreeObject *self, const unsigned char *start, size_t sn, uint64_t from_value,
		      const unsigned char *prefix, size_t pn, const unsigned char *stop, size_t stn, long limit)
{
  BytesKey k(self->n), c(self->n);
  PyObject *l = PyList_New(0);
  if(!l) return 0;
  int ok;
  if(start) { k.set(start, sn, from_value); ok = self->bt->PartialFind(k, c); }
  else ok = self->bt->FindFirst(k);
  for(; ok && limit != 0; ok = self->bt->FindNext(k, c)) {
    if(pn && memcmp(k.d.data(), prefix, pn) != 0) break;
    if(stn && memcmp(k.d.data(), stop, std::min(stn, self->n)) >= 0) break;
    PyObject *t = Py_BuildValue("(NK)", k.key_bytes(), (unsigned long long)k.value());
    if(!t || PyList_Append(l, t) < 0) { Py_XDECREF(t); Py_DECREF(l); return 0; }
    Py_DECREF(t);
    if(limit > 0) limit--;
  }
  return l;
}
static PyObject *BTree_find(BTreeObject *self, PyObject *args)
{
  Py_buffer b; if(!PyArg_ParseTuple(args, "y*", &b)) return 0;
  if(!self->bt || !bt_is_open(self->bt)) { PyBuffer_Release(&b); return raise("the B-tree is closed"); }
  if((size_t)b.len > self->n) { PyBuffer_Release(&b); return raise("the key is longer than key_size"); }
  std::vector<unsigned char> key(self->n, 0); memcpy(key.data(), b.buf, (size_t)b.len); PyBuffer_Release(&b);
  PyObject *all = scan(self, key.data(), self->n, 0, key.data(), self->n, 0, 0, -1);
  if(!all) return 0;
  PyObject *vals = PyList_New(PyList_GET_SIZE(all));
  for(Py_ssize_t i = 0; vals && i < PyList_GET_SIZE(all); i++) {
    PyObject *v = PyTuple_GET_ITEM(PyList_GET_ITEM(all, i), 1); Py_INCREF(v); PyList_SET_ITEM(vals, i, v);
  }
  Py_DECREF(all);
  return vals;
}
static PyObject *BTree_prefix(BTreeObject *self, PyObject *args)
{
  Py_buffer b; long limit = -1; if(!PyArg_ParseTuple(args, "y*|l", &b, &limit)) return 0;
  if(!self->bt || !bt_is_open(self->bt)) { PyBuffer_Release(&b); return raise("the B-tree is closed"); }
  if((size_t)b.len > self->n) { PyBuffer_Release(&b); return raise("the prefix is longer than key_size"); }
  std::vector<unsigned char> pre((const unsigned char *)b.buf, (const unsigned char *)b.buf + b.len); PyBuffer_Release(&b);
  return scan(self, pre.data(), pre.size(), 0, pre.data(), pre.size(), 0, 0, limit);
}
// range(start=None, stop=None, limit=-1): entries with start <= key < stop (keys compared as bytes)
static PyObject *BTree_range(BTreeObject *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"start", "stop", "limit", 0};
  PyObject *so = Py_None, *eo = Py_None; long limit = -1;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "|OOl", (char **)kwlist, &so, &eo, &limit)) return 0;
  BT_OPEN(self);
  std::vector<unsigned char> s, e;
  if(so != Py_None) { char *p; Py_ssize_t n; if(PyBytes_AsStringAndSize(so, &p, &n) < 0) return 0; s.assign(p, p + n); }
  if(eo != Py_None) { char *p; Py_ssize_t n; if(PyBytes_AsStringAndSize(eo, &p, &n) < 0) return 0; e.assign(p, p + n); }
  if(s.size() > self->n || e.size() > self->n) return raise("a range bound is longer than key_size");
  return scan(self, s.empty() ? 0 : s.data(), s.size(), 0, 0, 0, e.empty() ? 0 : e.data(), e.size(), limit);
}
static PyObject *BTree_count(BTreeObject *self, PyObject *)
{
  BT_OPEN(self);
  BytesKey k(self->n), c(self->n); unsigned long long n = 0;
  Py_BEGIN_ALLOW_THREADS
  for(int ok = self->bt->FindFirst(k); ok; ok = self->bt->FindNext(k, c)) n++;
  Py_END_ALLOW_THREADS
  return PyLong_FromUnsignedLongLong(n);
}
static PyObject *BTree_flush(BTreeObject *self, PyObject *)
{
  BT_OPEN(self); gxDatabaseError e = self->bt->Flush();
  if(e != gxDBASE_NO_ERROR) return raise(gxDatabaseExceptionMessage(e));
  Py_RETURN_NONE;
}
static PyObject *BTree_close(BTreeObject *self, PyObject *)
{
  if(self->attached) return raise("the B-tree is attached to a journal: close the journal");
  if(self->bt) self->bt->Close();
  Py_RETURN_NONE;
}
static PyObject *BTree_enter(BTreeObject *self, PyObject *) { Py_INCREF(self); return (PyObject *)self; }
static PyObject *BTree_exit(BTreeObject *self, PyObject *) { if(self->bt && !self->attached) self->bt->Close(); Py_RETURN_FALSE; }
static PyObject *BTree_key_size(BTreeObject *self, void *) { return PyLong_FromSize_t(self->n); }
static PyMethodDef BTree_methods[] = {
  {"insert", (PyCFunction)BTree_insert, METH_VARARGS, "insert(key, value=0) -> True, or False when that key and value are there already"},
  {"delete", (PyCFunction)BTree_delete, METH_VARARGS, "delete(key, value=0) -> True, or False when it was not there"},
  {"contains", (PyCFunction)BTree_contains, METH_VARARGS, "contains(key, value=0) -> bool"},
  {"find", (PyCFunction)BTree_find, METH_VARARGS, "find(key) -> the values stored under KEY (zero padded to key_size)"},
  {"prefix", (PyCFunction)BTree_prefix, METH_VARARGS, "prefix(prefix, limit=-1) -> [(key, value)] whose key starts with PREFIX, in key order"},
  {"range", (PyCFunction)(void (*)(void))BTree_range, METH_VARARGS | METH_KEYWORDS, "range(start=None, stop=None, limit=-1) -> [(key, value)] with start <= key < stop"},
  {"count", (PyCFunction)BTree_count, METH_NOARGS, "count() -> entries"},
  {"flush", (PyCFunction)BTree_flush, METH_NOARGS, "flush()"},
  {"close", (PyCFunction)BTree_close, METH_NOARGS, "close()"},
  {"__enter__", (PyCFunction)BTree_enter, METH_NOARGS, ""},
  {"__exit__", (PyCFunction)BTree_exit, METH_VARARGS, ""},
  {0, 0, 0, 0}
};
static PyGetSetDef BTree_getset[] = { {"key_size", (getter)BTree_key_size, 0, "key bytes (the value is 8 more)", 0}, {0, 0, 0, 0, 0} };
static PyTypeObject BTreeType = { PyVarObject_HEAD_INIT(0, 0) };

// Journal(path): gxdJournal over Database, BTree and VectorIndex files
struct JournalObject {
  PyObject_HEAD
  gxdJournal *j;
  PyObject *files;                 // the attached objects (kept alive while the journal is open)
};
static void journal_release(JournalObject *self)
{
  if(!self->files) return;
  for(Py_ssize_t i = 0; i < PyList_GET_SIZE(self->files); i++) {
    PyObject *o = PyList_GET_ITEM(self->files, i);
    if(PyObject_TypeCheck(o, &DatabaseType)) ((DatabaseObject *)o)->attached = 0;
    else if(PyObject_TypeCheck(o, &BTreeType)) ((BTreeObject *)o)->attached = 0;
    else if(PyObject_TypeCheck(o, &VixType)) ((VixObject *)o)->attached = 0;
  }
  Py_CLEAR(self->files);
}
static void Journal_dealloc(JournalObject *self)
{
  if(self->j) self->j->Close();
  journal_release(self);
  delete self->j;
  Py_TYPE(self)->tp_free((PyObject *)self);
}
static PyObject *Journal_new(PyTypeObject *type, PyObject *, PyObject *)
{
  JournalObject *self = (JournalObject *)type->tp_alloc(type, 0);
  if(self) { self->j = new gxdJournal; self->files = PyList_New(0); }
  return (PyObject *)self;
}
static int Journal_init(JournalObject *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"path", "write_back", 0};
  PyObject *p = 0; int wb = 0;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "O&|p", (char **)kwlist, PyUnicode_FSConverter, &p, &wb)) return -1;
  std::string path(PyBytes_AS_STRING(p)); Py_DECREF(p);
  gxDatabaseError e;
  Py_BEGIN_ALLOW_THREADS
  e = self->j->Open(path.c_str());
  Py_END_ALLOW_THREADS
  if(e != gxDBASE_NO_ERROR) { raise("cannot open the journal " + path + ": " + gxdJournal::RecoverError()); return -1; }
  if(wb) self->j->SetWriteMode(gxdJOURNAL_WRITE_BACK);
  return 0;
}
static PyObject *Journal_attach(JournalObject *self, PyObject *args)
{
  PyObject *o; if(!PyArg_ParseTuple(args, "O", &o)) return 0;
  if(self->j->InTransaction()) return raise("attach() inside a transaction: attach every file before begin()");
  gxDatabaseError e;
  if(PyObject_TypeCheck(o, &DatabaseType)) { DatabaseObject *d = (DatabaseObject *)o; if(!d->db->IsOpen()) return raise("the database is closed"); e = self->j->Attach(d->db); if(e == gxDBASE_NO_ERROR) d->attached = 1; }
  else if(PyObject_TypeCheck(o, &BTreeType)) { BTreeObject *t = (BTreeObject *)o; if(!t->bt || !bt_is_open(t->bt)) return raise("the B-tree is closed"); e = self->j->Attach(t->bt); if(e == gxDBASE_NO_ERROR) t->attached = 1; }
  else if(PyObject_TypeCheck(o, &VixType)) { VixObject *v = (VixObject *)o; if(!v->vx->IsOpen()) return raise("the vector index is closed"); e = self->j->Attach(v->vx->Database()); if(e == gxDBASE_NO_ERROR) v->attached = 1; }
  else return raise("attach() takes a Database, BTree or VectorIndex");
  if(e != gxDBASE_NO_ERROR) return raise(std::string("attach failed: ") + gxDatabaseExceptionMessage(e));
  PyList_Append(self->files, o);
  Py_RETURN_NONE;
}
#define J_CALL(expr) do { gxDatabaseError e_; Py_BEGIN_ALLOW_THREADS e_ = (expr); Py_END_ALLOW_THREADS \
  if(e_ != gxDBASE_NO_ERROR) return raise(std::string(#expr) + ": " + gxDatabaseExceptionMessage(e_)); Py_RETURN_NONE; } while(0)
static PyObject *Journal_begin(JournalObject *self, PyObject *) { J_CALL(self->j->Begin()); }
static PyObject *Journal_commit(JournalObject *self, PyObject *args)
{
  int durable = 1; if(!PyArg_ParseTuple(args, "|p", &durable)) return 0;
  J_CALL(self->j->Commit(durable));
}
static PyObject *Journal_rollback(JournalObject *self, PyObject *)
{
  gxDatabaseError e;
  Py_BEGIN_ALLOW_THREADS
  e = self->j->Rollback();
  Py_END_ALLOW_THREADS
  if(e != gxDBASE_NO_ERROR) return raise(std::string("rollback: ") + gxDatabaseExceptionMessage(e));
  // a vector index keeps its nodes in memory: read them again from the restored file
  for(Py_ssize_t i = 0; i < PyList_GET_SIZE(self->files); i++) {
    PyObject *o = PyList_GET_ITEM(self->files, i);
    if(PyObject_TypeCheck(o, &VixType)) ((VixObject *)o)->vx->Reload();
  }
  Py_RETURN_NONE;
}
static PyObject *Journal_checkpoint(JournalObject *self, PyObject *) { J_CALL(self->j->Checkpoint()); }
static PyObject *Journal_close(JournalObject *self, PyObject *)
{
  gxDatabaseError e;
  Py_BEGIN_ALLOW_THREADS
  e = self->j->Close();
  Py_END_ALLOW_THREADS
  journal_release(self);
  if(e != gxDBASE_NO_ERROR) return raise(std::string("close: ") + gxDatabaseExceptionMessage(e));
  Py_RETURN_NONE;
}
static PyObject *Journal_in_transaction(JournalObject *self, void *) { return PyBool_FromLong(self->j->InTransaction()); }
static PyObject *Journal_recovered(JournalObject *self, void *) { return PyLong_FromUnsignedLong(self->j->Recovered()); }
static PyObject *Journal_enter(JournalObject *self, PyObject *) { Py_INCREF(self); return (PyObject *)self; }
static PyObject *Journal_exit(JournalObject *self, PyObject *a) { PyObject *r = Journal_close(self, a); if(!r) return 0; Py_DECREF(r); Py_RETURN_FALSE; }
static PyMethodDef Journal_methods[] = {
  {"attach", (PyCFunction)Journal_attach, METH_VARARGS, "attach(file): a Database, BTree or VectorIndex (before begin())"},
  {"begin", (PyCFunction)Journal_begin, METH_NOARGS, "begin()"},
  {"commit", (PyCFunction)Journal_commit, METH_VARARGS, "commit(durable=True): every attached file's changes, all or none; durable: synced when it returns"},
  {"rollback", (PyCFunction)Journal_rollback, METH_NOARGS, "rollback(): the files as they were at begin()"},
  {"checkpoint", (PyCFunction)Journal_checkpoint, METH_NOARGS, "checkpoint(): write committed pages to the files and empty the journal"},
  {"close", (PyCFunction)Journal_close, METH_NOARGS, "close(): checkpoint and close; then close the files"},
  {"__enter__", (PyCFunction)Journal_enter, METH_NOARGS, ""},
  {"__exit__", (PyCFunction)Journal_exit, METH_VARARGS, ""},
  {0, 0, 0, 0}
};
static PyGetSetDef Journal_getset[] = {
  {"in_transaction", (getter)Journal_in_transaction, 0, "a transaction is open", 0},
  {"recovered", (getter)Journal_recovered, 0, "committed transactions the open replayed (after a crash)", 0},
  {0, 0, 0, 0, 0}
};
static PyTypeObject JournalType = { PyVarObject_HEAD_INIT(0, 0) };

// ---------- 7.5 beta 2: gxStream (TLS on port 2073) and the database object service client ----------
struct StreamObject {
  PyObject_HEAD
  gxStream *st;
};
static void Stream_dealloc(StreamObject *self)
{
  if(self->st) { Py_BEGIN_ALLOW_THREADS self->st->Close(); Py_END_ALLOW_THREADS delete self->st; }
  Py_TYPE(self)->tp_free((PyObject *)self);
}
static PyObject *Stream_new(PyTypeObject *type, PyObject *, PyObject *)
{
  StreamObject *self = (StreamObject *)type->tp_alloc(type, 0);
  if(self) self->st = new gxStream;
  return (PyObject *)self;
}
static std::string fs_arg(PyObject *o)
{
  if(!o || o == Py_None) return std::string();
  PyObject *b = 0;
  if(!PyUnicode_FSConverter(o, &b)) return std::string();
  std::string s(PyBytes_AS_STRING(b)); Py_DECREF(b);
  return s;
}
// Stream(host, port=2073, ca=None, cert=None, key=None, timeout=30): a gxStream client over TLS. The server's
// certificate is checked against CA (the system store without it) and HOST; CERT and KEY are this client's
// certificate (gxStream servers require one)
static int Stream_init(StreamObject *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"host", "port", "ca", "cert", "key", "timeout", 0};
  const char *host; int port = gxsDATA_PORT, timeout = 30; PyObject *ca = 0, *cert = 0, *key = 0;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "s|iOOOi", (char **)kwlist, &host, &port, &ca, &cert, &key, &timeout)) return -1;
  if(!host) return -1;
  std::string h(host), caf = fs_arg(ca), cf = fs_arg(cert), kf = fs_arg(key);
  if(PyErr_Occurred()) return -1;
  if(!caf.empty()) self->st->Connection().SetCAFile(caf);
  if(!cf.empty()) self->st->SetClientCert(cf, kf);
  self->st->SetTimeOut(timeout);
  int rc;
  Py_BEGIN_ALLOW_THREADS
  rc = self->st->Connect(h, port);
  Py_END_ALLOW_THREADS
  if(rc != 0) { raise("cannot connect to " + h + " port " + std::to_string(port) + ": " + self->st->Error()); return -1; }
  return 0;
}
static PyObject *Stream_write_block(StreamObject *self, PyObject *args)
{
  Py_buffer b; if(!PyArg_ParseTuple(args, "y*", &b)) return 0;
  std::vector<unsigned char> v((const unsigned char *)b.buf, (const unsigned char *)b.buf + b.len);
  PyBuffer_Release(&b);
  if(v.size() > (size_t)self->st->MaxBlockSize()) return raise("a block is at most " + std::to_string(self->st->MaxBlockSize()) + " bytes");
  int rc;
  Py_BEGIN_ALLOW_THREADS
  rc = self->st->WriteBlock(v.data(), (int)v.size());
  Py_END_ALLOW_THREADS
  if(rc != 0) return raise("write_block: " + self->st->Error());
  Py_RETURN_NONE;
}
static PyObject *Stream_read_block(StreamObject *self, PyObject *)
{
  gxBlockHeader gx; std::vector<unsigned char> v; int rc;
  Py_BEGIN_ALLOW_THREADS
  rc = self->st->ReadHeader(gx);
  if(rc == 0) rc = self->st->ReadBlock(v, gx);
  Py_END_ALLOW_THREADS
  if(rc != 0) return raise("read_block: " + (self->st->Error().empty() ? std::string("the connection closed") : self->st->Error()));
  return PyBytes_FromStringAndSize((const char *)v.data(), (Py_ssize_t)v.size());
}
static PyObject *Stream_peer(StreamObject *self, void *)
{
  return Py_BuildValue("(ss)", self->st->Connection().PeerCN().c_str(), self->st->Connection().PeerFingerprint().c_str());
}
static PyObject *Stream_close(StreamObject *self, PyObject *) { Py_BEGIN_ALLOW_THREADS self->st->Close(); Py_END_ALLOW_THREADS Py_RETURN_NONE; }
static PyObject *Stream_enter(StreamObject *self, PyObject *) { Py_INCREF(self); return (PyObject *)self; }
static PyObject *Stream_exit(StreamObject *self, PyObject *) { Py_BEGIN_ALLOW_THREADS self->st->Close(); Py_END_ALLOW_THREADS Py_RETURN_FALSE; }
static PyMethodDef Stream_methods[] = {
  {"write_block", (PyCFunction)Stream_write_block, METH_VARARGS, "write_block(data): one gxStream block"},
  {"read_block", (PyCFunction)Stream_read_block, METH_NOARGS, "read_block() -> the next block's bytes (waits up to the timeout)"},
  {"close", (PyCFunction)Stream_close, METH_NOARGS, "close()"},
  {"__enter__", (PyCFunction)Stream_enter, METH_NOARGS, ""},
  {"__exit__", (PyCFunction)Stream_exit, METH_VARARGS, ""},
  {0, 0, 0, 0}
};
static PyGetSetDef Stream_getset[] = { {"peer", (getter)Stream_peer, 0, "(CN, SHA-256 fingerprint) of the client's certificate, on a stream StreamServer.accept() returned", 0}, {0, 0, 0, 0, 0} };
static PyTypeObject StreamType = { PyVarObject_HEAD_INIT(0, 0) };

// StreamServer(port, cert, key, client_ca, allow_cn=None, address="127.0.0.1"): a gxStream server; clients must
// present a certificate from CLIENT_CA, and with ALLOW_CN one of those names
struct StreamServerObject {
  PyObject_HEAD
  gxStreamServer *srv;
};
static void StreamServer_dealloc(StreamServerObject *self)
{
  if(self->srv) { self->srv->Listener().Close(); delete self->srv; }
  Py_TYPE(self)->tp_free((PyObject *)self);
}
static PyObject *StreamServer_new(PyTypeObject *type, PyObject *, PyObject *)
{
  StreamServerObject *self = (StreamServerObject *)type->tp_alloc(type, 0);
  if(self) self->srv = new gxStreamServer;
  return (PyObject *)self;
}
static int StreamServer_init(StreamServerObject *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"port", "cert", "key", "client_ca", "allow_cn", "address", 0};
  int port; PyObject *cert, *key, *ca, *allow = Py_None; const char *addr = "127.0.0.1";
  if(!PyArg_ParseTupleAndKeywords(args, kw, "iOOO|Os", (char **)kwlist, &port, &cert, &key, &ca, &allow, &addr)) return -1;
  std::string cf = fs_arg(cert), kf = fs_arg(key), caf = fs_arg(ca);
  if(PyErr_Occurred()) return -1;
  self->srv->SetClientCAFile(caf);
  if(allow != Py_None) {
    PyObject *seq = PyUnicode_Check(allow) ? PyTuple_Pack(1, allow) : PySequence_Tuple(allow);
    if(!seq) return -1;
    for(Py_ssize_t i = 0; i < PyTuple_GET_SIZE(seq); i++) {
      const char *cn = PyUnicode_AsUTF8(PyTuple_GET_ITEM(seq, i));
      if(!cn || self->srv->AllowClientCN(cn) != 0) { Py_DECREF(seq); if(!PyErr_Occurred()) raise("bad allow_cn"); return -1; }
    }
    Py_DECREF(seq);
  } else self->srv->AllowAnonymousClients(true);       // any certificate the client CA issued
  std::string a(addr); int rc;
  Py_BEGIN_ALLOW_THREADS
  rc = self->srv->Listen(port, cf, kf, std::string(), a);
  Py_END_ALLOW_THREADS
  if(rc != 0) { raise("cannot listen on " + a + ":" + std::to_string(port) + ": " + self->srv->Error()); return -1; }
  return 0;
}
// accept(timeout=None) -> Stream, or None after TIMEOUT seconds (a client that fails the checks is closed)
static PyObject *StreamServer_accept(StreamServerObject *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"timeout", "stream_timeout", 0};
  PyObject *to = Py_None; int stream_timeout = 30;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "|Oi", (char **)kwlist, &to, &stream_timeout)) return 0;
  int ms = -1;
  if(to != Py_None) { double t = PyFloat_AsDouble(to); if(PyErr_Occurred()) return 0; ms = (int)(t * 1000); }
  StreamObject *s = (StreamObject *)StreamType.tp_alloc(&StreamType, 0);
  if(!s) return 0;
  s->st = new gxStream;
  s->st->SetTimeOut(stream_timeout);
  int rc, w = 1;
  Py_BEGIN_ALLOW_THREADS
  if(ms >= 0) w = self->srv->Listener().WaitReadable(ms);
  rc = w > 0 ? self->srv->Accept(*s->st) : 1;
  Py_END_ALLOW_THREADS
  if(w <= 0) { Py_DECREF(s); Py_RETURN_NONE; }
  if(rc != 0) { std::string e = self->srv->Error(); Py_DECREF(s); return raise("accept: " + e); }
  return (PyObject *)s;
}
static PyObject *StreamServer_close(StreamServerObject *self, PyObject *) { self->srv->Listener().Close(); Py_RETURN_NONE; }
static PyObject *StreamServer_enter(StreamServerObject *self, PyObject *) { Py_INCREF(self); return (PyObject *)self; }
static PyObject *StreamServer_exit(StreamServerObject *self, PyObject *) { self->srv->Listener().Close(); Py_RETURN_FALSE; }
static PyMethodDef StreamServer_methods[] = {
  {"accept", (PyCFunction)(void (*)(void))StreamServer_accept, METH_VARARGS | METH_KEYWORDS, "accept(timeout=None, stream_timeout=30) -> Stream, or None after TIMEOUT seconds"},
  {"close", (PyCFunction)StreamServer_close, METH_NOARGS, "close()"},
  {"__enter__", (PyCFunction)StreamServer_enter, METH_NOARGS, ""},
  {"__exit__", (PyCFunction)StreamServer_exit, METH_VARARGS, ""},
  {0, 0, 0, 0}
};
static PyTypeObject StreamServerType = { PyVarObject_HEAD_INIT(0, 0) };

// ObjectClient(host, port, db, ca, cert, key, server_name=None, write=False): the database object service of a
// DataReel object server (object_server, gxdbobject.h) over one multiplex channel
struct ObjectClientObject {
  PyObject_HEAD
  gxStream *st;
  gxdMuxSession *mux;
};
static void ObjectClient_dealloc(ObjectClientObject *self)
{
  Py_BEGIN_ALLOW_THREADS
  if(self->mux) { self->mux->Shutdown(); delete self->mux; }
  delete self->st;
  Py_END_ALLOW_THREADS
  Py_TYPE(self)->tp_free((PyObject *)self);
}
static PyObject *ObjectClient_new(PyTypeObject *type, PyObject *, PyObject *)
{
  ObjectClientObject *self = (ObjectClientObject *)type->tp_alloc(type, 0);
  if(self) { self->st = new gxStream; self->mux = 0; }
  return (PyObject *)self;
}
static int ObjectClient_init(ObjectClientObject *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"host", "port", "db", "ca", "cert", "key", "server_name", "write", 0};
  const char *host, *db, *sn = 0; int port, write = 0; PyObject *ca, *cert, *key;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "sisOOO|zp", (char **)kwlist, &host, &port, &db, &ca, &cert, &key, &sn, &write)) return -1;
  std::string h(host), d(db), name(sn ? sn : host), caf = fs_arg(ca), cf = fs_arg(cert), kf = fs_arg(key);
  if(PyErr_Occurred()) return -1;
  gxsTLSConnection &c = self->st->Connection();
  c.SetCAFile(caf); c.SetClientCert(cf, kf); c.SetVerifyPeer(true);
  std::string err; int rc = 0;
  Py_BEGIN_ALLOW_THREADS
  gxSocket sock;
  if(sock.Connect(h, port, 10) != 0 || c.AttachClient(std::move(sock), name) != 0) { err = "cannot connect: " + c.Error(); rc = -1; }
  else {
    self->mux = new gxdMuxSession(gxdMuxStreamTransport(*self->st));
    self->mux->Start();
    if(self->mux->Hello(err) != 0 || self->mux->Open(1, "object", d, write ? "write" : "read", 0, err) != 0) rc = -1;
  }
  Py_END_ALLOW_THREADS
  if(rc != 0) { raise(err); return -1; }
  return 0;
}
static PyObject *obj_call(ObjectClientObject *self, uint8_t op, const std::vector<unsigned char> &body, std::vector<unsigned char> &res)
{
  if(!self->mux) { raise("not connected"); return 0; }
  std::vector<unsigned char> m(1, op), back; m.insert(m.end(), body.begin(), body.end());
  std::string err; int rc; uint64_t seq;
  Py_BEGIN_ALLOW_THREADS
  rc = self->mux->Send(1, m, gxdMUX_LAST, err);
  if(rc == 0) rc = self->mux->ReceiveMessage(1, back, seq, 30000, err);
  Py_END_ALLOW_THREADS
  if(rc != 0) { raise(err.empty() ? std::string("the connection closed") : err); return 0; }
  if(back.empty()) { raise("an empty reply"); return 0; }
  if(back[0] != 0) {
    std::string why = "the server refused the request";
    uint32_t l; if(back.size() >= 5) { memcpy(&l, back.data() + 1, 4); if(back.size() >= 5 + (size_t)l) why = std::string((const char *)back.data() + 5, l); }
    raise(why); return 0;
  }
  res.assign(back.begin() + 1, back.end());
  return Py_None;
}
static void p64v(std::vector<unsigned char> &v, uint64_t x) { unsigned char b[8]; memcpy(b, &x, 8); v.insert(v.end(), b, b + 8); }
static void p32v(std::vector<unsigned char> &v, uint32_t x) { unsigned char b[4]; memcpy(b, &x, 4); v.insert(v.end(), b, b + 4); }
static uint64_t g64v(const std::vector<unsigned char> &v, size_t o) { uint64_t x = 0; if(o + 8 <= v.size()) memcpy(&x, v.data() + o, 8); return x; }
static uint32_t g32v(const std::vector<unsigned char> &v, size_t o) { uint32_t x = 0; if(o + 4 <= v.size()) memcpy(&x, v.data() + o, 4); return x; }

static PyObject *ObjectClient_add(ObjectClientObject *self, PyObject *args)
{
  Py_buffer b; if(!PyArg_ParseTuple(args, "y*", &b)) return 0;
  std::vector<unsigned char> body, res; p64v(body, (uint64_t)b.len);
  body.insert(body.end(), (const unsigned char *)b.buf, (const unsigned char *)b.buf + b.len); PyBuffer_Release(&b);
  if(!obj_call(self, gxOBJ_ADD, body, res)) return 0;
  return PyLong_FromUnsignedLongLong(g64v(res, 0));
}
static PyObject *ObjectClient_request(ObjectClientObject *self, PyObject *args)
{
  unsigned long long a; if(!PyArg_ParseTuple(args, "K", &a)) return 0;
  std::vector<unsigned char> body, res; p64v(body, a);
  if(!obj_call(self, gxOBJ_REQUEST, body, res)) return 0;
  uint64_t n = g64v(res, 0);
  if(res.size() < 8 + n) return raise("a short reply");
  return PyBytes_FromStringAndSize((const char *)res.data() + 8, (Py_ssize_t)n);
}
static PyObject *ObjectClient_change(ObjectClientObject *self, PyObject *args)
{
  unsigned long long a; Py_buffer b; if(!PyArg_ParseTuple(args, "Ky*", &a, &b)) return 0;
  std::vector<unsigned char> body, res; p64v(body, a); p64v(body, (uint64_t)b.len);
  body.insert(body.end(), (const unsigned char *)b.buf, (const unsigned char *)b.buf + b.len); PyBuffer_Release(&b);
  if(!obj_call(self, gxOBJ_CHANGE, body, res)) return 0;
  Py_RETURN_NONE;
}
static PyObject *ObjectClient_delete(ObjectClientObject *self, PyObject *args)
{
  unsigned long long a; if(!PyArg_ParseTuple(args, "K", &a)) return 0;
  std::vector<unsigned char> body, res; p64v(body, a);
  if(!obj_call(self, gxOBJ_DELETE, body, res)) return 0;
  Py_RETURN_NONE;
}
static PyObject *ObjectClient_addresses(ObjectClientObject *self, PyObject *)
{
  std::vector<unsigned char> body, res;
  PyObject *l = PyList_New(0); if(!l) return 0;
  if(!obj_call(self, gxOBJ_FIRST, body, res)) { Py_DECREF(l); return 0; }
  for(uint64_t a = g64v(res, 0); a; ) {
    PyObject *x = PyLong_FromUnsignedLongLong(a); PyList_Append(l, x); Py_DECREF(x);
    body.clear(); p64v(body, a);
    if(!obj_call(self, gxOBJ_NEXT, body, res)) { Py_DECREF(l); return 0; }
    a = g64v(res, 0);
  }
  return l;
}
static PyObject *ObjectClient_vector_info(ObjectClientObject *self, PyObject *)
{
  std::vector<unsigned char> body, res;
  if(!obj_call(self, gxOBJ_VECTOR_INFO, body, res)) return 0;
  uint32_t sl = g32v(res, 36);
  std::string spec = 40 + (size_t)sl <= res.size() ? std::string((const char *)res.data() + 40, sl) : std::string();
  return Py_BuildValue("{s:I,s:I,s:I,s:I,s:K,s:K,s:s}", "dim", g32v(res, 4), "element", g32v(res, 8), "metric", g32v(res, 12),
		       "files", g32v(res, 16), "nodes", (unsigned long long)g64v(res, 20), "deleted", (unsigned long long)g64v(res, 28), "spec", spec.c_str());
}
// vector_search(query, k=10, ef=0, tags=None) -> [(id, distance, tags, file)]
static PyObject *ObjectClient_vector_search(ObjectClientObject *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"query", "k", "ef", "tags", 0};
  PyObject *q, *tags = 0; unsigned k = 10, ef = 0;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "O|IIO", (char **)kwlist, &q, &k, &ef, &tags)) return 0;
  std::vector<float> v; if(!floats(q, v)) return 0;
  uint32_t mask = 0, lo[4] = {0, 0, 0, 0}, hi[4] = {0, 0, 0, 0};
  if(tags && tags != Py_None) {
    if(!PyDict_Check(tags)) return raise("tags is a dict {tag: value or (lo, hi)}");
    PyObject *key, *val; Py_ssize_t pos = 0;
    while(PyDict_Next(tags, &pos, &key, &val)) {
      long i = PyLong_AsLong(key);
      if(i < 0 || i > 3) { if(!PyErr_Occurred()) raise("a tag number is 0 to 3"); return 0; }
      if(PyTuple_Check(val) && PyTuple_GET_SIZE(val) == 2) { lo[i] = (uint32_t)PyLong_AsUnsignedLong(PyTuple_GET_ITEM(val, 0)); hi[i] = (uint32_t)PyLong_AsUnsignedLong(PyTuple_GET_ITEM(val, 1)); }
      else lo[i] = hi[i] = (uint32_t)PyLong_AsUnsignedLong(val);
      if(PyErr_Occurred()) return 0;
      mask |= 1u << i;
    }
  }
  std::vector<unsigned char> body, res;
  p32v(body, 1); p32v(body, k); p32v(body, ef); p32v(body, (uint32_t)v.size()); p32v(body, 0); p32v(body, mask);
  for(int i = 0; i < 4; i++) p32v(body, lo[i]);
  for(int i = 0; i < 4; i++) p32v(body, hi[i]);
  p32v(body, 0); p32v(body, 0);
  body.insert(body.end(), (const unsigned char *)v.data(), (const unsigned char *)v.data() + 4 * v.size());
  if(!obj_call(self, gxOBJ_VECTOR_SEARCH, body, res)) return 0;
  uint32_t n = g32v(res, 4); size_t o = 16;
  PyObject *l = PyList_New(0); if(!l) return 0;
  for(uint32_t i = 0; i < n && o + 32 <= res.size(); i++, o += 32) {
    long long id; float d; uint32_t t[4], f;
    memcpy(&id, res.data() + o, 8); memcpy(&d, res.data() + o + 8, 4); memcpy(t, res.data() + o + 12, 16); memcpy(&f, res.data() + o + 28, 4);
    PyObject *x = Py_BuildValue("(Ld(IIII)I)", id, (double)d, t[0], t[1], t[2], t[3], f);
    PyList_Append(l, x); Py_DECREF(x);
  }
  return l;
}
static PyObject *ObjectClient_close(ObjectClientObject *self, PyObject *)
{
  Py_BEGIN_ALLOW_THREADS
  if(self->mux) { self->mux->Shutdown(); delete self->mux; self->mux = 0; }
  Py_END_ALLOW_THREADS
  Py_RETURN_NONE;
}
static PyObject *ObjectClient_enter(ObjectClientObject *self, PyObject *) { Py_INCREF(self); return (PyObject *)self; }
static PyObject *ObjectClient_exit(ObjectClientObject *self, PyObject *a) { PyObject *r = ObjectClient_close(self, a); Py_XDECREF(r); Py_RETURN_FALSE; }
static PyMethodDef ObjectClient_methods[] = {
  {"add", (PyCFunction)ObjectClient_add, METH_VARARGS, "add(data) -> the new object's address (a write channel)"},
  {"request", (PyCFunction)ObjectClient_request, METH_VARARGS, "request(address) -> the object's bytes"},
  {"change", (PyCFunction)ObjectClient_change, METH_VARARGS, "change(address, data): the same length as the object"},
  {"delete", (PyCFunction)ObjectClient_delete, METH_VARARGS, "delete(address)"},
  {"addresses", (PyCFunction)ObjectClient_addresses, METH_NOARGS, "addresses() -> every object's address (FIRST/NEXT)"},
  {"vector_info", (PyCFunction)ObjectClient_vector_info, METH_NOARGS, "vector_info() -> the database's vector index (dim, element, metric, files, nodes, deleted, spec)"},
  {"vector_search", (PyCFunction)(void (*)(void))ObjectClient_vector_search, METH_VARARGS | METH_KEYWORDS, "vector_search(query, k=10, ef=0, tags=None) -> [(id, distance, tags, file)]"},
  {"close", (PyCFunction)ObjectClient_close, METH_NOARGS, "close()"},
  {"__enter__", (PyCFunction)ObjectClient_enter, METH_NOARGS, ""},
  {"__exit__", (PyCFunction)ObjectClient_exit, METH_VARARGS, ""},
  {0, 0, 0, 0}
};
static PyTypeObject ObjectClientType = { PyVarObject_HEAD_INIT(0, 0) };

// ---------- the module ----------
static PyObject *mod_library_version(PyObject *, PyObject *) { return PyUnicode_FromString(DATAREEL_VERSION_STRING); }
static PyObject *mod_engine(PyObject *, PyObject *) { return PyLong_FromLong(sizeof(FAU_t) * 8); }

// 7.5 beta 2: node folder tools (no node open on ROOT), as the objrepl tool's status, digest and takeover
static PyObject *mod_status(PyObject *, PyObject *args)
{
  PyObject *r = 0; if(!PyArg_ParseTuple(args, "O&", PyUnicode_FSConverter, &r)) return 0;
  std::string root(PyBytes_AS_STRING(r)); Py_DECREF(r);
  std::string line; int rc;
  Py_BEGIN_ALLOW_THREADS
  rc = gxObjRepl::Status(root, line);
  Py_END_ALLOW_THREADS
  if(rc != 0) return raise(line.empty() ? "no state in " + root : line + " (" + root + ")");
  PyObject *d = PyDict_New(); if(!d) return 0;
  // "epoch E next_seq N acked_seq A applied_seq P in_snapshot S role R digest H"
  std::vector<std::string> w; size_t i = 0;
  while(i < line.size()) { size_t j = line.find(' ', i); if(j == std::string::npos) j = line.size(); if(j > i) w.push_back(line.substr(i, j - i)); i = j + 1; }
  for(size_t k = 0; k + 1 < w.size(); k += 2) {
    const std::string &key = w[k], &val = w[k + 1];
    PyObject *v = (key == "role" || key == "digest") ? PyUnicode_FromString(val.c_str()) : PyLong_FromString(val.c_str(), 0, 10);
    if(!v) { PyErr_Clear(); v = PyUnicode_FromString(val.c_str()); }
    PyDict_SetItemString(d, key.c_str(), v); Py_DECREF(v);
  }
  return d;
}
static PyObject *mod_digest(PyObject *, PyObject *args)
{
  PyObject *r = 0; const char *links = "";
  if(!PyArg_ParseTuple(args, "O&|s", PyUnicode_FSConverter, &r, &links)) return 0;
  std::string root(PyBytes_AS_STRING(r)), lk(links); Py_DECREF(r);
  std::string line, detail; int rc;
  Py_BEGIN_ALLOW_THREADS
  rc = gxObjRepl::Digest(root, lk, line, &detail);
  Py_END_ALLOW_THREADS
  if(rc != 0) return raise(line);
  return Py_BuildValue("(ss)", line.c_str(), detail.c_str());
}
static PyObject *mod_takeover(PyObject *, PyObject *args)
{
  PyObject *r = 0; if(!PyArg_ParseTuple(args, "O&", PyUnicode_FSConverter, &r)) return 0;
  std::string root(PyBytes_AS_STRING(r)); Py_DECREF(r);
  std::string msg; int rc;
  Py_BEGIN_ALLOW_THREADS
  rc = gxObjRepl::Takeover(root, msg);
  Py_END_ALLOW_THREADS
  if(rc != 0) return raise(msg);
  return PyUnicode_FromString(msg.c_str());
}

// create_vector_index(path, dim, metric="l2", element="float32", m=16, spec=None, ef_construction=200)
static PyObject *mod_create_vix(PyObject *, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"path", "dim", "metric", "element", "m", "spec", "ef_construction", 0};
  PyObject *p = 0; unsigned dim, m = 16, efc = 200; const char *metric = "l2", *elem = "float32", *spec = 0;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "O&I|ssIzI", (char **)kwlist, PyUnicode_FSConverter, &p, &dim, &metric, &elem, &m, &spec, &efc)) return 0;
  std::string path(PyBytes_AS_STRING(p)); Py_DECREF(p);
  std::string mt(metric), el(elem), sp(spec ? spec : "");
  gxvMetric me = mt == "l2" ? gxvL2 : mt == "cosine" ? gxvCOSINE : mt == "ip" ? gxvIP : (gxvMetric)0;
  gxvElementType et = el == "float32" ? gxvFLOAT32 : el == "float16" ? gxvFLOAT16 : el == "int8" ? gxvINT8 : (gxvElementType)0;
  if(!me) return raise("metric is l2, cosine or ip");
  if(!et) return raise("element is float32, float16 or int8");
  gxVectorIndex v; gxvError e;
  Py_BEGIN_ALLOW_THREADS
  e = v.Create(path.c_str(), dim, et, me, m, spec ? sp.c_str() : 0, efc);
  if(e == gxv_NO_ERROR) v.Close();
  Py_END_ALLOW_THREADS
  if(e != gxv_NO_ERROR) return vix_err("create", e);
  Py_RETURN_NONE;
}
// merge_vector_indexes(out, inputs, spec=None, m=0, ef_construction=200) -> report
static PyObject *mod_merge_vix(PyObject *, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"out", "inputs", "spec", "m", "ef_construction", 0};
  PyObject *o = 0, *in; const char *spec = 0; unsigned m = 0, efc = 200;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "O&O|zII", (char **)kwlist, PyUnicode_FSConverter, &o, &in, &spec, &m, &efc)) return 0;
  std::string out(PyBytes_AS_STRING(o)), sp(spec ? spec : ""); Py_DECREF(o);
  PyObject *seq = PySequence_Fast(in, "inputs is a list of .vix paths");
  if(!seq) return 0;
  std::vector<std::string> files;
  for(Py_ssize_t i = 0; i < PySequence_Fast_GET_SIZE(seq); i++) {
    PyObject *b = 0;
    if(!PyUnicode_FSConverter(PySequence_Fast_GET_ITEM(seq, i), &b)) { Py_DECREF(seq); return 0; }
    files.push_back(PyBytes_AS_STRING(b)); Py_DECREF(b);
  }
  Py_DECREF(seq);
  std::string report; gxvError e;
  Py_BEGIN_ALLOW_THREADS
  e = gxVectorIndex::Merge(out.c_str(), files, spec ? sp.c_str() : 0, m, efc, &report);
  Py_END_ALLOW_THREADS
  if(e != gxv_NO_ERROR) return vix_err("merge", e);
  return PyUnicode_FromString(report.c_str());
}

// rejoin(root, incremental=True): 7.5 beta 3 incremental by default (see gxObjRepl::Rejoin)
static PyObject *mod_rejoin(PyObject *, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"root", "incremental", 0};
  PyObject *r = 0; int incr = 1;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "O&|p", (char **)kwlist, PyUnicode_FSConverter, &r, &incr)) return 0;
  std::string root(PyBytes_AS_STRING(r)); Py_DECREF(r);
  std::string msg; int rc;
  Py_BEGIN_ALLOW_THREADS
  rc = gxObjRepl::Rejoin(root, msg, incr != 0);
  Py_END_ALLOW_THREADS
  if(rc != 0) return raise(msg);
  return PyUnicode_FromString(msg.c_str());
}
// swap(old, new, index="") -> message (7.5 beta 3, gxObjRepl::Swap)
static PyObject *mod_swap(PyObject *, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"old", "new", "index", 0};
  PyObject *a = 0, *b = 0; const char *ix = "";
  if(!PyArg_ParseTupleAndKeywords(args, kw, "O&O&|s", (char **)kwlist, PyUnicode_FSConverter, &a, PyUnicode_FSConverter, &b, &ix)) return 0;
  std::string o(PyBytes_AS_STRING(a)), n(PyBytes_AS_STRING(b)), spec(ix); Py_DECREF(a); Py_DECREF(b);
  std::string msg; int rc;
  Py_BEGIN_ALLOW_THREADS
  rc = gxObjRepl::Swap(o, n, spec, msg);
  Py_END_ALLOW_THREADS
  if(rc != 0) return raise(msg);
  return PyUnicode_FromString(msg.c_str());
}
static PyObject *mod_clear_alarm(PyObject *, PyObject *args)
{
  PyObject *r = 0; if(!PyArg_ParseTuple(args, "O&", PyUnicode_FSConverter, &r)) return 0;
  std::string root(PyBytes_AS_STRING(r)); Py_DECREF(r);
  std::string msg; int rc;
  Py_BEGIN_ALLOW_THREADS
  rc = gxObjRepl::ClearAlarm(root, msg);
  Py_END_ALLOW_THREADS
  if(rc != 0) return raise(msg);
  return PyUnicode_FromString(msg.c_str());
}
static PyObject *mod_index_check(PyObject *, PyObject *args)
{
  PyObject *r = 0; const char *ix;
  if(!PyArg_ParseTuple(args, "O&s", PyUnicode_FSConverter, &r, &ix)) return 0;
  std::string root(PyBytes_AS_STRING(r)), spec(ix); Py_DECREF(r);
  std::string report; int rc;
  Py_BEGIN_ALLOW_THREADS
  rc = gxObjRepl::IndexCheck(root, spec, report);
  Py_END_ALLOW_THREADS
  return Py_BuildValue("(Os)", rc == 0 ? Py_True : Py_False, report.c_str());
}
// check(root, settings=None, config=None) -> (level, line): 0 OK, 1 WARN, 2 FAIL, from the status and alarm files
static PyObject *mod_check(PyObject *, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"root", "settings", "config", 0};
  PyObject *r = 0, *settings = 0, *conf_o = 0;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "O&|OO&", (char **)kwlist, PyUnicode_FSConverter, &r, &settings,
				  PyUnicode_FSConverter, &conf_o)) return 0;
  std::string root(PyBytes_AS_STRING(r)), conf = conf_o ? std::string(PyBytes_AS_STRING(conf_o)) : std::string();
  Py_DECREF(r); Py_XDECREF(conf_o);
  gxObjReplConfig cfg; std::string why;
  if(!conf.empty() && !cfg.Load(conf, &why)) return raise(why);
  if(settings && settings != Py_None) {
    if(!PyDict_Check(settings)) return raise("settings is a dict of \"key\": value");
    PyObject *k, *v; Py_ssize_t pos = 0;
    while(PyDict_Next(settings, &pos, &k, &v)) {
      PyObject *ks = PyObject_Str(k), *vs = PyObject_Str(v);
      if(!ks || !vs) { Py_XDECREF(ks); Py_XDECREF(vs); return 0; }
      bool ok = cfg.Set(PyUnicode_AsUTF8(ks), PyUnicode_AsUTF8(vs), &why);
      Py_DECREF(ks); Py_DECREF(vs);
      if(!ok) return raise(why);
    }
  }
  std::string line; int rc;
  Py_BEGIN_ALLOW_THREADS
  rc = gxObjRepl::Check(root, cfg, line);
  Py_END_ALLOW_THREADS
  return Py_BuildValue("(is)", rc, line.c_str());
}

static PyMethodDef module_methods[] = {
  {"create_vector_index", (PyCFunction)(void (*)(void))mod_create_vix, METH_VARARGS | METH_KEYWORDS,
   "create_vector_index(path, dim, metric='l2', element='float32', m=16, spec=None, ef_construction=200): a new .vix file"},
  {"merge_vector_indexes", (PyCFunction)(void (*)(void))mod_merge_vix, METH_VARARGS | METH_KEYWORDS,
   "merge_vector_indexes(out, inputs, spec=None, m=0, ef_construction=200) -> report: one index from many (the day files)"},
  {"rejoin", (PyCFunction)(void (*)(void))mod_rejoin, METH_VARARGS | METH_KEYWORDS, "rejoin(root, incremental=True) -> message: a former primary (or a copy of another history) becomes a copy; beta 3: an incremental rejoin (only the objects either node changed since the split) when the new primary can match the histories and the schemas agree, else a full send"},
  {"swap", (PyCFunction)(void (*)(void))mod_swap, METH_VARARGS | METH_KEYWORDS, "swap(old, new, index='') -> message: after a rebuild into NEW (both stopped), OLD is kept as OLD.before-rebuild-DATE and NEW becomes OLD"},
  {"clear_alarm", mod_clear_alarm, METH_VARARGS, "clear_alarm(root) -> message: the node's alarm file is set aside"},
  {"index_check", mod_index_check, METH_VARARGS, "index_check(root, index) -> (consistent, report): each index against the objects"},
  {"check", (PyCFunction)(void (*)(void))mod_check, METH_VARARGS | METH_KEYWORDS, "check(root, settings=None, config=None) -> (0 OK / 1 WARN / 2 FAIL, line)"},
  {"status", mod_status, METH_VARARGS, "status(root) -> dict (epoch, next_seq, acked_seq, applied_seq, in_snapshot, role, digest) of a node no process holds"},
  {"digest", mod_digest, METH_VARARGS, "digest(root, links='') -> (line, detail): ids and contents with links as ids; two nodes with the same objects give the same line"},
  {"takeover", mod_takeover, METH_VARARGS, "takeover(root) -> message: a stopped copy becomes a primary at the next epoch (it never follows its old primary again)"},
  {"library_version", mod_library_version, METH_NOARGS, "the DataReel C++ library version this module was built from"},
  {"engine_bits", mod_engine, METH_NOARGS, "64 for the 64-bit database engine, 32 otherwise"},
  {0, 0, 0, 0}
};

static struct PyModuleDef module_def = {
  PyModuleDef_HEAD_INIT, "_datareel", "DataReel C++ library bindings (see the datareel package)", -1, module_methods,
  0, 0, 0, 0
};

static bool ready(PyTypeObject &t, const char *name, Py_ssize_t size, destructor dealloc, newfunc nf, initproc init,
		  PyMethodDef *m, PyGetSetDef *gs, const char *doc)
{
  t.tp_name = name; t.tp_basicsize = size; t.tp_dealloc = dealloc; t.tp_flags |= Py_TPFLAGS_DEFAULT;
  t.tp_new = nf; t.tp_init = init; t.tp_methods = m; t.tp_getset = gs; t.tp_doc = doc;
  return PyType_Ready(&t) == 0;
}

PyMODINIT_FUNC PyInit__datareel(void)
{
  if(!ready(ReaderType, "datareel.Reader", sizeof(ReaderObject), (destructor)Reader_dealloc, Reader_new, (initproc)Reader_init,
	    Reader_methods, Reader_getset,
	    "Reader(root, index='') - a gxObjRepl node folder, read only, while the node runs in another process. "
	    "index: the node's index spec, for find_key() and nearest(). Each call sees whole commits only.")) return 0;
  if(!ready(VixType, "datareel.VectorIndex", sizeof(VixObject), (destructor)Vix_dealloc, Vix_new, (initproc)Vix_init,
	    Vix_methods, Vix_getset,
	    "VectorIndex(path, spec=None) - a .vix file, read only. Use it on files no process is writing; "
	    "for a running node use Reader.nearest().")) return 0;
  if(!ready(DatabaseType, "datareel.Database", sizeof(DatabaseObject), (destructor)Database_dealloc, Database_new, (initproc)Database_init,
	    Database_methods, 0, "Database(path, create=False, readonly=False): a gxDatabase block file, read and write")) return 0;
  if(!ready(BTreeType, "datareel.BTree", sizeof(BTreeObject), (destructor)BTree_dealloc, BTree_new, (initproc)BTree_init,
	    BTree_methods, BTree_getset, "BTree(path, key_size, create=False, order=16, readonly=False): a gxBtree index file of "
	    "KEY_SIZE-byte keys (compared as bytes) with an 8-byte value each")) return 0;
  if(!ready(JournalType, "datareel.Journal", sizeof(JournalObject), (destructor)Journal_dealloc, Journal_new, (initproc)Journal_init,
	    Journal_methods, Journal_getset, "Journal(path, write_back=False): a gxdJournal: atomic, crash-safe transactions over "
	    "attached Database, BTree and VectorIndex files")) return 0;
  if(!ready(StreamType, "datareel.Stream", sizeof(StreamObject), (destructor)Stream_dealloc, Stream_new, (initproc)Stream_init,
	    Stream_methods, Stream_getset, "Stream(host, port=2073, ca=None, cert=None, key=None, timeout=30): a gxStream client "
	    "over TLS: write_block(), read_block()")) return 0;
  if(!ready(StreamServerType, "datareel.StreamServer", sizeof(StreamServerObject), (destructor)StreamServer_dealloc, StreamServer_new,
	    (initproc)StreamServer_init, StreamServer_methods, 0, "StreamServer(port, cert, key, client_ca, allow_cn=None, "
	    "address='127.0.0.1'): a gxStream server (mutual TLS): accept() -> Stream")) return 0;
  if(!ready(ObjectClientType, "datareel.ObjectClient", sizeof(ObjectClientObject), (destructor)ObjectClient_dealloc, ObjectClient_new,
	    (initproc)ObjectClient_init, ObjectClient_methods, 0, "ObjectClient(host, port, db, ca, cert, key, server_name=None, "
	    "write=False): the object service of a DataReel object server")) return 0;
  if(!ready(CopyType, "datareel.Copy", sizeof(PrimaryObject), (destructor)Primary_dealloc, Primary_new, (initproc)Copy_init,
	    Copy_methods, Copy_getset,
	    "Copy(root, config=None, settings=None, quiet=False): a gxObjRepl copy node: serve() applies what the primary "
	    "sends until stop(); reads as Reader. Settings: listen, ca, cert, key, allow-cn, index, name, peer-name ...")) return 0;
  PrimaryType.tp_flags = Py_TPFLAGS_BASETYPE;   // datareel.Primary (Python) adds transaction()
  if(!ready(PrimaryType, "datareel._Primary", sizeof(PrimaryObject), (destructor)Primary_dealloc, Primary_new, (initproc)Primary_init,
	    Primary_methods, Primary_getset,
	    "_Primary(root, config=None, settings=None, quiet=False): a gxObjRepl primary (see datareel.Primary)")) return 0;
  if(!ready(BlockType, "datareel.BlockFile", sizeof(BlockObject), (destructor)Block_dealloc, Block_new, (initproc)Block_init,
	    Block_methods, 0,
	    "BlockFile(path) - a .gxd file, read only: its objects by address. Use it on files no process is writing.")) return 0;
  PyObject *m = PyModule_Create(&module_def);
  if(!m) return 0;
  DataReelError = PyErr_NewException("datareel.Error", PyExc_RuntimeError, 0);
  if(!DataReelError ||
     PyModule_AddObjectRef(m, "Error", DataReelError) < 0 ||
     PyModule_AddObjectRef(m, "Reader", (PyObject *)&ReaderType) < 0 ||
     PyModule_AddObjectRef(m, "VectorIndex", (PyObject *)&VixType) < 0 ||
     PyModule_AddObjectRef(m, "BlockFile", (PyObject *)&BlockType) < 0 ||
     PyModule_AddObjectRef(m, "_Primary", (PyObject *)&PrimaryType) < 0 ||
     PyModule_AddObjectRef(m, "Copy", (PyObject *)&CopyType) < 0 ||
     PyModule_AddObjectRef(m, "Database", (PyObject *)&DatabaseType) < 0 ||
     PyModule_AddObjectRef(m, "BTree", (PyObject *)&BTreeType) < 0 ||
     PyModule_AddObjectRef(m, "Journal", (PyObject *)&JournalType) < 0 ||
     PyModule_AddObjectRef(m, "Stream", (PyObject *)&StreamType) < 0 ||
     PyModule_AddObjectRef(m, "StreamServer", (PyObject *)&StreamServerType) < 0 ||
     PyModule_AddObjectRef(m, "ObjectClient", (PyObject *)&ObjectClientType) < 0) { Py_DECREF(m); return 0; }
  return m;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
