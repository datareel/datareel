// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
// Date Last Modified: 10/12/2016
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

LDM config for DR load balancer.

*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <regex.h>

#include "../drlb_server/drlb_server.h"
#include "ldm_config.h"

LDMconfig LDMConfigSruct;
LDMconfig *ldmcfg = &LDMConfigSruct;

// Internal parser vars
const size_t ldm_line_len = 8192; // Allow really long lines
const size_t ldm_num_key_words = 5;
const char *ldm_key_words[ldm_num_key_words] = { "EXEC", "REQUEST", "ALLOW", "ACCEPT", "INCLUDE" };

LDMconfig::LDMconfig() 
{ 
  ldm_etc_dir = "/usr/local/ldm/etc";
  ldmd_conf_file = "ldmd.conf";
  resolve_ldm_hostnames = 1;
  enable_frontend_proto_capture = 0;
  enable_backend_proto_capture = 0;
  proto_capture_dir = "/tmp/ldm_proto_capture";
}

LDMconfig::~LDMconfig() 
{ 
  ldm_accept_list.ClearList();
  ldm_allow_list.ClearList();
}

int LoadLDMConfig()
{
  NT_print("Loading LDM configuration");
  
  const size_t linelen = 1024;
  char linebuf[linelen];
  DiskFileB ifile;
  ifile.df_Open(servercfg->config_file.c_str());
  if(!ifile) {
    NT_print("ERROR - Cannot open config file for reading", servercfg->config_file.c_str());
    NT_print(ifile.df_ExceptionMessage());
    return 1;
  }

  UString info_line, sbuf, parm_name;
  int error_level = 0;
  gxList<gxString> filelist;
  char pbuf[255];
  int filter_linefeeds = 0;

  while(!ifile.df_EOF()) {
    memset(linebuf, 0, linelen);
    ifile.df_GetLine(linebuf, linelen, '\n', filter_linefeeds);
    if(ifile.df_GetError() != DiskFileB::df_NO_ERROR) {
      NT_print("ERROR - A fatal I/O error reading", servercfg->config_file.c_str());
      NT_print(ifile.df_ExceptionMessage());
      error_level = 1;
      break;
    }
    info_line << clear << linebuf;
    filelist.Add(info_line);
  }
  ifile.df_Close();

  if(error_level != 0) return error_level;

  int has_ldm_config_section = 0;
  gxListNode<gxString> *ptr = filelist.GetHead();
  while(ptr) {
    if(ptr->data.IFind("[LDMCONFIG]") != -1) {
      has_ldm_config_section = 1;
      break;
    }
    ptr = ptr->next;
  }

  if(!has_ldm_config_section) {
    NT_print("INFO - DRLB config file does not have and [LDMCONFIG] section:", servercfg->config_file.c_str());
    NT_print("INFO - Adding [LDMCONFIG] section to DRLB server config:", servercfg->config_file.c_str());
    DiskFileB dfile(servercfg->config_file.c_str(), DiskFileB::df_READWRITE, DiskFileB::df_CREATE); 
    if(!dfile) {
      NT_print("ERROR - Cannot write to config file", servercfg->config_file.c_str());
      return 1;
    }
    dfile << "#" << "\n";
    dfile << "\n";
    dfile << "# DRLB server added LDMCONFIG section to config file" << "\n";
    dfile << "[LDMCONFIG]" << "\n";
    dfile << "# The LDM etc dir is used to find ldmd.conf and all include files" << "\n";
    dfile << "# We must CD to LDM etc dir to read includes with relative paths" << "\n";
    dfile << "ldm_etc_dir = /usr/local/ldm/etc" << "\n";
    dfile << "# Name of your ldmd.conf file with no path" << "\n";
    dfile << "ldmd_conf_file = ldmd.conf" << "\n";
    dfile << "resolve_ldm_hostnames = 1" << "\n";
    dfile << "#" << "\n";
    dfile << "# For development and testing" << "\n";
    dfile << "## enable_frontend_proto_capture = 0" << "\n";
    dfile << "## enable_backend_proto_capture = 0" << "\n";
    dfile << "## proto_capture_dir = /tmp/ldm_proto_capture" << "\n";
    dfile << "#" << "\n";
    dfile << "\n";
    ptr = filelist.GetHead();
    while(ptr) {
      dfile << ptr->data.c_str();
      ptr = ptr->next;
    }
    dfile.df_Close();
  }
  else {
    NT_print("INFO - Reading [LDMCONFIG] config section from DRLB config file:", servercfg->config_file.c_str());
    ptr = filelist.GetHead();
    has_ldm_config_section = 0;
    while(ptr) {
      info_line << clear << ptr->data;
      info_line.FilterChar('\n');    
      info_line.FilterChar('\r');    
      info_line.TrimLeadingSpaces();
      info_line.TrimTrailingSpaces();
      if(info_line[0] == '#') { ptr = ptr->next; continue; } 
      if(info_line[0] == ';') { ptr = ptr->next; continue; } 
      info_line.DeleteAfterIncluding("#");
      info_line.TrimTrailingSpaces();
      if(info_line.is_null()) { ptr = ptr->next; continue; } 
      
      gxString strdup = ptr->data;
      strdup.FilterChar(' ');
      if(strdup.IFind("[LDMCONFIG]") != -1) {
	has_ldm_config_section++;
	ptr = ptr->next;
	while(ptr) {
	  if(ptr->data[0] == '[') {
	    ptr = ptr->prev;
	    break;
	  }
	  info_line << clear << ptr->data;
	  info_line.FilterChar('\n');    
	  info_line.FilterChar('\r');    
	  info_line.TrimLeadingSpaces();
	  info_line.TrimTrailingSpaces();
	  if(info_line[0] == '#') { ptr = ptr->next; continue; } 
	  if(info_line[0] == ';') { ptr = ptr->next; continue; } 
	  info_line.DeleteAfterIncluding("#");
	  info_line.TrimTrailingSpaces();
	  if(info_line.is_null()) { ptr = ptr->next; continue; } 

	  // Add all [LDMCONFIG] reads here
	  if(info_line.IFind("ldm_etc_dir") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ldm_etc_dir") {
	      ldmcfg->ldm_etc_dir = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(info_line.IFind("ldmd_conf_file") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ldmd_conf_file") {
	      ldmcfg->ldmd_conf_file = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(info_line.IFind("resolve_ldm_hostnames") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "resolve_ldm_hostnames") {
	      ldmcfg->resolve_ldm_hostnames = CfgGetTrueFalseValue(info_line);
	    }
	  }
	  if(info_line.IFind("enable_frontend_proto_capture") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "enable_frontend_proto_capture") {
	      ldmcfg->enable_frontend_proto_capture = CfgGetTrueFalseValue(info_line);
	    }
	  }
	  if(info_line.IFind("enable_backend_proto_capture") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "enable_backend_proto_capture") {
	      ldmcfg->enable_backend_proto_capture = CfgGetTrueFalseValue(info_line);
	    }
	  }
	  if(info_line.IFind("proto_capture_dir") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "proto_capture_dir") {
	      ldmcfg->proto_capture_dir = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(!ptr) break;
	  ptr = ptr->next;
	}
      }
      if(!ptr) break;
      ptr = ptr->next;
    }
    if(has_ldm_config_section > 1) {
      sbuf << clear << "ERROR - DRLB server config has " << has_ldm_config_section 
	   << " [LDMCONFIG] sections " << servercfg->config_file;
      NT_print(sbuf.c_str());
      return 1;
    }
  }

  char pwd[2048];
  memset(pwd, 0, 2048);
  gxString ldmd_conf_file;

  if(getcwd(pwd, 2048) == 0) {
    NT_print("ERROR - Cannot get PWD while loading LDM conf file");
    return 1;
  }
  if(chdir(ldmcfg->ldm_etc_dir.c_str()) != 0) {
    NT_print("ERROR - Cannot CD to LDM etc dir:", ldmcfg->ldm_etc_dir.c_str());
    return 1;
  }
  
  sbuf = ldmcfg->ldmd_conf_file;
  if(sbuf.Find("/") != -1) sbuf.DeleteBeforeLastIncluding("/");
  sbuf.FilterChar(' ');
  ldmd_conf_file << clear << ldmcfg->ldm_etc_dir;
  if(ldmd_conf_file[ldmd_conf_file.length()-1] != '/') ldmd_conf_file << "/";
  ldmd_conf_file << sbuf;
  
  error_level = read_ldm_config(ldmd_conf_file);
  switch(error_level) {
    case 0:
      NT_print("INFO - Loaded LDM conf file:", ldmd_conf_file.c_str());
      break;

    case 1:
      NT_print("ERROR - Fatal file error loading LDM conf file:", ldmd_conf_file.c_str());
      break;

    case 2:
      NT_print("ERROR - LDM conf file has bad lines:", ldmd_conf_file.c_str());
      break;
	
    case 3:
	NT_print("ERROR - LDM conf file has bad regex:", ldmd_conf_file.c_str());
      break;

    default:
      NT_print("ERROR - Fatal error loading LDM conf file:", ldmd_conf_file.c_str());
      break;
  }
  
  if(chdir(pwd) != 0) {
    error_level = 1;
    NT_print("ERROR - Cannot CD back to PWD:", pwd);
  }

  return error_level;
}

