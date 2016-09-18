// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 07/17/2016
// Date Last Modified: 09/17/2016
// Copyright (c) 2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ---------- Include File Description and Details  ---------- // 
// ----------------------------------------------------------- // 
/*
This file is part of the DataReel software distribution.

Datareel is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your
option) any later version. 

Datareel software is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with the DataReel software distribution.  If not, see
<http://www.gnu.org/licenses/>.

Multi-threaded framework Datareel cluster manager.

*/
// ----------------------------------------------------------- //   
#ifndef __DRCM_MTHREAD_HPP__
#define __DRCM_MTHREAD_HPP__

#include "gxdlcode.h"

#include "gxmutex.h"
#include "gxcond.h"
#include "gxthread.h"
#include "gxsocket.h"
#include "dfileb.h"
#include "devcache.h"
#include "memblock.h"
#include "membuf.h"

#include "drcm_server.h"
#include "m_socket.h"

class CM_UDP_ServerThread : public gxSocket, public gxThread
{
public:
  CM_UDP_ServerThread() { }
  ~CM_UDP_ServerThread() { }

public: // Server functions
  int InitServer();

private: // gxThread Interface
  void *ThreadEntryRoutine(gxThread_t *thread);
  void ThreadExitRoutine(gxThread_t *thread);
  void ThreadCleanupHandler(gxThread_t *thread);
};

class CM_KeepAliveThread : public gxSocket, public gxThread
{
public:
  CM_KeepAliveThread() { }
  ~CM_KeepAliveThread() { }

private: // gxThread Interface
  void *ThreadEntryRoutine(gxThread_t *thread);
  void ThreadExitRoutine(gxThread_t *thread);
  void ThreadCleanupHandler(gxThread_t *thread);
};

class CM_CrontabsThread : public gxSocket, public gxThread
{
public:
  CM_CrontabsThread() { 
    node_is_set = 0;
  }
  ~CM_CrontabsThread() { }

  int set_this_node();
  int install_crontabs();
  int remove_all_crontabs(); // Removes all ctabs in global config

private: // gxThread Interface
  void *ThreadEntryRoutine(gxThread_t *thread);
  void ThreadExitRoutine(gxThread_t *thread);
  void ThreadCleanupHandler(gxThread_t *thread);

private:
  CMnode node; // This node
  int node_is_set;
  gxList<gxString> installed_crontabs;
  gxList<gxString> installed_backup_crontabs;
};

class CM_IPaddrsThread : public gxSocket, public gxThread
{
public:
  CM_IPaddrsThread() { 
    node_is_set = 0;
  }
  ~CM_IPaddrsThread() { }

  int set_this_node();
  int install_floating_ipaddrs();
  int remove_all_ipaddrs();

private: // gxThread Interface
  void *ThreadEntryRoutine(gxThread_t *thread);
  void ThreadExitRoutine(gxThread_t *thread);
  void ThreadCleanupHandler(gxThread_t *thread);

private:
  CMnode node; // This node
  int node_is_set;
  gxList<gxString> installed_floating_ipaddrs;
  gxList<gxString> installed_backup_floating_ipaddrs;
};

class CM_servicesThread : public gxSocket, public gxThread
{
public:
  CM_servicesThread() { 
    node_is_set = 0;
  }
  ~CM_servicesThread() { }

  int set_this_node();
  int install_services();
  int stop_all_services();
  int stop_backup_services();

private: // gxThread Interface
  void *ThreadEntryRoutine(gxThread_t *thread);
  void ThreadExitRoutine(gxThread_t *thread);
  void ThreadCleanupHandler(gxThread_t *thread);

private:
  CMnode node; // This node
  int node_is_set;
  gxList<gxString> installed_services;
  gxList<gxString> installed_backup_services;
};

class CM_applicationsThread : public gxSocket, public gxThread
{
public:
  CM_applicationsThread() { 
    node_is_set = 0;
  }
  ~CM_applicationsThread() { }

  int set_this_node();
  int stop_all_applications();
  int stop_backup_applications();
  int install_applications();

private: // gxThread Interface
  void *ThreadEntryRoutine(gxThread_t *thread);
  void ThreadExitRoutine(gxThread_t *thread);
  void ThreadCleanupHandler(gxThread_t *thread);

private:
  CMnode node; // This node
  int node_is_set;
  gxList<gxString> installed_applications;
  gxList<gxString> installed_backup_applications;
};

class CM_filesystemsThread : public gxSocket, public gxThread
{
public:
  CM_filesystemsThread() { 
    node_is_set = 0;
  }
  ~CM_filesystemsThread() { }

  int set_this_node();
  int mount_filesystems();
  int unmount_all_filesystems();
  int unmount_backup_filesystems();

private:
  void *ThreadEntryRoutine(gxThread_t *thread);
  void ThreadExitRoutine(gxThread_t *thread);
  void ThreadCleanupHandler(gxThread_t *thread);

private:
  CMnode node;
  int node_is_set;
  gxList<gxString> installed_filesystems;
  gxList<gxString> installed_backup_filesystems;
};

#endif // __DRCM_MTHREAD_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
