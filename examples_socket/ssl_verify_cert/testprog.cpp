// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: server.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/17/2001
// Date Last Modified: 10/24/2026
// Copyright (c) 2001-2025 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description and Details ------------- // 
// ----------------------------------------------------------- // 
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.
 
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  
USA

SSL cert verify example program for client side SSL.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "gxssl.h"

// Program's main thread of execution.
// ----------------------------------------------------------- 
int main(int argc, char **argv)
{
  int error_level = 0;

  if(argc < 2) { 
    cout << "ERROR - You must supply an HTTPS hostname" << "\n";
    return 1;
  }

  gxString hostname = argv[1];
  int port_number = 443;
  gxSocket client;
  gxSSL openssl_client;

  hostname.DeleteBeforeIncluding("https://");
  hostname.DeleteAfterLastIncluding("/");

  // Send the hostname to the server (SNI) for virtual hosted sites
  openssl_client.SetHostName(hostname.c_str());

  // We must supply a trusted CA list for client-side cert verification
  openssl_client.SetCAList("ca-bundle.crt");

  if(client.InitSocketLibrary() == 0) {
    if(client.InitSocket(SOCK_STREAM, port_number,  hostname.c_str()) < 0) {
      cout << "ERROR - Cannot open connection to " << hostname.c_str() <<  " TCP port " << port_number << "\n";
      cout << client.SocketExceptionMessage() << "\n";
      return 1;
    }
  }
  else {
   cout << "ERROR - Cannot init socket library" << "\n";
   cout << client.SocketExceptionMessage() << "\n";
   return 1;
  }
  
  if(client.Connect() < 0) {
    cout << "ERROR - Cannot connect to " << hostname.c_str() <<  " TCP port " << port_number << "\n";
    cout << client.SocketExceptionMessage() << "\n";
    return 1;
  }
  if(openssl_client.OpenSSLSocket(client.GetSocket()) != gxSSL_NO_ERROR) {
    cout << "ERROR - Cannot open SSL socket to " << hostname.c_str() <<  " TCP port " << port_number << "\n";
    cout << openssl_client.SSLErrorMessage() << "\n";
    client.Close();
    return 1;
  }
  if(openssl_client.ConnectSSLSocket() != gxSSL_NO_ERROR) {
    cout << "ERROR - Cannot connect SSL socket to " << hostname.c_str() <<  " TCP port " << port_number << "\n";
    cout << openssl_client.SSLErrorMessage() << "\n";
    openssl_client.CloseSSLSocket();
    client.Close();
    return 1;
  }

  // 10/24/2026: Use the gxSSL cert functions so this example works with
  // both OpenSSL and the native Windows Schannel builds.
  gxString subject, issuer;
  if(openssl_client.GetPeerCertInfo(subject, issuer) != gxSSL_NO_ERROR) {
    cout << "ERROR - No certificate presented by " << hostname.c_str() << "\n";
    cout << "\n";
    openssl_client.CloseSSLSocket();
    client.Close();
    return 1;
  }

  cout << "\n";
  cout << "Certificate Info:" << "\n";
  cout << "Subject: " << subject.c_str() << "\n";
  cout << "Issuer: " << issuer.c_str() << "\n";
  cout << "\n";

  long X509_return_code = 0;
  int rv = openssl_client.VerifyCert(hostname.c_str(), &X509_return_code);

  if(X509_return_code != 0) {
    cout << "ERROR - Cannot verify " << hostname.c_str()  << " certificate" << "\n";
    cout << gxSSL::CertVerifyErrorString(X509_return_code) << "\n";
    error_level = 1;
  }
  if(rv != 0) {
    cout << "ERROR - Cert verify returned an error condition" << "\n";
    cout << openssl_client.SSLErrorMessage() << "\n";
    error_level = 1;
  }
  if(error_level == 0) {
    cout << "SSL cert for " << hostname.c_str() << " verify passed" << "\n";
    cout << "\n";
  }
  else {
    cout << "SSL cert for " << hostname.c_str() << " verify failed" << "\n";
    cout << "\n";
  }

  openssl_client.CloseSSLSocket();
  client.Close();
  return error_level;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