// Read the ldmd.config config
// Return 0 if no errors
// Return 1 if fatal error
// Return 2 if bad ldmd.config line
// Return 3 if bad regex
int read_ldm_config(const gxString &fname)
{
  int error_level = 0;
  gxList<gxString> conf_lines;
  gxList<gxString> includes_list;
  gxList<gxString> nested_includes_list;

  // Read the ldmd.conf file and look for includes
  error_level = read_ldm_config_file(fname, conf_lines, includes_list);
  if(error_level != 0) return error_level;

  // Read all include files and check for nested include files
  gxListNode<gxString> *lptr = includes_list.GetHead();
  while(lptr) {
    nested_includes_list.ClearList();
    error_level = read_ldm_config_file(lptr->data, conf_lines, nested_includes_list);
    if(error_level != 0) return error_level;
    lptr = lptr->next;
    if(!lptr) {
      includes_list.ClearList();
      gxListNode<gxString> *nptr = nested_includes_list.GetHead();
      while(nptr) {
	includes_list.Add(nptr->data);
	nptr = nptr->next;
      }
      lptr = includes_list.GetHead();
    }
  }

  // Compile all the config file lines into allow and accept lists
  error_level = parse_ldm_config(conf_lines);
  if(error_level != 0) return error_level;

  return error_level;
}

