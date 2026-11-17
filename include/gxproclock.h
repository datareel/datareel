// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxproclock.h
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/17/2026
// Date Last Modified: 11/17/2026
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
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

gxProcessLock: one instance of a program at a time. Acquire() takes an
exclusive lock on a lock file (flock on POSIX, LockFileEx on Windows)
and writes the holder's process id into it; a second Acquire() of the
same file, in this process or another, fails at once and HolderPid()
names the process that holds it. The operating system drops the lock
when the holder ends, however it ends (a crash or kill -9 included),
so a stale lock file never blocks a restart: only a live holder does.
The lock is not inherited by child processes (close-on-exec). It does
not work on network file systems that do not support locks.

Changes:
==============================================================
11/17/2026: New in DataReel 7.4 (from dougg's app_framework and
is_file_locked).
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_PROCLOCK_HPP__
#define __GX_PROCLOCK_HPP__

#include "gxdlcode.h"

#include <string>

namespace datareel {

class GXDLCODE_API gxProcessLock
{
public:
  gxProcessLock() { }
  ~gxProcessLock() { Release(); }
  gxProcessLock(const gxProcessLock &) = delete;
  gxProcessLock &operator=(const gxProcessLock &) = delete;

public:
  // the lock on FNAME (the file is made if needed): true if this process holds it now; false if another holder has
  // it (HolderPid() then names it) or the file cannot be opened (Error())
  bool Acquire(const std::string &fname);
  // the lock given up (the file stays, emptied: a later Acquire() takes it at once)
  void Release();
  bool Held() const { return held; }
  const std::string &Error() const { return error; }
  // the process id written by the holder, or 0
  long long HolderPid() const { return holder; }
  // without taking it: true if a live process holds the lock on FNAME (PID: its id, if it wrote one)
  static bool IsLocked(const std::string &fname, long long *pid = 0);

private:
#if defined (_WIN32)
  void *h = 0;                         // HANDLE
#else
  int fd = -1;
#endif
  bool held = false;
  long long holder = 0;
  std::string error, name;
};

} // namespace datareel

#endif // __GX_PROCLOCK_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
