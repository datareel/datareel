// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
// Date Last Modified: 09/17/2016
// Copyright (c) 2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description and Details ------------- // 
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

Process and signal functions for Datareel load balancer.

*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "drlb_server.h"
#include "m_proc.h"
#include "m_log.h"

void *ProcessThread::ThreadEntryRoutine(gxThread_t *thread)
{
  while(servercfg->process_loop) { 
    if(servercfg->kill_server) break;
    if(servercfg->restart_server) break;

    servercfg->process_is_locked = 1;
    if(servercfg->process_lock.MutexLock() != 0) {
      LogProcMessage("ERROR - Process thread could not obtain a mutex lock");
      servercfg->process_is_locked = 0;
      break;
    }
    // Block this thread from its own execution 
    servercfg->process_cond.ConditionWait(&servercfg->process_lock);
    servercfg->process_lock.MutexUnlock();
    servercfg->process_is_locked = 0;

    if(servercfg->accept_clients == 0) {
      // Server thread has stopped, end the process thread here
      break;
    }
  }
  sSleep(1); // Allow for I/O recovery time before exiting the program
  return (void *)0;
}

void signal_handler_IO(int status)
// Signal handler. Sets wait_flag to 0, to indicate the above loop that  
// characters have been received. 
{
  // Add all signal I/O calls and variables here
}

int StopThreads()
{
  if(servercfg->debug) LogProcMessage("Entering stop threads() function");

  unsigned num_threads = 0;
  gxString message;
  LBServerThread t;
  int error_level = 0;

  if(servercfg->debug) LogProcMessage("Checking client request pool status");
  if(servercfg->client_request_pool) {
    if(!servercfg->client_request_pool->IsEmpty()) {
      LogProcMessage("Closing all open client request threads");
      thrPoolNode *ptr = servercfg->client_request_pool->GetHead();
      while(ptr) {
	gxThread_t *thread = ptr->GetThreadPtr();
	if(thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
	  num_threads++;
	  t.CancelThread(thread);
	  LogProcMessage("Shutting down client thread");
	  if(thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
	    LogProcMessage("Client thread was not canceled");
	    error_level++;
	  }
	  else {
	    LogProcMessage("Client thread was canceled");
	  }
	}
	ptr = ptr->GetNext();
      } 
      message << clear << "Found " << num_threads << " working client threads";
      LogProcMessage(message.c_str());
      sSleep(1); // Allow all threads to finish clean up handler calls
      t.DestroyThreadPool(servercfg->client_request_pool, 0);
      servercfg->client_request_pool = 0;
    }
    else {
      LogProcMessage("No open connections");
    }
  }

  if(error_level != 0) return error_level;

  if(servercfg->debug) LogProcMessage("Checking LB server thread status");
  if(servercfg->server_thread) { 
    if(servercfg->server_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      LogProcMessage("Stopping LB server thread");
      t.CancelThread(servercfg->server_thread);
      sSleep(5); // Allow I/O recovery time
      t.JoinThread(servercfg->server_thread);
      if(servercfg->server_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	LogProcMessage("LB server thread was stopped");
	delete servercfg->server_thread;
	servercfg->server_thread = 0;
      }
      else {
	LogProcMessage("LB server thread did not shutdown properly");
	error_level++;
      }
    }
    else {
      LogProcMessage("LB Server thread not running");
    }
  }

  if(error_level != 0) return error_level;

  if(servercfg->debug) LogProcMessage("Checking stats thread status");
  if(servercfg->stats_thread) { 
    StatsThread stats;
    if(servercfg->stats_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      LogProcMessage("Stopping stats thread");
      stats.CancelThread(servercfg->stats_thread);
      sSleep(1);
      stats.JoinThread(servercfg->stats_thread);
      if(servercfg->stats_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	LogProcMessage("Stats thread was stopped");
	delete servercfg->stats_thread;
	servercfg->stats_thread = 0;
      }
      else {
	LogProcMessage("LB stats thread did not shutdown properly");
	error_level++;
      }
    }
    else {
      LogProcMessage("LB Stats thread not running");
    }
  }

  if(error_level != 0) return error_level;
  
  if(servercfg->debug) LogProcMessage("Checking log thread status");
  if(servercfg->log_thread) { 
    LogThread log;
    if(servercfg->log_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      LogProcMessage("Stopping log thread");
      log.CancelThread(servercfg->log_thread);
      sSleep(1);
      log.JoinThread(servercfg->log_thread);
      if(servercfg->log_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	LogProcMessage("Log thread was stopped");
	delete servercfg->log_thread;
	servercfg->log_thread = 0;
      }
      else {
	LogProcMessage("LB log thread did not shutdown properly");
	error_level++;
      }
    }
    else {
      LogProcMessage("LB Log thread not running");
    }
  }
  
  if(servercfg->debug) LogProcMessage("Stop threads() call complete");
  return error_level;
}

