// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 07/17/2016
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

Process functions for Datareel cluster manager.

*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "drcm_server.h"
#include "m_proc.h"
#include "m_log.h"

void *ProcessThread::ThreadEntryRoutine(gxThread_t *thread)
{
  servercfg->process_lock.MutexLock();
  while(servercfg->process_loop) { 
    if(servercfg->kill_server) break;
    if(servercfg->restart_server) break;
    // Block this thread from its own execution 
    servercfg->process_cond.ConditionWait(&servercfg->process_lock);
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
  servercfg->accept_clients = 0;
  servercfg->echo_loop = 0;

  unsigned num_threads = 0;
  gxString message;
  ProcessThread t;
  int error_level = 0;

  // If client or non-interactive command stop the console thread
  if(servercfg->console_thread) { 
    LogProcMessage("Stopping CM console_thread");
    if(servercfg->console_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      t.CancelThread(servercfg->console_thread);
      sSleep(1); // Allow I/O recovery time
      t.JoinThread(servercfg->console_thread);
      if(servercfg->console_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	LogProcMessage("CM console_thread was halted");
      }
      else {
	LogProcMessage("CM console_thread did not shutdown properly");
	error_level = 1;
      }
    }
    else {
      LogProcMessage("CM console_thread was stopped");
    }
    if(error_level == 0) {
      delete servercfg->console_thread;
      servercfg->console_thread = 0;
    }
  }
  else {
    LogProcMessage("CM console_thread not active");
  }

  // Stop the CM crons first
  if(servercfg->crontab_thread) { 
    LogProcMessage("Stopping CM crontab_thread");
    if(servercfg->crontab_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      t.CancelThread(servercfg->crontab_thread);
      sSleep(1); // Allow I/O recovery time
      t.JoinThread(servercfg->crontab_thread);
      if(servercfg->crontab_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	LogProcMessage("CM crontab_thread was halted");
      }
      else {
	LogProcMessage("CM crontab_thread did not shutdown properly");
	error_level = 1;
      }
    }
    else {
      LogProcMessage("CM crontab_thread was stopped");
    }
    if(error_level == 0) {
      delete servercfg->crontab_thread;
      servercfg->crontab_thread = 0;
    }
  }
  else {
    LogProcMessage("CM crontab_thread not active");
  }

  // Release all floating IP addresses
  if(servercfg->ipaddr_thread) { 
    LogProcMessage("Stopping CM ipaddr_thread");
    if(servercfg->ipaddr_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      t.CancelThread(servercfg->ipaddr_thread);
      sSleep(1); // Allow I/O recovery time
      t.JoinThread(servercfg->ipaddr_thread);
      if(servercfg->ipaddr_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	LogProcMessage("CM ipaddr_thread was halted");
      }
      else {
	LogProcMessage("CM ipaddr_thread did not shutdown properly");
	error_level = 1;
      }
    }
    else {
      LogProcMessage("CM ipaddr_thread was stopped");
    }
    if(error_level == 0) {
      delete servercfg->ipaddr_thread;
      servercfg->ipaddr_thread = 0;
    }
  }
  else {
    LogProcMessage("CM ipaddr_thread not active");
  }

  // The the other nodes to take over
  if(servercfg->keep_alive_thread) { 
    LogProcMessage("Stopping CM UDP keep alive thread");
    if(servercfg->keep_alive_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      t.CancelThread(servercfg->keep_alive_thread);
      sSleep(1); // Allow I/O recovery time
      t.JoinThread(servercfg->keep_alive_thread);
      if(servercfg->keep_alive_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	LogProcMessage("CM UDP keep alive thread was halted");
      }
      else {
	LogProcMessage("CM UDP keep alive thread did not shutdown properly");
	error_level = 1;
      }
    }
    else {
      LogProcMessage("CM UDP keep alive thread was stopped");
    }
    if(error_level == 0) {
      delete servercfg->keep_alive_thread;
      servercfg->keep_alive_thread = 0;
    }
  }
  else {
    LogProcMessage("CM UDP keep alive thread not active");
  }

  if(servercfg->udp_server_thread) { 
    LogProcMessage("Stopping CM UDP server thread");
    if(servercfg->udp_server_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      t.CancelThread(servercfg->udp_server_thread);
      sSleep(1); // Allow I/O recovery time
      t.JoinThread(servercfg->udp_server_thread);
      if(servercfg->udp_server_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	LogProcMessage("CM UDP server thread was halted");
      }
      else {
	LogProcMessage("CM UDP server thread did not shutdown properly");
	error_level = 1;
      }
    }
    else {
      LogProcMessage("CM UDP server thread was stopped");
    }
    if(error_level == 0) {
      delete servercfg->udp_server_thread;
      servercfg->udp_server_thread = 0;
    }
  }
  else {
    LogProcMessage("CM UDP server thread not active");
  }

  if(servercfg->client_request_pool ) {
    if(!servercfg->client_request_pool->IsEmpty()) {
      LogProcMessage("Closing all open TCP client threads");
      thrPoolNode *ptr = servercfg->client_request_pool->GetHead();
      while(ptr) {
	gxThread_t *thread = ptr->GetThreadPtr();
	if(thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
	  num_threads++;
	  t.CancelThread(thread);
	  LogProcMessage("Shutting down TCP client thread");
	  t.JoinThread(thread);
	  if(thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	    LogProcMessage("Client TCP thread was canceled");
	  }
	  else {
	    LogProcMessage("Client TCP thread was not canceled");
	    error_level = 1;
	  }
	}
	ptr = ptr->GetNext();
      } 
      message << clear << "Found " << num_threads << " working TCP client threads";
      LogProcMessage(message.c_str());
      if(error_level == 0) {
	t.DestroyThreadPool(servercfg->client_request_pool);
	servercfg->client_request_pool = 0;
      }
    }
    else {
      LogProcMessage("No open TCP connections");
    }
  }

  if(servercfg->applications_thread) { 
    LogProcMessage("Stopping CM applications_thread");
    if(servercfg->applications_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      t.CancelThread(servercfg->applications_thread);
      sSleep(1); // Allow I/O recovery time
      t.JoinThread(servercfg->applications_thread);
      if(servercfg->applications_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	LogProcMessage("CM applications_thread was halted");
      }
      else {
	LogProcMessage("CM applications_thread did not shutdown properly");
	error_level = 1;
      }
    }
    else {
      LogProcMessage("CM applications_thread was stopped");
    }
    if(error_level == 0) {   
      delete servercfg->applications_thread;
      servercfg->applications_thread = 0;
    }
  }
  else {
    LogProcMessage("CM applications_thread not active");
  }

  if(servercfg->services_thread) { 
    LogProcMessage("Stopping CM services_thread");
    if(servercfg->services_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      t.CancelThread(servercfg->services_thread);
      sSleep(1); // Allow I/O recovery time
      t.JoinThread(servercfg->services_thread);
      if(servercfg->services_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	LogProcMessage("CM services_thread was halted");
      }
      else {
	LogProcMessage("CM services_thread did not shutdown properly");
	error_level = 1;
      }
    }
    else {
      LogProcMessage("CM services_thread was stopped");
    }
    if(error_level == 0) {
      delete servercfg->services_thread;
      servercfg->services_thread = 0;
    }
  }
  else {
    LogProcMessage("CM services_thread not active");
  }

  if(servercfg->filesystems_thread) { 
    LogProcMessage("Stopping CM filesystems_thread");
    if(servercfg->filesystems_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      t.CancelThread(servercfg->filesystems_thread);
      sSleep(1); // Allow I/O recovery time
      t.JoinThread(servercfg->filesystems_thread);
      if(servercfg->filesystems_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	LogProcMessage("CM filesystems_thread was stopped");
      }
      else {
	LogProcMessage("CM filesystems_thread did not shutdown properly");
	error_level = 1;
      }
    }
    else {
      LogProcMessage("CM filesystems_thread was halted");
    }
    if(error_level == 0) {
      delete servercfg->filesystems_thread;
      servercfg->filesystems_thread = 0;
    }
  }
  else {
    LogProcMessage("CM filesystems_thread not active");
  }


  // Stop the log thread last
  if(servercfg->log_thread) { 
    LogProcMessage("Stopping log thread");
    LogThread log;
    if(servercfg->log_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      log.CancelThread(servercfg->log_thread);
      sSleep(1);
      log.JoinThread(servercfg->log_thread);
      if(servercfg->log_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	LogProcMessage("Log thread was halted");
      }
      else {
	LogProcMessage("Log thread did not shutdown properly");
	error_level = 1;
      }
    }
    else {
      LogProcMessage("Log thread was stopped");
    }
    if(error_level == 0) {
      delete servercfg->log_thread;
      servercfg->log_thread = 0;
    }
  }
  else {
    LogProcMessage("Log thread not active");
  }

  LogThread log_t;
  log_t.flush_all_logs();

  return error_level;
}

int StopProc() 
{
  if(servercfg->debug && servercfg->log_level == 0) servercfg->log_level = 1;
  servercfg->server_retry = 0;

  int retries = 3;
  while(StopThreads() != 0 && retries--) sleep(1);
 
  // Signal the process thread to exit
  servercfg->kill_server = 1;
  servercfg->process_loop = 0;
  servercfg->process_cond.ConditionSignal(); 
  servercfg->process_lock.MutexUnlock();

  NT_print("Sending termination signal to end process");
  sSleep(1);

  return 1;
}

void ExitProc(int return_code)
{
  NT_print("Exiting DRCM process...");
  exit(return_code);
}

void termination_handler(int signum)
// UNIX signal hander used to handle process termination signals  
{
  sigset_t mask_set;
  sigset_t old_set;
  int retries = 3;
  gxString sbuf;

#ifndef __DEBUG__
  if(signum == SIGSEGV) {
    signal(SIGSEGV, termination_handler); // Reset the signal handler again
    sigfillset(&mask_set); // Make any further signals while in handler
    sigprocmask(SIG_SETMASK, &mask_set, &old_set); 
    LogProcMessage("Process received segmentation violation");
    StopProc();
    ExitProc(1);
    sigprocmask(SIG_SETMASK, &old_set, NULL); // Restore the old signal mask2
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
  if(t_mutex.MutexLock() != 0) return;
  t_condition.ConditionTimedWait(&t_mutex, seconds);
  t_mutex.MutexUnlock();
}

// Pauses for a specified number of nanoseconds. 
void nSleep(unsigned long nanoseconds)
{
  gxMutex t_mutex;
  gxCondition t_condition;
  if(t_mutex.MutexLock() != 0) return;
  t_condition.ConditionTimedWait(&t_mutex, 0, nanoseconds);
  t_mutex.MutexUnlock();
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

int RunSystemCommand(const gxString &input_command)
{
  gxString superuser = "root";
  gxString command;
  if(servercfg->cluster_user == superuser) {
    command = input_command;
  }
  else {
    command << clear << servercfg->sudo_command << " \'" << input_command << "\'";
  }

  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage(command.c_str());
  }

  return system(command.c_str());
} 

int RunSystemCommand(const gxString &input_command, const gxString &user, const gxString &group)
{
  gxString superuser = "root";
  gxString command;
  if(servercfg->cluster_user == superuser) {
#ifdef __HAS_SU_DASH_G__
    command << clear << "su -g " << group << " -c \'" << input_command << "\' - " << user << " >/dev/null 2>&1";
#else
    command << clear << "su -c \'" << input_command << "\' - " << user << " >/dev/null 2>&1";
#endif
  }
  else {
#ifdef __HAS_SU_DASH_G__
    command << clear << servercfg->sudo_command 
	    << " \"su -g " << group << " -c \'" << input_command << "\' - " << user << "\""; 
#else
    command << clear << servercfg->sudo_command 
	    << " \"su -c \'" << input_command << "\' - " << user << "\""; 
#endif
  }

  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage(command.c_str());
  }

  return system(command.c_str());
} 
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
