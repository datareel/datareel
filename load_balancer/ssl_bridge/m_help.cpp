// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
// Date Last Modified: 10/18/2016
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

Help functions for Datareel SSL load balancer.

*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "drlb_server.h"
#include "m_help.h"

// Version number, program  and publisher info
const char *VersionString = "1.58";
const char *ProgramDescription = "DR SSL Load Balancer";
const char *ProducedBy = "http://datareel.com";

const char *GetVersionString()
{
  return VersionString;
}

const char *GetProgramDescription()
{
  return ProgramDescription;
}

const char *GetProducedBy()
{
  return ProducedBy;
}

void HelpMessage(const char *ProgramName)
{
  VersionMessage();
  cout << "Usage: " << ProgramName << " [args]" << "\n" << flush;
  cout << "args: " << "\n" << flush;
  cout << "     --help                      Print help message and exit" << "\n" << flush;
  cout << "     --version                   Print version number and exit" << "\n" << flush;
  cout << "     --check-config              Check the LB config file and exit" << "\n" << flush;
  cout << "     --debug                     Enable debug mode default level --debug=1" << "\n" << flush;
  cout << "     --debug-level=1             Set debugging level, 1-5" << "\n" << flush;
  cout << "     --verbose                   Turn on verbose messaging to console" << "\n" << flush;
  cout << "     --verbose-level=1           Set verbose level, 0-5" << "\n" << flush;
  cout << "     --config-file=\"config.cfg\"  Specify configuration CFG file location" << "\n" << flush;
  cout << "     --log-file=\"logfile.log\"    Use log file to log program output" << "\n" << flush;
  cout << "     --log-level=1               Set logging level, 0-5" << "\n" << flush;
  cout << "     --log-file-clear            Clear the log file when program starts" << "\n" << flush;
  cout << "     --log-disable               Disable logging, override cfg setting" << "\n" << flush;
  cout << "     --stats-file=\"stats.log\"    Write server stats to specified file " << "\n" << flush;
  cout << "     --stats-disable             Disable server stats, override cfg setting" << "\n" << flush;
  cout << "\n" << flush;
}

void VersionMessage()
{
  cout << "\n" << flush;
  cout << ProgramDescription << " version " << VersionString  << "\n" 
	      << flush;
  cout << "Produced by: " << ProducedBy << "\n" << flush;
  cout << "\n" << flush;
}

// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
