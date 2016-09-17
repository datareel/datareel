// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
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

Extra socket functions for Datareel load balancer.

*/
// ----------------------------------------------------------- //   
#ifndef __DRLB_SOCKET_HPP__
#define __DRLB_SOCKET_HPP__

#include "gxdlcode.h"
#include "gxstring.h"

// Returns 0 for pass, with hostname set in hostname_str
// Return non-zero value for fail, with error str returned in error_message
int ip_to_hostname(const gxString &ip_str, gxString &hostname_str, gxString &error_message);

// Get string value for errno. Returns the error_number provided by caller
int fd_error_to_string(int error_number, gxString &sbuf);

#endif // __DRLB_SOCKET_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