int parse_ldm_config(gxList<gxString> &conf_lines)
{
  int error_level = 0;
  int has_key_word = 0;
  int has_line = 0;
  gxString line;
  int is_accept = 0;
  int is_allow = 0;
  size_t num;
  unsigned num_arr;
  gxString *vals = 0; 
  gxString delimiter = "\t";
  regex_t regex;
  int reti;

  gxListNode<gxString> *lptr = conf_lines.GetHead();
  while(lptr) {
    has_line = 0;
    has_key_word = 0;
    is_accept = 0;
    is_allow = 0;
    
    gxString kw(lptr->data);
    kw.DeleteAfterIncluding(" "); kw.DeleteAfterIncluding("\t");
    kw.ToUpper();
    for(num = 0; num < ldm_num_key_words; num++) {
      if(kw == ldm_key_words[num]) {
	has_key_word = 1;
	line = lptr->data;
	if(kw == "ACCEPT") { is_accept = 1; is_allow = 0; }
	if(kw == "ALLOW") { is_allow = 1; is_accept = 0; }
	break;
      }
    }
    if(has_key_word) {
      if(lptr->next) {
	gxString next_kw(lptr->next->data);
	next_kw.DeleteAfterIncluding(" "); next_kw.DeleteAfterIncluding("\t");
	next_kw.ToUpper();
	for(num = 0; num < ldm_num_key_words; num++) {
	  if(next_kw == ldm_key_words[num]) {
	    has_line = 1;
	    break;
	  }
	}
      }
      else {
	has_line = 1;
      }
    }
    while(!has_line) {
      lptr = lptr->next;
      if(lptr) {
	gxString kwbuf(lptr->data);
	kwbuf.DeleteAfterIncluding(" "); kwbuf.DeleteAfterIncluding("\t");
	kwbuf.ToUpper();
	for(num = 0; num < ldm_num_key_words; num++) {
	  if(kwbuf == ldm_key_words[num]) {
	    has_line = 1;
	    lptr = lptr->prev; // Reset to previous config line
	    break;
	  }
	}
	// Keep appending until we have the whole line
	if(!has_line) line << "\t" << lptr->data;
      }
      else {
	has_line = 1;
	break;
      }
    }

    if(is_accept) {
      while(line.Find("  ") != -1) line.ReplaceString("  ", " ");
      line.ReplaceChar(' ', '\t');
      while(line.Find("\t\t") != -1) line.ReplaceString("\t\t", "\t");
      line.TrimLeading('\t'); line.TrimTrailing('\t');
      if(servercfg->debug && servercfg->debug_level >= 5) {
	NT_print("Compiling:", line.c_str());
      }
      num_arr = 0;
      vals = ParseStrings(line, delimiter, num_arr);
      if(num_arr != 4) {
	NT_print("ERROR - LDM conf ACCEPT line error, too many or to few args:", line.c_str());
	error_level = 2;
      }
      else {
	LDMaccept ldm_accept;
	ldm_accept.Reset();
	ldm_accept.feedtype = vals[1];
	ldm_accept.prodIdEre = vals[2];	fix_regex_quotes(ldm_accept.prodIdEre);
	ldm_accept.hostIdEre = vals[3]; fix_regex_quotes(ldm_accept.hostIdEre);
	reti = regcomp(&regex, ldm_accept.prodIdEre.c_str(), REG_EXTENDED|REG_NOSUB);
	if(reti) {
	  NT_print("ERROR - LDM conf ACCEPT bad product ID REGEX:", line.c_str());
	  error_level = 3;
	}
	regfree(&regex);
	reti = regcomp(&regex, ldm_accept.hostIdEre.c_str(), REG_EXTENDED|REG_NOSUB);
	if(reti) {
	  NT_print("ERROR - LDM conf ACCEPT bad host ID REGEX:", line.c_str());
	  error_level = 3;
	}
	regfree(&regex);
	ldmcfg->ldm_accept_list.Add(ldm_accept);
      }
      if(vals) { delete[] vals; vals = 0; }
    }
    if(is_allow) {
      while(line.Find("  ") != -1) line.ReplaceString("  ", " ");
      line.ReplaceChar(' ', '\t');
      while(line.Find("\t\t") != -1) line.ReplaceString("\t\t", "\t");
      line.TrimLeading('\t'); line.TrimTrailing('\t');
      if(servercfg->debug && servercfg->debug_level >= 5) {
	NT_print("Compiling:", line.c_str());
      }
      num_arr = 0;
      vals = ParseStrings(line, delimiter, num_arr);
      if(num_arr < 3 || num_arr > 5) {
	if(num_arr < 3)	{
	  NT_print("ERROR - LDM conf ALLOW line error, too few args:", line.c_str());
	}
	else {
	  NT_print("ERROR - LDM conf ALLOW line error, too many args:", line.c_str());
	}
	error_level = 2;
      }
      else {
	LDMallow ldm_allow;
	ldm_allow.Reset();
	ldm_allow.feedtype = vals[1];
	ldm_allow.hostIdEre = vals[2]; fix_regex_quotes(ldm_allow.hostIdEre);
	reti = regcomp(&regex, ldm_allow.hostIdEre.c_str(), REG_EXTENDED|REG_NOSUB);
	if(reti) {
	  NT_print("ERROR - LDM conf ALLOW bad host ID REGEX:", line.c_str());
	  error_level = 3;
	}
	regfree(&regex);
	ldm_allow.OK_pattern.Clear();
	ldm_allow.NOT_pattern.Clear();
	if(num_arr >= 4) {
	  if(!vals[3].is_null()) {
	    ldm_allow.OK_pattern = vals[3]; fix_regex_quotes(ldm_allow.OK_pattern);
	    reti = regcomp(&regex, ldm_allow.OK_pattern.c_str(), REG_EXTENDED|REG_NOSUB);
	    if(reti) {
	      NT_print("ERROR - LDM conf ALLOW bad OK pattern REGEX:", line.c_str());
	      error_level = 3;
	    }
	    regfree(&regex);
	  }
	}
	if(num_arr == 5) {
	  if(!vals[3].is_null()) {
	    if(!vals[4].is_null()) {
	      ldm_allow.NOT_pattern = vals[4]; fix_regex_quotes(ldm_allow.NOT_pattern);
	      reti = regcomp(&regex, ldm_allow.NOT_pattern.c_str(), REG_EXTENDED|REG_NOSUB);
	      if(reti) {
		NT_print("ERROR - LDM conf ALLOW bad NOT pattern REGEX:", line.c_str());
		error_level = 3;
	      }
	      regfree(&regex);
	    }
	  }
	}
	ldmcfg->ldm_allow_list.Add(ldm_allow);
      }
      if(vals) { delete[] vals; vals = 0; }
    }
    lptr = lptr->next;
  }

  return error_level;
}

