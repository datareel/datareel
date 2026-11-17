// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxproclock.cpp
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/17/2026
// Date Last Modified: 11/17/2026
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

Single-instance process lock. See gxproclock.h.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#if defined (_WIN32)
#include <windows.h>
#include <process.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#endif
#include "gxproclock.h"

namespace datareel {

#if defined (_WIN32)
static long long my_pid() { return (long long)GetCurrentProcessId(); }
#else
static long long my_pid() { return (long long)getpid(); }
#endif

bool gxProcessLock::Acquire(const std::string &fname)
{
  Release();
  error.clear(); holder = 0; name = fname;
#if defined (_WIN32)
  // shared read (IsLocked() and HolderPid() read the id), the lock is on the file's first byte
  HANDLE f = CreateFileA(fname.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_ALWAYS,
			 FILE_ATTRIBUTE_NORMAL, 0);
  if(f == INVALID_HANDLE_VALUE) { error = "cannot open " + fname + " (error " + std::to_string((unsigned long)GetLastError()) + ")"; return false; }
  SetHandleInformation(f, HANDLE_FLAG_INHERIT, 0);
  OVERLAPPED ov; memset(&ov, 0, sizeof(ov));
  if(!LockFileEx(f, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &ov)) {
    // the holder's id: after the locked first byte (byte 0 is locked; the id is written from byte 1)
    char b[32] = {0}; DWORD n = 0;
    OVERLAPPED ro; memset(&ro, 0, sizeof(ro)); ro.Offset = 1;
    if(ReadFile(f, b, sizeof(b) - 1, &n, &ro) && n) holder = atoll(b);
    CloseHandle(f);
    error = fname + " is locked" + (holder ? " by process " + std::to_string(holder) : std::string());
    return false;
  }
  char b[40]; int len = snprintf(b, sizeof(b), " %lld\n", my_pid());
  DWORD w = 0; SetFilePointer(f, 0, 0, FILE_BEGIN);
  WriteFile(f, b, (DWORD)len, &w, 0); SetEndOfFile(f); FlushFileBuffers(f);
  h = f;
#else
  int d = ::open(fname.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
  if(d < 0) { error = "cannot open " + fname + ": " + strerror(errno); return false; }
  if(flock(d, LOCK_EX | LOCK_NB) != 0) {
    int e = errno;
    char b[32] = {0}; ssize_t n = pread(d, b, sizeof(b) - 1, 0);
    if(n > 0) holder = atoll(b);
    ::close(d);
    error = e == EWOULDBLOCK ? fname + " is locked" + (holder ? " by process " + std::to_string(holder) : std::string()) : "cannot lock " + fname + ": " + strerror(e);
    return false;
  }
  char b[32]; int len = snprintf(b, sizeof(b), "%lld\n", my_pid());
  if(ftruncate(d, 0) != 0 || pwrite(d, b, (size_t)len, 0) != (ssize_t)len) { }   // the id is a help, not the lock
  fd = d;
#endif
  held = true; holder = my_pid();
  return true;
}

void gxProcessLock::Release()
{
  if(!held) return;
#if defined (_WIN32)
  HANDLE f = (HANDLE)h;
  SetFilePointer(f, 0, 0, FILE_BEGIN); SetEndOfFile(f);
  OVERLAPPED ov; memset(&ov, 0, sizeof(ov));
  UnlockFileEx(f, 0, 1, 0, &ov);
  CloseHandle(f); h = 0;
#else
  if(ftruncate(fd, 0) != 0) { }
  flock(fd, LOCK_UN); ::close(fd); fd = -1;
#endif
  held = false; holder = 0;
}

bool gxProcessLock::IsLocked(const std::string &fname, long long *pid)
{
  if(pid) *pid = 0;
  gxProcessLock t;
  if(t.Acquire(fname)) { t.Release(); return false; }
  if(pid) *pid = t.HolderPid();
  return t.HolderPid() != 0 || t.Error().find("is locked") != std::string::npos;
}

} // namespace datareel
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
