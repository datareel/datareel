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
#include "ssl_utils.h"

int init_ssl_cert()
{
  gxSSL *ssl = &sslcfg->openssl;
  int error_level = 0;

  if(!futils_exists(sslcfg->ssl_key_file.c_str())) {
    NT_print("ERROR - Missing private key:", sslcfg->ssl_key_file.c_str());
    return 1;
  }

  if(!futils_exists(sslcfg->ssl_cert_file.c_str())) {
    NT_print("ERROR - Missing SSL cert:", sslcfg->ssl_key_file.c_str());
    return 1;
  }
  
  if(sslcfg->ssl_use_dhparms) {
    if(!futils_exists(sslcfg->ssl_dhparms_file.c_str())) {
      NT_print("ERROR - Missing dhparms file:", sslcfg->ssl_dhparms_file.c_str());
      return 1;
    }
  }

  char hexbuf[255];
  memset(hexbuf, 0, 255);
  // OPENSSL_VERSION_NUMBER = 0xnnnnnnnnnL
  sprintf(hexbuf, "0x%09lx", (unsigned long)OPENSSL_VERSION_NUMBER & 0xFFFFFFFFF);
  NT_print("Open SSl version:", hexbuf);

  sslcfg->ssl_protocol.ToUpper();
  if(sslcfg->ssl_protocol == "SSLV2") {
    sslcfg->meth = gxSSL_SSLv2;
    NT_print("SSL config set to use protocol SSLv2"); 
  }
  if(sslcfg->ssl_protocol == "SSLV23") {
    sslcfg->meth = gxSSL_SSLv23;
    NT_print("SSL config set to use protocol SSLv23"); 
  }
  if(sslcfg->ssl_protocol == "SSLV3") {
    sslcfg->meth = gxSSL_SSLv3;
    NT_print("SSL config set to use protocol SSLv3"); 
  }
  if(sslcfg->ssl_protocol == "TLSV1") {
    sslcfg->meth = gxSSL_TLSv1;
    NT_print("SSL config set to use protocol TLSv1"); 
  }
  if(sslcfg->ssl_protocol == "TLS") {
    sslcfg->meth = gxSSL_TLS;
    NT_print("SSL config set to use protocol TLS"); 
  }
  if(sslcfg->ssl_protocol == "TLSV1_1") {
    sslcfg->meth = gxSSL_TLSv1_1;
    NT_print("SSL config set to use protocol TLSv1_1"); 
  }
  if(sslcfg->ssl_protocol == "TLSV1_2") {
    sslcfg->meth = gxSSL_TLSv1_2;
    NT_print("SSL config set to use protocol TLSv1_2"); 
  }
  if(sslcfg->ssl_protocol == "DTLS") {
    sslcfg->meth = gxSSL_DTLS;
    NT_print("SSL config set to use protocol DTLS"); 
  }
  if(sslcfg->ssl_protocol == "DTLSV1") {
    sslcfg->meth = gxSSL_DTLSv1;
    NT_print("SSL config set to use protocol DTLSv1"); 
  }
  if(sslcfg->ssl_protocol == "DTLSV1_2") {
    sslcfg->meth = gxSSL_DTLSv1_2;
    NT_print("SSL config set to use protocol DTLSv1_2"); 
  }

  ssl->SetSSLProto(sslcfg->meth);
  if(!gxSSL::InitSSLibrary()) {
    NT_print("ERROR - Cannot initializing SSL library");
    if(servercfg->debug) NT_print(ssl->SSLExceptionMessage());
    return 1;
  }

  if(!sslcfg->ssh_ca_list_file.is_null()) ssl->SetCAList(sslcfg->ssh_ca_list_file.c_str());

  NT_print("Loading SSL key and cert");
  if(!ssl->InitCTX(sslcfg-> ssl_cert_file.c_str(), sslcfg->ssl_key_file.c_str())) {
    NT_print("ERROR - Problem loading key and cert");
    if(servercfg->debug) NT_print(ssl->SSLExceptionMessage());
    return 1;
  }
  if(sslcfg->ssl_use_dhparms) {
    NT_print("Loading Diffie-Hellman parameters");
    if(ssl->LoadDHParms(sslcfg->ssl_dhparms_file.c_str()) != gxSSL_NO_ERROR) {
      NT_print("ERROR - Problem loading DH parms");
      if(servercfg->debug) NT_print(ssl->SSLExceptionMessage());
      return 1;
    }
  }

  switch(sslcfg->meth) {
    case gxSSL_SSLv2:
      NT_print("SSL connections will only use SSLv2"); 
      break;
    case gxSSL_SSLv23:
      NT_print("SSL connections will use SSLv2, SSLv3, TLSv1, TLSv1.1 and TLSv1.2"); 
      break;
    case gxSSL_SSLv3:
      NT_print("SSL connections will only use SSLv3"); 
      break;
    case gxSSL_TLSv1:
      NT_print("SSL connections will only use TLSv1"); 
      break;
    case gxSSL_TLS:
      NT_print("SSL connections will use SSLv3, TLSv1, TLSv1.1 and TLSv1.2"); 
      break;
    case gxSSL_TLSv1_1:
      NT_print("SSL connections will only use TLSv1_1"); 
      break;
    case gxSSL_TLSv1_2:
      NT_print("SSL connections will only use TLSv1_2"); 
      break;
    case gxSSL_DTLS:
      NT_print("SSL connections will use multiple DTLS methods"); 
      break;
    case gxSSL_DTLSv1:
      NT_print("SSL connections will only use DTLSv1"); 
      break;
    case gxSSL_DTLSv1_2:
      NT_print("SSL connections will only use DTLSv1_2"); 
      break;
    default:
      NT_print("SSL connections will use SSLv2, SSLv3, TLSv1, TLSv1.1 and TLSv1.2"); 
      break;
  }

  return error_level;
}

// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