void fix_regex_quotes(gxString &regex_str)
{
  regex_str.TrimLeading('"');
  if((regex_str[regex_str.length()-1] == '"') &&
     (regex_str[regex_str.length()-2] != '\\')) {
    regex_str.TrimTrailing('"');
  }
  regex_str.TrimLeading('\'');
  if((regex_str[regex_str.length()-1] == '\'') &&
     (regex_str[regex_str.length()-2] != '\\')) {
    regex_str.TrimTrailing('\'');
  }
}

int read_ldm_config_file(const gxString &fname, gxList<gxString> &conf_lines, gxList<gxString> &includes_list)
{
  DiskFileB cfgfile;
  int error_level = 0;
  char lbuf[ldm_line_len];
  gxString info_line;
  gxString message;

  cfgfile.df_Open(fname.c_str());
  if(!cfgfile) {
    message << clear << "Cannot open LDMD include conf file " << fname.c_str();
    NT_print(message.c_str());
    return 1;
  }

  NT_print("Reading ldmd conf file", fname.c_str());

  while(!cfgfile.df_EOF()) {
    memset(lbuf, 0, ldm_line_len);
    cfgfile.df_GetLine(lbuf, sizeof(lbuf), '\n');
    if(cfgfile.df_GetError() != DiskFileB::df_NO_ERROR) {
      message << clear << "ERROR - read failed " << fname.c_str();
      NT_print(message.c_str());
      error_level = 1;
      break;
    }
    
    info_line = lbuf;
    info_line.FilterChar('\n'); info_line.FilterChar('\r');    
    info_line.TrimLeadingSpaces(); info_line.TrimTrailingSpaces();
    info_line.TrimLeading('\t'); info_line.TrimTrailing('\t');
    if(info_line[0] == '#') continue; 
    if(info_line.Find("#") != -1) { // Filter inline comments/remarks 
      info_line.DeleteAfterIncluding("#");
      info_line.TrimTrailingSpaces();
      info_line.TrimTrailing('\t');
    }
    if(info_line.is_null()) continue;
    if(info_line[0] == '#') continue; 
    
    if(info_line.IFind("INCLUDE") != -1) {
      gxString incbuf(info_line);
      gxString inc_file_name(info_line);
      incbuf.DeleteAfterIncluding(" "); incbuf.DeleteAfterIncluding("\t");
      incbuf.ToUpper();
      if(incbuf == "INCLUDE") {
	inc_file_name.DeleteBeforeIncluding(" "); inc_file_name.DeleteBeforeIncluding("\t");
	includes_list.Add(inc_file_name);	
      }
    }

    gxString cfg_file_line(info_line);
    conf_lines.Add(cfg_file_line);      
  }

  cfgfile.df_Close();
  return error_level;
} 

