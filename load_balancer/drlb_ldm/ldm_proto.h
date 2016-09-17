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

LDM protcol plug in for DR load balancer.

*/
// ----------------------------------------------------------- //   
#ifndef __LDM_PROTO_HPP__
#define __LDM_PROTO_HPP__

#include "gxdlcode.h"
#include "gxstring.h"

#include "ldm_rpc_messages.h"
#include "ldm_feed_types.h"

struct LDMrequest {
  LDMrequest() {
    request = 0;
    feed_type = 0;
  }
  ~LDMrequest() { }
  
  void SetFeedType(char net_int[4]);
  unsigned int network_to_int(char net_int[4]);

  int request;

  gxString request_string;
  // HIYA requires ACCEPT entry, and feedtype entry 
  // FEEDME requires ALLOW entry, and feedtype entry

  unsigned int feed_type;
  gxString feed_type_strings;
  gxString client_ip;
  gxString client_host_names;
};


int set_ldm_request_string(int ldm_request, gxString &s);
int set_ldm_feed_type_strings(unsigned int ldm_feed_type, gxString &s);

#endif // __LDM_PROTO_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
