// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
// Date Last Modified: 07/17/2016
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

int num_seg_violations = 0;

void *ConsoleThread::ThreadEntryRoutine(gxThread_t *thread)
{
  const int cmd_len = 255;
  char sbuf[cmd_len];
  
  while(1) {
    memset(sbuf, 0, 255);
    cin >> sbuf;
    if(strcmp(sbuf, "quit") == 0) break;
    if(strcmp(sbuf, "exit") == 0) break;
    cout << "Invalid command" << "\n" << flush;
    cout << "Enter quit to exit" << "\n" << flush;
  }
  return (void *)0;
}

void signal_handler_IO(int status)
// Signal handler. Sets wait_flag to 0, to indicate the above loop that  
// characters have been received. 
{
  // Add all signal I/O calls and variables here
}

int StopProc() 
{
  if(servercfg->debug && servercfg->log_level == 0) servercfg->log_level = 1;
  servercfg->server_retry = 0;
  LogProcMessage("Disconnecting any open connections");
  // Reset any global control variables here
  servercfg->echo_loop = 0;
  servercfg->accept_clients = 0;

  unsigned num_threads = 0;
  gxString message;
  ConsoleThread t;

  LogProcMessage("Closing all open client request threads");
  if(!servercfg->client_request_pool->IsEmpty()) {
    thrPoolNode *ptr = servercfg->client_request_pool->GetHead();
    while(ptr) {
      gxThread_t *thread = ptr->GetThreadPtr();
      if(thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
	num_threads++;
	t.CancelThread(thread);
	LogProcMessage("Shutting down client thread");
	t.JoinThread(thread);
	if(thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	  LogProcMessage("Client thread was canceled");
	}
	else {
	  LogProcMessage("Client thread was not canceled");
	  return 0;
	}
      }
      ptr = ptr->GetNext();
    } 
    message << clear << "Found " << num_threads << " working client threads";
    LogProcMessage(message.c_str());
    sSleep(1); // Allow I/O recovery time
    t.DestroyThreadPool(servercfg->client_request_pool);
    servercfg->client_request_pool = 0;
  }
  else {
    LogProcMessage("No open connections");
  }

  if(servercfg->console_thread) {
    if(servercfg->console_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      t.CancelThread(servercfg->console_thread);
      t.JoinThread(servercfg->console_thread);
      if(servercfg->console_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	LogProcMessage("LB console thread was stopped");
      }
      else {
	LogProcMessage("LB console thread did not shutdown properly");
	return 0;
      }
    }
  }

  if(servercfg->server_thread) { 
    if(servercfg->server_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      LogProcMessage("Stopping LB server thread");
      t.CancelThread(servercfg->server_thread);
      sSleep(5); // Allow I/O recovery time
      t.JoinThread(servercfg->server_thread);
      if(servercfg->server_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	LogProcMessage("LB server thread was stopped");
      }
      else {
	LogProcMessage("LB server thread did not shutdown properly");
	return 0;
      }
    }
  }

  // Stop the log thread last
  if(servercfg->log_thread) { 
    LogThread log;
    if(servercfg->log_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      NT_print("Stopping log thread");
      log.CancelThread(servercfg->log_thread);
      sSleep(1);
      log.JoinThread(servercfg->log_thread);
      if(servercfg->log_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	NT_print("Log thread was stopped");
      }
      else {
	NT_print("LB server thread did not shutdown properly");
	return 0;
      }
    }
    else {
      NT_print("Log thread was stopped");
    }
  }

  NT_print("Sending termination signal to end process");
  sSleep(1);

  return 1;
}

void ExitProc()
{
  NT_print("Exiting process...");
  exit(1);
}

void termination_handler(int signum)
// UNIX signal hander used to handle process termination signals  
{
  sigset_t mask_set;
  sigset_t old_set;
  int retries = 3;
  gxString sbuf;

  if(signum == SIGSEGV) {
    signal(SIGSEGV, termination_handler); // Reset the signal handler again
    sigfillset(&mask_set); // Make any further signals while in handler
    sigprocmask(SIG_SETMASK, &mask_set, &old_set); 
    LogProcMessage("Process received segmentation violation");
    num_seg_violations++;
    if(num_seg_violations > 3) {
      while(!StopProc() && --retries) sSleep(1);
      ExitProc();
    }
    sigprocmask(SIG_SETMASK, &old_set, NULL); // Restore the old signal mask2
    return;
  }
  if(signum == SIGBUS) {
    signal(SIGBUS, termination_handler);
    sigfillset(&mask_set);
    sigprocmask(SIG_SETMASK, &mask_set, &old_set); 
    LogProcMessage("Process received bus violation");
    num_seg_violations++;
    if(num_seg_violations > 3) {
      while(!StopProc() && --retries) sSleep(1);
    }
    sigprocmask(SIG_SETMASK, &old_set, NULL);
    ExitProc();
  }
  if(signum == SIGINT) {
    signal(SIGINT, SIG_IGN); // Log first and ignore all others
    sigfillset(&mask_set);
    sigprocmask(SIG_SETMASK, &mask_set, &old_set); 
    LogProcMessage("Process interrupted with Ctrl-C");
    while(!StopProc() && --retries) sSleep(1);
    sigprocmask(SIG_SETMASK, &old_set, NULL);
    ExitProc();
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
    ExitProc();
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
    ExitProc();
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

void SpwanChildProcess(const char *command, const char *switches, const char *arg) {
  gxString sbuf;
  sbuf << clear << command << " " << switches << " " << arg;
  LogProcMessage("Starting child process");
  LogProcMessage(sbuf.c_str());
  if(execlp(command, command, switches, arg, (char *)0) < 0) {
    if(servercfg->debug && servercfg->debug_level == 5) perror("execlp");
    LogProcMessage("Child process failed");
    servercfg->fatal_error = 1;
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