// Returns 0 for pass
// Return non-zero value for fail, with error str returned in error_message
int ldm_ip_to_hostname(const gxString &ip_str, gxString &hostname_str, gxString &error_message)
{
  struct addrinfo *result;
  int gai_result;
  char hostname[NI_MAXHOST];

  memset(hostname, 0, NI_MAXHOST);
  hostname_str.Clear();
  error_message.Clear();

  if((gai_result = getaddrinfo(ip_str.c_str(), 0, 0, &result)) != 0) {
    error_message << clear << gai_strerror(gai_result);
    return gai_result;
  }
  if((gai_result = getnameinfo(result->ai_addr, result->ai_addrlen, hostname, NI_MAXHOST, NULL, 0, 0)) != 0) {  
    error_message << clear << gai_strerror(gai_result);
    freeaddrinfo(result);
    return gai_result;
  }

  if(*hostname != '\0') hostname_str << clear << hostname;
  freeaddrinfo(result);
  return 0;
}

int compare_ldm_regex(const gxString &regex_str, const gxString &str, int check_case)
{
  char message_buf[255];
  return compare_ldm_regex(regex_str, str, message_buf, check_case);
}

// Returns 0 for a regex match to str
// Returns REG_NOMATCH if no match to str
// Returns other non-zero value on error, with error message in message_buf
int compare_ldm_regex(const gxString &regex_str, const gxString &str, char message_buf[255], int check_case)
{
  regex_t regex;
  int reti;
  memset(message_buf, 0, 255);

  if(check_case == 1) {
    reti = regcomp(&regex, regex_str.c_str(), REG_EXTENDED|REG_NOSUB);
  }
  else {
    reti = regcomp(&regex, regex_str.c_str(), REG_EXTENDED|REG_NOSUB|REG_ICASE);
  }
  if(reti != 0) {
    regerror(reti, &regex, message_buf, 255);
    regfree(&regex);
    return reti;
  }

  reti = regexec(&regex, str.c_str(), 0, NULL, 0);
  if(reti != REG_NOMATCH) regerror(reti, &regex, message_buf, 255);
  regfree(&regex);

  return reti;
} 

