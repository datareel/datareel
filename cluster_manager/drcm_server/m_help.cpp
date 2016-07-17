// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 07/17/2016
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

Help functions for Datareel cluster manager.

*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "drcm_server.h"
#include "m_help.h"

// Version number, program  and publisher info
const char *VersionString = "1.01";
const char *ProgramDescription = "Datareel Cluster Manager";
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
