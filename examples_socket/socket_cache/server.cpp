// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: server.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 10/31/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description and Details ------------- // 
// ----------------------------------------------------------- // 
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.
 
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

This is a test program for the device cache classes.

10/31/2026: The stream server authenticates its clients (DataReel 6.3,
the same options as the DataReel 7.3.1 stream_server example):

  server --cert server.pem --key server.key --client-ca ca.pem
         [--allow-fingerprint SHA256] [--allow-cn NAME] ...
  server --open [--cert server.pem --key server.key]
  [--allow-remote-shutdown]

--client-ca with at least one --allow-fingerprint or --allow-cn turns
on mutual TLS. --open runs an open server (plain TCP, or TLS without
client certificates with --cert/--key). Without either the stream
server refuses to start. A client's shutdown request is obeyed only
with --allow-remote-shutdown.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include <string.h>
#include "gxsfile.h"

// 10/31/2026: Stream server options, see above
static const char *opt_cert = 0, *opt_key = 0, *opt_client_ca = 0;
static const char *opt_fp[16], *opt_cn[16];
static int num_fp = 0, num_cn = 0;
static int opt_open = 0, opt_allow_shutdown = 0;

static void PrintableName(const char *s)
// Print a name from a client certificate with control characters escaped
{
  for(; s && *s; s++) {
    unsigned char c = (unsigned char)*s;
    if(c < 0x20 || c == 0x7f) cout << "?";
    else cout << (char)c;
  }
}

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

void ClearInputStream(istream &s)
{
  char c;
  s.clear();
  while(s.get(c) && c != '\n') { ; }
}

void InputString(char *mesg, MemoryBuffer &s)
{
  cout << mesg << ": ";
  char buf[255];
  for(int i = 0; i < 255; i++) buf[i] = 0;
  cin >> buf;
  s.Load(buf, (strlen(buf) + 1));
  s[s.length()] += '\0'; // Null terminate the string
}

void InputInt(char *mesg, int &i)
{
  cout << mesg << ": ";
  cin >> i;
  ClearInputStream(cin);
}

void Menu()
{
  cout << "\n" << flush;
  cout << "(?)   Display this menu" << "\n" << flush;
  cout << "(D)   Copy a Datagram Socket to a disk file" << "\n" << flush;
  cout << "(F)   Copy a disk file to another disk file" << "\n" << flush;
  cout << "(P)   Copy a Serial port to a disk file" << "\n" << flush;
  cout << "(Q)   Quit this program" << "\n" << flush;
  cout << "(S)   Copy a Stream Socket to a disk file" << "\n" << flush;
  cout << "\n" << flush;
}