// Returns 0 for a regex match to IP address, has_match set to 1
// Returns REG_NOMATCH if no match to IP address, has_match set to 0
// Returns other non-zero value on error, will log message
int compare_ldm_hostere_to_hostip(const gxString &hostIdEre, const gxString &hostip, int seq_num, int &has_match, int check_hostname)
{
  int reti;
  char message_buf[255];
  int check_case = 0;
  gxString hostname_str, error_message, sbuf;
  has_match = 0;

  // Check the IP address
  reti = compare_ldm_regex(hostIdEre, hostip, message_buf, check_case);
  if(reti == 0) {
    if(servercfg->debug && servercfg->debug_level >= 5) {
      sbuf << clear << "[" << seq_num << "]: INFO - LDM host IP matches regex: " << hostip << " match " << hostIdEre;
      LogDebugMessage(sbuf.c_str());
    }
    has_match = 1;
    return 0;
  }
  if((reti == REG_NOMATCH) && (check_hostname == 1)) {
    if(ldm_ip_to_hostname(hostip, hostname_str, error_message) != 0) {
      sbuf << clear << "[" << seq_num << "]: ERROR - LDM get hostname error for IP " << hostip << " " << error_message;
      LogMessage(sbuf.c_str());
      return reti;
    }
    if((hostip != hostname_str) && (!hostname_str.is_null())) { 
      // Check the hostname if we resolved the IP to a hostname
      reti = compare_ldm_regex(hostIdEre, hostname_str, message_buf, check_case);
      if(reti == 0) {
	if(servercfg->debug && servercfg->debug_level >= 5) {
	  sbuf << clear << "[" << seq_num << "]: INFO - LDM hostname matches regex: " << hostname_str << " match " << hostIdEre;
	  LogDebugMessage(sbuf.c_str());
	}
	has_match = 1;
	return 0;
      }
    }
  }

  if(reti == REG_NOMATCH) {
    if(servercfg->debug && servercfg->debug_level >= 5 && servercfg->verbose_level >= 5) {
      if(!hostname_str.is_null()) {
	sbuf << clear << "[" << seq_num << "]: INFO - LDM host IP/hostname does not match regex: " << hostip 
	     << "/" << hostname_str << " no match "  << hostIdEre;
      }
      else {
	sbuf << clear << "[" << seq_num << "]: INFO - LDM host IP does not match regex: " << hostip << " no match "  << hostIdEre;
      }
      LogDebugMessage(sbuf.c_str());
    }
    has_match = 0;
  }
  else {
    if(servercfg->debug && servercfg->debug_level >= 5) {
      sbuf << clear << "[" << seq_num << "]: ERROR - Regex match for " << hostIdEre << " failed: " << message_buf;
      LogDebugMessage(sbuf.c_str());
    }
    has_match = 0;
  }

  return reti;
}

