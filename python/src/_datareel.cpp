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
  static const char *kwlist[] = {"db", "tombstones", 0};
  const char *db; int tombs = 0;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "s|p", (char **)kwlist, &db, &tombs)) return 0;
  READER_OPEN(self);
  std::vector<uint64_t> ids; int rc; std::string d(db);
  Py_BEGIN_ALLOW_THREADS
  rc = self->r->Ids(d, ids, tombs ? gxObjRepl::TOMBSTONES : gxObjRepl::LIVE);
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

static PyMethodDef Reader_methods[] = {
  {"get", (PyCFunction)Reader_get, METH_VARARGS, "get(db, id) -> bytes, or None when the object is not live"},
  {"is_live", (PyCFunction)Reader_is_live, METH_VARARGS, "is_live(db, id) -> bool"},
  {"ids", (PyCFunction)(void (*)(void))Reader_ids, METH_VARARGS | METH_KEYWORDS, "ids(db, tombstones=False) -> sorted list of ids"},
  {"databases", (PyCFunction)Reader_databases, METH_NOARGS, "databases() -> list of database names"},
  {"find_key", (PyCFunction)Reader_find_key, METH_VARARGS, "find_key(index, key: bytes) -> ids whose B-tree key starts with key"},
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
  {0, 0, 0, 0, 0}
};

static PyTypeObject ReaderType = { PyVarObject_HEAD_INIT(0, 0) };

// ---------- VectorIndex ----------
struct VixObject {
  PyObject_HEAD
  gxVectorIndex *vx;
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
  static const char *kwlist[] = {"path", "spec", 0};
  PyObject *p = 0; const char *spec = 0;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "O&|z", (char **)kwlist, PyUnicode_FSConverter, &p, &spec)) return -1;
  std::string path(PyBytes_AS_STRING(p)), sp(spec ? spec : "");
  Py_DECREF(p);
  gxvError e;
  Py_BEGIN_ALLOW_THREADS
  e = self->vx->Open(path.c_str(), gxDBASE_READONLY, spec ? sp.c_str() : 0);
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

static PyObject *Vix_close(VixObject *self, PyObject *) { self->vx->Close(); Py_RETURN_NONE; }
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
  static const char *kwlist[] = {"db", "tombstones", 0};
  const char *db; int tombs = 0;
  if(!PyArg_ParseTupleAndKeywords(args, kw, "s|p", (char **)kwlist, &db, &tombs)) return 0;
  PRIMARY_OPEN(self);
  std::vector<uint64_t> ids; std::string d(db);
  Py_BEGIN_ALLOW_THREADS
  ids = self->r->Ids(d, tombs ? gxObjRepl::TOMBSTONES : gxObjRepl::LIVE);
  Py_END_ALLOW_THREADS
  std::sort(ids.begin(), ids.end());
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
  {"ids", (PyCFunction)(void (*)(void))Primary_ids, METH_VARARGS | METH_KEYWORDS, "ids(db, tombstones=False) -> sorted ids"},
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
  {"ids", (PyCFunction)(void (*)(void))Primary_ids, METH_VARARGS | METH_KEYWORDS, "ids(db, tombstones=False) -> sorted ids"},
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
  {"epoch", (getter)Primary_epoch, 0, "the node's epoch", 0},
  {"in_snapshot", (getter)Copy_in_snapshot, 0, "a full send is under way", 0},
  {"is_open", (getter)Primary_is_open, 0, "whether the copy is open", 0},
  {0, 0, 0, 0, 0}
};
static PyTypeObject CopyType = { PyVarObject_HEAD_INIT(0, 0) };

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

static PyObject *mod_rejoin(PyObject *, PyObject *args)
{
  PyObject *r = 0; if(!PyArg_ParseTuple(args, "O&", PyUnicode_FSConverter, &r)) return 0;
  std::string root(PyBytes_AS_STRING(r)); Py_DECREF(r);
  std::string msg; int rc;
  Py_BEGIN_ALLOW_THREADS
  rc = gxObjRepl::Rejoin(root, msg);
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
  {"rejoin", mod_rejoin, METH_VARARGS, "rejoin(root) -> message: a former primary (or a copy of another history) becomes a copy waiting for a full send"},
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
     PyModule_AddObjectRef(m, "Copy", (PyObject *)&CopyType) < 0) { Py_DECREF(m); return 0; }
  return m;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