int CopyStreamSocketToFile(gxsFile *dev)
// Copies data from a stream socket to the memory cache
// and flushes the cache buffers to a disk file.
{
  MemoryBuffer s;
  InputString("Enter the name of the output file", s);
  
  if(!dev->OpenOutputFile((char *)s)) {
    cout << "Could not open the " << (char *)s << " file" << "\n" << flush;
    return 0;
  }

  int port;
  InputInt("Enter the server's port number", port);

  gxStream server;
  gxsSocket_t remote_socket;
  FAU_t byte_count = (FAU_t)0;

  // 10/31/2026: Client authentication, or the explicit --open opt-out
  if(opt_cert) server.SetServerCert(opt_cert, opt_key);
  if(opt_client_ca) server.SetClientCAFile(opt_client_ca);
  for(int i = 0; i < num_fp; i++) {
    if(server.AllowClientFingerprint(opt_fp[i]) != 0) {
      cout << "Bad fingerprint: " << server.TLSError() << "\n" << flush;
      return 1;
    }
  }
  for(int i = 0; i < num_cn; i++) server.AllowClientCN(opt_cn[i]);
  if(opt_open) server.AllowAnonymousClients(true);
  
  cout << "Initializing the GX stream server..." << "\n" << flush;
  if(server.StreamServer(port) != 0) {
    cout << server.SocketExceptionMessage() << "\n" << flush;
    if(*server.TLSError()) cout << server.TLSError() << "\n" << flush;
    return 1;
  }
  
  // Get the host name assigned to this machine
  char hostname[gxsMAX_NAME_LEN];
  if(server.HostName(hostname) != 0) {
    cout << server.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }

  cout << "Opening stream server on host " << hostname << "\n" << flush;
  int server_up = 1;

  cout << "Listening on port " << port << "\n" << flush;
  remote_socket = server.Accept();
  
  if(remote_socket < 0) {
    cout << server.SocketExceptionMessage() << "\n" << flush;
    if(*server.TLSError()) cout << server.TLSError() << "\n" << flush;
    return 1;
  }
  if(server.ClientAuthenticated()) {
    cout << "Client certificate CN=";
    PrintableName(server.PeerCN());
    cout << " SHA-256 " << server.PeerFingerprint() << "\n" << flush;
  }

  while(server_up) {
    // Read the block following a client connection
    gxBlockHeader gx;
    if(server.ReadClientHeader(gx) != 0) {
      cout << server.SocketExceptionMessage() << "\n" << flush;
      return 1;
    }

    // Get the client info
    char client_name[gxsMAX_NAME_LEN]; int r_port = -1;
    server.GetClientInfo(client_name, r_port);
    cout << client_name << " connecting on port " << r_port << "\n" << flush;
    
    // Read the status byte to determine what to do with this block
    __ULWORD__ block_status = gx.block_status;
    __SBYTE__ status = (__SBYTE__)((block_status & 0xFF00)>>8);

    switch(status) { 
      // Process each block of data
      case gxSendBlock :  
	// 10/31/2026: The length comes from the client. Check it before
	// the cache allocates a buffer for it.
	if((__ULWORD__)gx.block_length > (__ULWORD__)server.MaxBlockSize()) {
	  cout << "Block of " << gx.block_length << " bytes is too large" 
	       << "\n" << flush;
	  return 0;
	}
	cout << "Reading " << gx.block_length << " bytes from " << client_name 
	     << " remote port " << r_port << "\n" << flush;
	
	if(!dev->CopyStreamSocketToFile(&server, gx)) {
	  cout << "Error copying from stream to the file." << "\n" << flush;
	  return 0;
	}
	
	byte_count += (__ULWORD__)gx.block_length;
	break;

      case gxKillServer:
	// 10/31/2026: Any client could stop the server. Obeyed only with
	// --allow-remote-shutdown.
	if(!opt_allow_shutdown) {
	  cout << "Client shutdown request ignored" << "\n" << flush;
	  server.Close();
	  server_up = 0;
	  break;
	}
	cout << "Client shutdown the server" << "\n" << flush;
	server.Close();
	server_up = 0;
	break;

      default:
	cout << "Only accepting raw data blocks" << "\n" << flush;
	cout << "The \"" << status << "\" command was rejected" << "\n" 
	     << flush;
	server.CloseRemoteSocket();
	break;
    }
  }

  cout << "Number of cache buckets in use = " << dev->BucketsInUse() << "\n" 
       << flush;
  dev->Flush(); // Flush all the cache buffers before exiting
  dev->CloseOutputFile();
  cout << "Exiting..." << "\n" << flush;
  return 1;
}

int CopyDatagramSocketToFile(gxsFile *dev)
// Copies data from a stream socket to the memory cache
// and flushes the cache buffers to a disk file.
{
  MemoryBuffer s;
  InputString("Enter the name of the output file", s);
  
  if(!dev->OpenOutputFile((char *)s)) {
    cout << "Could not open the " << (char *)s << " file" << "\n" << flush;
    return 0;
  }

  int port;
  InputInt("Enter the server's port number", port);

  gxDatagram server;
  FAU_t byte_count = (FAU_t)0;
  
  cout << "Initializing the GX datagram server..." << "\n" << flush;
  if(server.DatagramServer(port) != 0) {
    cout << server.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }

  // Get the host name assigned to this machine
  char hostname[gxsMAX_NAME_LEN];
  if(server.HostName(hostname) != 0) {
    cout << server.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }
  cout << "Opening GX datagram server on host " << hostname << "\n" << flush;

  // Find out what port was really assigned 
  int assigned_port;
  if(server.PortNumber(assigned_port) != 0) {
    cout << server.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }
  cout << "Port assigned is " << assigned_port << "\n" << flush;

  int server_up = 1;
  while(server_up) {
    // Read the block following a client connection
    gxBlockHeader gx;
    if(server.ReadClientHeader(gx) != 0) {
      cout << server.SocketExceptionMessage() << "\n" << flush;
      return 1;
    }

    // Get the client info
    char client_name[gxsMAX_NAME_LEN]; int r_port = -1;
    server.GetClientInfo(client_name, r_port);
    cout << client_name << " connecting on port " << r_port << "\n" << flush;

    // Read the status byte to determine what to do with this block
    __ULWORD__ block_status = gx.block_status;
    __SBYTE__ status = (__SBYTE__)((block_status & 0xFF00)>>8);

    switch(status) { 
      // Process each block of data
      case gxSendBlock :
	// 11/14/2026: The length comes from the client. Check it before
	// the cache allocates a buffer for it.
	if((__ULWORD__)gx.block_length > (__ULWORD__)server.MaxBlockSize()) {
	  cout << "Block of " << gx.block_length << " bytes is too large" 
	       << "\n" << flush;
	  return 0;
	}
	cout << "Reading " << gx.block_length << " bytes from " << client_name 
	     << " remote port " << r_port << "\n" << flush;
	
	if(!dev->CopyDatagramSocketToFile(&server, gx)) {
	  cout << "Error copying from stream to the file." << "\n" << flush;
	  return 0;
	}

	byte_count += (__ULWORD__)gx.block_length;
	break;

      case gxKillServer:
	cout << "Client shutdown the server" << "\n" << flush;
	server.Close();
	server_up = 0;
	break;
	
      default:
	cout << "Only accepting raw data blocks" << "\n" << flush;
	cout << "The \"" << status << "\" command was rejected" << "\n" 
	     << flush;
	break;
    }
  }
    
  cout << "Number of cache buckets in use = " << dev->BucketsInUse() << "\n" 
       << flush;
  dev->Flush(); // Flush all the cache buffers before exiting
  dev->CloseOutputFile();
  cout << "Exiting..." << "\n" << flush;
  return 1;
}