int StopProc() 
{
  if(servercfg->debug && servercfg->log_level == 0) servercfg->log_level = 1;
  servercfg->accept_clients = 0;
  servercfg->kill_server = 1;
  servercfg->process_loop = 0;
  servercfg->process_cond.ConditionSignal(); 

  int retries = 3;
  while(StopThreads() != 0 && retries--) sleep(1);
 
  LogThread log_t;
  log_t.flush_all_logs();
  if(servercfg->logfile) servercfg->logfile.Close();
  if(servercfg->stats_file) servercfg->stats_file.Close();

  NT_print("Sending termination signal to end process");
  sSleep(1);

  return 1;
}

void ExitProc(int return_code)
{
  NT_print("Exiting DRLB process...");
  exit(return_code);
}

void termination_handler(int signum)
// UNIX signal hander used to handle process termination signals  
{
  sigset_t mask_set;
  sigset_t old_set;
  gxString sbuf;
  int retries = 3;

#ifndef __DEBUG__
  if(signum == SIGSEGV) {
    signal(SIGSEGV, termination_handler); // Reset the signal handler again
    sigfillset(&mask_set); // Make any further signals while in handler
    sigprocmask(SIG_SETMASK, &mask_set, &old_set); 
    LogProcMessage("Process received segmentation violation");
    StopProc();
    sigprocmask(SIG_SETMASK, &old_set, NULL); // Restore the old signal mask2
    ExitProc(1);
    return;
  }

  if(signum == SIGBUS) {
    signal(SIGBUS, termination_handler);
    sigfillset(&mask_set);
    sigprocmask(SIG_SETMASK, &mask_set, &old_set); 
    LogProcMessage("Process received bus violation");
    StopProc();
    sigprocmask(SIG_SETMASK, &old_set, NULL);
    ExitProc(1);
  }
#endif 

  if(signum == SIGINT) {
    signal(SIGINT, SIG_IGN); // Log first and ignore all others
    sigfillset(&mask_set);
    sigprocmask(SIG_SETMASK, &mask_set, &old_set); 
    LogProcMessage("Process interrupted with Ctrl-C");
    while(!StopProc() && --retries) sSleep(1);
    sigprocmask(SIG_SETMASK, &old_set, NULL);
    if(!retries) ExitProc(1);
    ExitProc(0);
  }
  if(signum == SIGQUIT) {
    signal(SIGQUIT, SIG_IGN);
    sigfillset(&mask_set);
    sigprocmask(SIG_SETMASK, &mask_set, &old_set); 
    LogProcMessage("Process interrupted Ctrl-backslash");
    sigprocmask(SIG_SETMASK, &old_set, NULL);
    return;
  }
  if(signum == SIGTERM) {
    signal(SIGTERM, termination_handler);
    sigfillset(&mask_set);
    sigprocmask(SIG_SETMASK, &mask_set, &old_set); 
    LogProcMessage("Process terminated by kill command");
    while(!StopProc() && --retries) sSleep(1);
    sigprocmask(SIG_SETMASK, &old_set, NULL);
    if(!retries) ExitProc(1);
    ExitProc(0);
  }
  if(signum == SIGHUP) {
    signal(SIGHUP, termination_handler);
    sigfillset(&mask_set);
    sigprocmask(SIG_SETMASK, &mask_set, &old_set); 
    LogProcMessage("Process received hangup");
    sigprocmask(SIG_SETMASK, &old_set, NULL);
    return;
  }
  if(signum == SIGKILL) {
    signal(SIGKILL, termination_handler);
    sigfillset(&mask_set);
    sigprocmask(SIG_SETMASK, &mask_set, &old_set); 
    LogProcMessage("Process terminated by SIGKILL");
    while(!StopProc() && --retries) sSleep(1);
    sigprocmask(SIG_SETMASK, &old_set, NULL);
    if(!retries) ExitProc(1);
    ExitProc(0);
  }
  if(signum == SIGTSTP) {
    signal(SIGTSTP, termination_handler);
    sigfillset(&mask_set);
    sigprocmask(SIG_SETMASK, &mask_set, &old_set); 
    LogProcMessage("Process received suspend from Ctrl-Z");
    sigprocmask(SIG_SETMASK, &old_set, NULL);
    return;
  }
  if(signum == SIGABRT) {
    signal(SIGABRT, termination_handler);
    sigfillset(&mask_set);
    sigprocmask(SIG_SETMASK, &mask_set, &old_set); 
    LogProcMessage("Process received SIGABRT");
    sigprocmask(SIG_SETMASK, &old_set, NULL);
    return;
  }
  if(signum == SIGUSR1) {
    signal(SIGUSR1, termination_handler);
    sigfillset(&mask_set);
    sigprocmask(SIG_SETMASK, &mask_set, &old_set); 
    LogProcMessage("Process received SIGUSR1, lowering debug level");
    
    if(!servercfg->debug) LogProcMessage("Debug mode is disabled");

    if(servercfg->debug && servercfg->debug_level > 0) {
      servercfg->debug_level--;
      if(servercfg->debug_level <= 0) {
	LogProcMessage("Debug mode has been disabled");
	servercfg->debug = 0;
	servercfg->debug_level = 0;
      }
      else {
	sbuf << clear << "Debug level decresed to " << servercfg->debug_level;
	LogProcMessage(sbuf.c_str());
      }
    }
    sigprocmask(SIG_SETMASK, &old_set, NULL);
    return;
  }
  if(signum == SIGUSR2) {
    signal(SIGUSR2, termination_handler);
    sigfillset(&mask_set);
    sigprocmask(SIG_SETMASK, &mask_set, &old_set); 
    LogProcMessage("Process received SIGUSR2, raising debug level");

    if(servercfg->debug) {
      LogProcMessage("Debug mode is enabled");
    }

    if(!servercfg->debug) {
      LogProcMessage("Debug mode has been enabled");
      servercfg->debug = 1;
      servercfg->debug_level = 1;
    }

    if(servercfg->debug && servercfg->debug_level >= 1) {
      servercfg->debug_level++;
      if(servercfg->debug_level >= 5) {
	LogProcMessage("Debug mode set to level 5");
	servercfg->debug = 1;
	servercfg->debug_level = 5;
      }
      else {
	sbuf << clear << "Debug level increased to " << servercfg->debug_level;
	LogProcMessage(sbuf.c_str());
      }
    }
    sigprocmask(SIG_SETMASK, &old_set, NULL);
    return;
  }
}

// Pauses for a specified number of seconds. 
void sSleep(unsigned long seconds)
{
  gxMutex t_mutex;
  gxCondition t_condition;
  t_mutex.MutexLock();
  while(1) {
    t_condition.ConditionTimedWait(&t_mutex, seconds);
    break;
  }
}

// Pauses for a specified number of nanoseconds. 
void nSleep(unsigned long nanoseconds)
{
  gxMutex t_mutex;
  gxCondition t_condition;
  t_mutex.MutexLock();
  while(1) {
    t_condition.ConditionTimedWait(&t_mutex, 0, nanoseconds);
    break;
  }
}

// Pauses for a specified number of microseconds. 
void uSleep(unsigned long microseconds)
{
  // Convert microseconds to nanoseconds
  unsigned long nseconds = microseconds * 1000; 
  nSleep(nseconds);
}

// Pauses for a specified number of milliseconds. 
void mSleep(unsigned long  milliseconds)
{
  // Convert millisecond to nanoseconds
  unsigned long nseconds = milliseconds * (1000 * 1000); 
  nSleep(nseconds);
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
