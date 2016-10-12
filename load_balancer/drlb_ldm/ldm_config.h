// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
// Date Last Modified: 10/12/2016
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

LDM config for DR load balancer.

*/
// ----------------------------------------------------------- //   
#ifndef __LDM_CONFIG_HPP__
#define __LDM_CONFIG_HPP__

#include "gxdlcode.h"

#include "gxstring.h"
#include "gxlist.h"
#include "ldm_rpc_messages.h"
#include "ldm_feed_types.h"
#include "ldm_proto.h"

struct LDMallow {
  LDMallow() { Reset(); }
  ~LDMallow() {  }
  LDMallow(const LDMallow &n) { Copy(n); }
  LDMallow operator=(const LDMallow &n) {
    Copy(n);
    return *this;
  }
  void Copy(const LDMallow &n) {
    if(this == &n) return;
    feedtype = n.feedtype;
    hostIdEre = n.hostIdEre; 
    OK_pattern = n.OK_pattern;
    NOT_pattern = n.NOT_pattern;
  }
  void Reset() {
    feedtype.Clear();
    hostIdEre.Clear();
    OK_pattern.Clear();
    NOT_pattern.Clear();
  }
 
  gxString feedtype;
  gxString hostIdEre;
  gxString OK_pattern;
  gxString NOT_pattern;
};

struct LDMaccept {
  LDMaccept() { Reset(); }
  ~LDMaccept() { }
  LDMaccept(const LDMaccept &n) { Copy(n); }
  LDMaccept operator=(const LDMaccept &n) {
    Copy(n);
    return *this;
  }
  void Copy(const LDMaccept &n) {
    if(this == &n) return;
    feedtype = n.feedtype;
    prodIdEre = n.prodIdEre;
    hostIdEre = n.hostIdEre; 
  }
  void Reset() {
    feedtype.Clear();
    prodIdEre.Clear();
    hostIdEre.Clear();
  }

  gxString feedtype;
  gxString prodIdEre;
  gxString hostIdEre;
};

struct LDMconfig {
  LDMconfig();
  ~LDMconfig();

  gxList<LDMaccept> ldm_accept_list;
  gxList<LDMallow> ldm_allow_list;
  int resolve_ldm_hostnames;
  gxString ldm_etc_dir;
  gxString ldmd_conf_file;
  int enable_frontend_proto_capture;
  int enable_backend_proto_capture;
  gxString proto_capture_dir;
};

// Load the LDM config, return 0 if pass, non-zero if fail
int LoadLDMConfig();

// Helper functions to read the ldmd.config config
// Return 0 if no errors
// Return 1 if fatal error
// Return 2 if bad ldmd.config line
// Return 3 if bad regex
int read_ldm_config(const gxString &fname);
int parse_ldm_config(gxList<gxString> &conf_lines);
int read_ldm_config_file(const gxString &fname, gxList<gxString> &conf_lines, gxList<gxString> &includes_list);

// Returns 0 for a regex match to str
// Returns REG_NOMATCH if no match to str
// Returns other non-zero value on error, with error message in message_buf
int compare_ldm_regex(const gxString &regex_str, const gxString &str, char message_buf[255], int check_case = 1);
int compare_ldm_regex(const gxString &regex_str, const gxString &str, int check_case = 1);

// Remove quotes from regex if not escaped with a back slash 
void fix_regex_quotes(gxString &regex_str);

// Returns 0 for pass
// Return non-zero value for fail, with error str returned in error_message
int ldm_ip_to_hostname(const gxString &ip_str, gxString &hostname_str, gxString &error_message);

// Returns 0 for a regex match to IP address, has_match set to 1
// Returns REG_NOMATCH if no match to IP address, has_match set to 0
// Returns other non-zero value on error, will log message
int compare_ldm_hostere_to_hostip(const gxString &hostIdEre, const gxString &hostip, int seq_num, int &has_match, int check_hostname = 1);

// Return 0 if pass, non-zero if fail
// If has match has_allow_accept == 1
// If no match has_allow_accept == 0
int ldm_check_queue_access(const gxString &accept_allow_feeds, const gxString &req_feeds, int &has_allow_accept, gxString &message);

extern LDMconfig LDMConfigSruct;
extern LDMconfig *ldmcfg;

#endif // __LDM_CONFIG_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