void CopyFileToFile(gxsFile *dev)
{
  MemoryBuffer in, out;

  InputString("Enter the name of the input file", in);

  if(!dev->OpenInputFile(in)) {
    cout << "Cannot open the specified file!" << "\n" << flush;
    return;
  }

  InputString("Enter the name of the output file", out);
  
  if(!dev->OpenOutputFile(out)) {
    cout << "Cannot open the specified file!" << "\n" << flush;
    return;
  }

  FAU_t byte_count = (FAU_t)0;

  if(!dev->CopyFileToFile(byte_count)) {
    cout << "Error copying file." << "\n" << flush;
    return;
  }

  cout << "Number of cache buckets in use = " << dev->BucketsInUse() << "\n" 
       << flush;
  cout << "Flushing the device cache..." << "\n" << flush;
  dev->Flush();
  dev->CloseInputFile(); dev->CloseOutputFile();
  cout << "Finished processing file." << "\n" << flush;
  cout << "Byte count = " << (long)byte_count << "\n" << flush;
}

void CopySerialPortToFile(gxsFile *dev) 
{
  MemoryBuffer out, port;
  InputString("Enter the name of the serial device", port);
  InputString("Enter the name of the output file", out);

  if(!dev->OpenOutputFile(out)) {
    cout << "Cannot open the specified file!" << "\n" << flush;
    return;
  }
  
  FAU_t byte_count = (FAU_t)0;
  gxSerialCommServer server;

  cout << "Initializing the GX serial port server..." << "\n" << flush;
  int rv = server.InitCommServer(port);
  if(rv != gxSerialComm::scomm_NO_ERROR) {
    cout << server.SerialCommExceptionMessage() << "\n" << flush;
    return;
  }
  
  cout << "Opening GX serial port server on " << (char*)port << "\n" 
       << flush;  

  int server_up = 1;
  while(server_up) {
    // Wait for a block header.
    gxBlockHeader gx;
    if(server.ReadHeader(gx) != 0) {
      cout << server.SerialCommExceptionMessage() << "\n" << flush;
      return;
    }
    
    cout << "Client connecting on " << (char *)port << "\n" << flush;
    
    // Read the status byte to determine what to do with this block
    __ULWORD__ block_status = gx.block_status;
    __SBYTE__ status = (__SBYTE__)((block_status & 0xFF00)>>8);
    
    switch(status) { 
      // Process each block of data
      case gxSendBlock :
	// 11/14/2026: The length comes from the client. Check it before
	// the cache allocates a buffer for it.
	if((__ULWORD__)gx.block_length > (__ULWORD__)gxDATAGRAM_DEF_MAX_BLOCK) {
	  cout << "Block of " << gx.block_length << " bytes is too large" 
	       << "\n" << flush;
	  return; // This function returns void
	}
	cout << "Reading database block " << gx.block_length
	     << " bytes in length" << "\n" << flush;
	if(!dev->CopySerialPortToFile(&server, gx)) {
	  cout << "Error copying from serial port to the file." << "\n" 
	       << flush;
	  return;
	}
	byte_count += (__ULWORD__)gx.block_length;
    	break;

      case gxKillServer:
	cout << "Client shutdown the server" << "\n" << flush;
	server.Close();
	server_up = 0;
	break;

      default:
	cout << "Only accepting raw data blocks" << "\n" << flush;
	cout << "The \"" << status << "\" command was rejected" << "\n" 
	     << flush;
	break;
    }
  }

  cout << "Number of cache buckets in use = " << dev->BucketsInUse() << "\n" 
       << flush;
  cout << "Flushing the device cache..." << "\n" << flush;
  dev->Flush();
  dev->CloseOutputFile();
  cout << "Finished processing file." << "\n" << flush;
  cout << "Byte count = " << (long)byte_count << "\n" << flush;
}