int ldm_check_queue_access(const gxString &accept_allow_feeds, const gxString &req_feeds, int &has_allow_accept, gxString &message)
{
  int error_level = 0;
  message.Clear();
  has_allow_accept = 0;
  
  gxString aa_feeds = accept_allow_feeds;
  aa_feeds.ToUpper();
  while(aa_feeds.Find("||") != -1) aa_feeds.ReplaceString("||", "|");
  aa_feeds.FilterChar(' '); aa_feeds.FilterChar('\t');
  gxString r_feeds = req_feeds;
  r_feeds.ToUpper();
  while(r_feeds.Find(",,") != -1) r_feeds.ReplaceString("||", "|");
  r_feeds.FilterChar(' '); r_feeds.FilterChar('\t');
  
  if(aa_feeds == "ANY") {
    has_allow_accept = 1;
    message << clear << "Requested feed is " << r_feeds << " opening access to " << aa_feeds;
    return error_level;
  }

  unsigned ilist = 0;
  gxString *listvars = 0;
  unsigned num_list_arr = 0;
  gxString list_delim = "|";
  listvars = ParseStrings(aa_feeds, list_delim, num_list_arr);
  if(num_list_arr == 0) {
    listvars = new gxString[1];
    listvars[0] = aa_feeds;
    num_list_arr = 1;
  }
		  
  unsigned ireq = 0;
  gxString *reqvars = 0;
  unsigned num_req_arr = 0;
  gxString req_delim = ",";
  reqvars = ParseStrings(r_feeds, req_delim, num_req_arr);
  if(num_req_arr == 0) {
    reqvars = new gxString[1];
    reqvars[0] = r_feeds;
    num_req_arr = 1;
  }

  for(ilist = 0; ilist < num_list_arr; ilist++) {
    for(ireq = 0; ireq < num_req_arr; ireq++) {
      if(listvars[ilist] == reqvars[ireq]) {
	has_allow_accept = 1;
	message << clear << "Requested feed is " << reqvars[ireq] << " opening access to " << listvars[ilist];
	break;
      }
    }
  }
 
  delete[] listvars; listvars = 0;
  delete[] reqvars; reqvars = 0;
  
  if(!has_allow_accept) {
    message << clear << "Requested feed was " << r_feeds << " no match to " << aa_feeds;
  }

  return error_level;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
