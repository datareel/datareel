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

SSL config for DR load balancer.

*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "../drlb_server/drlb_server.h"
#include "ssl_config.h"

SSLconfig SSLConfigSruct;
SSLconfig *sslcfg = &SSLConfigSruct;

SSLconfig::SSLconfig()
{
  ssl_key_file = "/etc/drlb/ssl/private/ca.key";
  ssl_cert_file =  "/etc/drlb/ssl/certs/ca.crt";
  ssl_dhparms_file =  "/etc/drlb/ssl/certs/dhparams.pem";
  ssl_backend_ca_list_file = "/etc/pki/tls/certs/ca-bundle.crt";
  ssl_encrypt_backend = 0;
  ssl_backend_hostname.Clear();
  ssl_verify_backend_cert = 0;
  ssh_ca_list_file.Clear();
  ssl_use_dhparms = 0;
  meth =  gxSSL_SSLv23;
  ssl_protocol = "SSLv23";
}

SSLconfig::~SSLconfig()
{

}

int LoadSSLConfig()
{
  NT_print("Loading SSL configuration");
  
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

  int has_ssl_config_section = 0;
  gxListNode<gxString> *ptr = filelist.GetHead();
  while(ptr) {
    if(ptr->data.IFind("[SSLCONFIG]") != -1) {
      has_ssl_config_section = 1;
      break;
    }
    ptr = ptr->next;
  }

  if(!has_ssl_config_section) {
    NT_print("INFO - DRLB config file does not have and [SSLCONFIG] section:", servercfg->config_file.c_str());
    NT_print("INFO - Adding [SSLCONFIG] section to DRLB server config:", servercfg->config_file.c_str());
    DiskFileB dfile(servercfg->config_file.c_str(), DiskFileB::df_READWRITE, DiskFileB::df_CREATE); 
    if(!dfile) {
      NT_print("ERROR - Cannot write to config file", servercfg->config_file.c_str());
      return 1;
    }
    dfile << "#" << "\n";
    dfile << "\n";
    dfile << "# DRLB server added SSLCONFIG section to config file" << "\n";
    dfile << "[SSLCONFIG]" << "\n";
    dfile << "# SSL bridge config" << "\n";
    dfile << "#" << "\n";
    dfile << "# SSL protocol" << "\n";
    dfile << "# Default is SSLv2, SSLv3, TLSv1, TLSv1.1 and TLSv1.2" << "\n";
    dfile << "ssl_protocol = SSLv23" << "\n";
    dfile << "# Other protocol options:ssl_protocol:" << "\n";
    dfile << "# " << "\n";
    dfile << "# SSLv2 - SSL version 2" << "\n";
    dfile << "# SSLv3 - SSL version 3" << "\n";
#if OPENSSL_VERSION_NUMBER >= 0x1000200fL
    dfile << "# TLS - SSLv3, TLSv1, TLSv1.1 and TLSv1.2" << "\n";
#endif
    dfile << "# TLSv1 - TLS version 1" << "\n";
    dfile << "# TLSv1_1 - TLS version 1.1" << "\n";
    dfile << "# TLSv1_2 - TLS version 1.2" << "\n";
    dfile << "#" << "\n";
    dfile << "# Private key file" << "\n";
    dfile << "ssl_key_file = /etc/drlb/ssl/private/ca.key" << "\n";
    dfile << "#" << "\n";
    dfile << "# SSL cert file" << "\n";
    dfile << "ssl_cert_file =  /etc/drlb/ssl/certs/ca.crt" << "\n";
    dfile << "#" << "\n";
    dfile << "# Certificate chain" << "\n";
    dfile << "##ssh_ca_list_file = /etc/drlb/ssl/certs/ca_list.crt" << "\n";
    dfile << "#" << "\n";
    dfile << "# Optional Diffie-Hellman parameters" << "\n";
    dfile << "##ssl_use_dhparms = 0" << "\n";
    dfile << "##ssl_dhparms_file =  /etc/drlb/ssl/certs/dhparams.pem" << "\n";
    dfile << "#" << "\n";
    dfile << "# Client side settings for backend SSL encryption" << "\n";
    dfile << "# Use SSL to connect to LB nodes" << "\n";
    dfile << "##ssl_encrypt_backend = 0" << "\n";
    dfile << "# The backend hostname must match the issuer's cert CN name" << "\n";
    dfile << "##ssl_backend_hostname = www.example.com" << "\n";
    dfile << "# If verify cert is set to 1 you will need to provide a trusted CA list" << "\n";
    dfile << "##ssl_verify_backend_cert = 0" << "\n";
    dfile << "##ssl_backend_ca_list_file = /etc/pki/tls/certs/ca-bundle.crt" << "\n";

    dfile << "\n";
    ptr = filelist.GetHead();
    while(ptr) {
      dfile << ptr->data.c_str();
      ptr = ptr->next;
    }
    dfile.df_Close();
  }
  else {
    NT_print("INFO - Reading [SSLCONFIG] config section from DRLB config file:", servercfg->config_file.c_str());
    ptr = filelist.GetHead();
    has_ssl_config_section = 0;
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
      if(strdup.IFind("[SSLCONFIG]") != -1) {
	has_ssl_config_section++;
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

	  // Add all [SSLCONFIG] reads here
	  if(info_line.IFind("ssl_key_file") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_key_file") {
	      sslcfg->ssl_key_file = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(info_line.IFind("ssl_cert_file") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_cert_file") {
	      sslcfg->ssl_cert_file = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(info_line.IFind("ssh_ca_list_file") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssh_ca_list_file") {
	      sslcfg->ssh_ca_list_file = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(info_line.IFind("ssl_encrypt_backend") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_encrypt_backend") {
	      sslcfg->ssl_encrypt_backend = CfgGetTrueFalseValue(info_line);
	    }
	  }
	  if(info_line.IFind("ssl_verify_backend_cert") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_verify_backend_cert") {
	      sslcfg->ssl_verify_backend_cert = CfgGetTrueFalseValue(info_line);
	    }
	  }
	  if(info_line.IFind("ssl_backend_hostname") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_backend_hostname") {
	      sslcfg->ssl_backend_hostname = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(info_line.IFind("ssl_backend_ca_list_file") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_backend_ca_list_file") {
	      sslcfg->ssl_backend_ca_list_file = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(info_line.IFind("ssl_use_dhparms") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_use_dhparms") {
	      sslcfg->ssl_use_dhparms = CfgGetTrueFalseValue(info_line);
	    }
	  }
	  if(info_line.IFind("ssl_dhparms_file") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_dhparms_file") {
	      sslcfg->ssl_dhparms_file = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(info_line.IFind("ssl_protocol") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_protocol") {
	      sslcfg->ssl_protocol = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(!ptr) break;
	  ptr = ptr->next;
	}
      }
      if(!ptr) break;
      ptr = ptr->next;
    }
    if(has_ssl_config_section > 1) {
      sbuf << clear << "ERROR - DRLB server config has " << has_ssl_config_section 
	   << " [SSLCONFIG] sections " << servercfg->config_file;
      NT_print(sbuf.c_str());
      return 1;
    }
  }

  return error_level;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