void SetupServer(int server_type)
{
  int cache_size = 1024;
  gxsFile dev(cache_size); // Device cache used to process a file

  cout << "Creating a device cache using " << cache_size 
       << " cache buckets." << "\n" << flush;
  cout << "Reserving " << (sizeof(gxDeviceBucket) * cache_size) 
       << " bytes of memory." << "\n" << flush;
  cout << "Number of cache buckets in use = " << dev.BucketsInUse() << "\n" 
       << flush;
  cout << "\n" << flush;
  
  switch(server_type) {
    case gxSOCKET_STREAM_SERVER:
      CopyStreamSocketToFile(&dev);
      Menu();
      break;
      
    case gxSOCKET_DATAGRAM_SERVER:
      CopyDatagramSocketToFile(&dev);
      Menu();
      break;

    case gxSOCKET_SERIAL_PORT_SERVER:
      CopySerialPortToFile(&dev);
      Menu();
      break;
      
    case gxSOCKET_LOCAL_FILE_SYSTEM:
      CopyFileToFile(&dev);
      Menu();
      break;
      
    default:
      break;
  }
}

int main(int argc, char **argv)
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  // 10/31/2026: Stream server options
  for(int i = 1; i < argc; i++) {
    if(strcmp(argv[i], "--cert") == 0 && i + 1 < argc) opt_cert = argv[++i];
    else if(strcmp(argv[i], "--key") == 0 && i + 1 < argc) opt_key = argv[++i];
    else if(strcmp(argv[i], "--client-ca") == 0 && i + 1 < argc) {
      opt_client_ca = argv[++i];
    }
    else if(strcmp(argv[i], "--allow-fingerprint") == 0 && i + 1 < argc &&
	    num_fp < 16) {
      opt_fp[num_fp++] = argv[++i];
    }
    else if(strcmp(argv[i], "--allow-cn") == 0 && i + 1 < argc && 
	    num_cn < 16) {
      opt_cn[num_cn++] = argv[++i];
    }
    else if(strcmp(argv[i], "--open") == 0) opt_open = 1;
    else if(strcmp(argv[i], "--allow-remote-shutdown") == 0) {
      opt_allow_shutdown = 1;
    }
    else {
      cout << "Usage: " << argv[0] << " [--cert server.pem --key server.key]"
	   << " [--client-ca ca.pem] [--allow-fingerprint SHA256]..."
	   << " [--allow-cn NAME]... [--open] [--allow-remote-shutdown]"
	   << "\n" << flush;
      return 1;
    }
  }
  if(opt_open && (opt_client_ca || num_fp || num_cn)) {
    cout << "--open cannot be used with --client-ca, --allow-fingerprint"
	 << " or --allow-cn" << "\n" << flush;
    return 1;
  }
  if((opt_cert == 0) != (opt_key == 0)) {
    cout << "--cert and --key go together" << "\n" << flush;
    return 1;
  }

  Menu();
  
  char key;
  int rv = 1;
  while(rv) {
    if (!cin) {
      ClearInputStream(cin);
      if (!cin) { 
	cout << "Input stream error" << "\n" << flush;
	return 0;
      }
    }
    cout << "\n" << flush;
    cout << '>';
    cin >> key;

    if (!cin) continue;
    switch(key) {
      case 'd' : case 'D' : 
	ClearInputStream(cin);
	SetupServer(gxSOCKET_DATAGRAM_SERVER);
	break;
	
      case 'f' : case 'F' : 
	ClearInputStream(cin);
	SetupServer(gxSOCKET_LOCAL_FILE_SYSTEM);
	break;
	
      case 'p' : case 'P' : 
	ClearInputStream(cin);
	SetupServer(gxSOCKET_SERIAL_PORT_SERVER);
	break;
	
      case 's' : case 'S' : 
	ClearInputStream(cin);
	SetupServer(gxSOCKET_STREAM_SERVER);
	break;

      case 'q' : case 'Q' :
	rv = 0;
	break;

      case '?' :
	Menu();
	break;

      default:
        cout << "Unrecognized command" << "\n" << flush;
	cout << "\n" << flush;
    }
  }
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
