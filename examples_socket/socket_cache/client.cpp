// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: client.cpp
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

10/31/2026: Stream client TLS options (DataReel 6.3, the same as the
DataReel 7.3.1 stream_client example):

  client [--ca ca.pem] [--insecure] [--cert client.pem --key client.key]

--ca or --cert turns on TLS. --ca is the CA that issued the server
certificate (the system store is used without it); --insecure skips
the server certificate check. --cert/--key is the client certificate
for servers that require one.
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

// 10/31/2026: Stream client options, see above
static const char *opt_ca = 0, *opt_cert = 0, *opt_key = 0;
static int opt_insecure = 0;

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
  cout << "(D)   Copy a disk file to a Datagram Socket" << "\n" << flush;
  cout << "(F)   Copy a disk file to another disk file" << "\n" << flush;
  cout << "(P)   Copy a disk file to a Serial port" << "\n" << flush;
  cout << "(Q)   Quit this program" << "\n" << flush;
  cout << "(S)   Copy a disk file to a Stream Socket" << "\n" << flush;
  cout << "\n" << flush;
}

void CopyFileToSerialPort(gxsFile *dev) 
{
  MemoryBuffer in, port;
  InputString("Enter the name of the serial device", port);
  InputString("Enter the name of file to copy", in);
  
  if(!dev->OpenInputFile(in)) {
    cout << "Cannot open the specified file!" << "\n" << flush;
    return;
  }
  
  FAU_t byte_count = (FAU_t)0;
  gxSerialCommServer client;
  
  int rv = client.InitCommServer(port);
  if(rv != gxSerialComm::scomm_NO_ERROR) {
    cout << client.SerialCommExceptionMessage() << "\n" << flush;
    return;
  }
  
  if(!dev->CopyFileToSerialPort(&client, byte_count)) {
    cout << "Error copying file." << "\n" << flush;
    return;
  }
  
  cout << "Number of cache buckets in use = " << dev->BucketsInUse() << "\n" 
       << flush;
  cout << "Flushing the device cache..." << "\n" << flush;
  dev->Flush();
  dev->CloseInputFile();
  
  // Shutdown the server
  client.TerminateConnection();
  
  cout << "Finished processing file." << "\n" << flush;
  cout << "Byte count = " << (long)byte_count << "\n" << flush;
}

void CopyFileToStreamSocket(gxsFile *dev)
{
  gxStream client;
  int port;
  MemoryBuffer in, host;

  InputString("Enter the name of the server", host);
  InputInt("Enter the server's port number", port);
  InputString("Enter the name of file to copy", in);

  if(!dev->OpenInputFile(in)) {
    cout << "Cannot open the specified file!" << "\n" << flush;
    return;
  }

  FAU_t byte_count = (FAU_t)0;

  // 10/31/2026: TLS options
  if(opt_ca || opt_cert || opt_insecure) client.UseTLS(1);
  if(opt_ca) client.SetServerCAFile(opt_ca);
  if(opt_insecure) client.SetVerifyServer(0);
  if(opt_cert) client.SetClientCert(opt_cert, opt_key);

  if(client.StreamClient(port, host) != 0) {
    cout << client.SocketExceptionMessage() << "\n" << flush;
    if(*client.TLSError()) cout << client.TLSError() << "\n" << flush;
    client.Close();
    return;
  }
  
  if(!dev->CopyFileToStreamSocket(&client, byte_count)) {
    cout << "Error copying file." << "\n" << flush;
    return;
  }

  cout << "Number of cache buckets in use = " << dev->BucketsInUse() << "\n" 
       << flush;
  cout << "Flushing the device cache..." << "\n" << flush;
  dev->Flush();
  dev->CloseInputFile();
  
  // Shutdown the server
  client.TerminateConnection();
  
  cout << "Finished processing file." << "\n" << flush;
  cout << "Byte count = " << (long)byte_count << "\n" << flush;
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

void CopyFileToDatagramSocket(gxsFile *dev)
{
  gxDatagram client;
  int port;
  MemoryBuffer in, host;

  InputString("Enter the name of the server", host);
  InputInt("Enter the server's port number", port);
  InputString("Enter the name of file to copy", in);
  
  if(!dev->OpenInputFile(in)) {
    cout << "Cannot open the specified file!" << "\n" << flush;
    return;
  }

  FAU_t byte_count = (FAU_t)0;

  if(client.DatagramClient(port, host) != 0) {
    cout << client.SocketExceptionMessage() << "\n" << flush;
    client.Close();
    return;
  }

  if(!dev->CopyFileToDatagramSocket(&client, byte_count)) {
    cout << "Error copying file." << "\n" << flush;
    return;
  }

  cout << "Number of cache buckets in use = " << dev->BucketsInUse() << "\n" 
       << flush;
  cout << "Flushing the device cache..." << "\n" << flush;
  dev->Flush();
  dev->CloseInputFile();

  // Shutdown the server
  client.TerminateConnection();

  cout << "Finished processing file." << "\n" << flush;
  cout << "Byte count = " << (long)byte_count << "\n" << flush;
}

void SetupClient(int client_type)
{
  int cache_size = 10;
  gxsFile dev(cache_size); // Device cache used to process a file

  cout << "Creating a device cache using " << cache_size 
       << " cache buckets." << "\n" << flush;
  cout << "Reserving " << (sizeof(gxDeviceBucket) * cache_size) 
       << " bytes of memory." << "\n" << flush;
  cout << "Number of cache buckets in use = " << dev.BucketsInUse() << "\n" 
       << flush;
  cout << "\n" << flush;
  
  switch(client_type) {
    case gxSOCKET_STREAM_CLIENT:
      CopyFileToStreamSocket(&dev);
      Menu();
      break;
      
    case gxSOCKET_DATAGRAM_CLIENT:
      CopyFileToDatagramSocket(&dev);
      Menu();
      break;

    case gxSOCKET_SERIAL_PORT_CLIENT:
      CopyFileToSerialPort(&dev);
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

  // 10/31/2026: Stream client options
  for(int i = 1; i < argc; i++) {
    if(strcmp(argv[i], "--ca") == 0 && i + 1 < argc) opt_ca = argv[++i];
    else if(strcmp(argv[i], "--insecure") == 0) opt_insecure = 1;
    else if(strcmp(argv[i], "--cert") == 0 && i + 1 < argc) opt_cert = argv[++i];
    else if(strcmp(argv[i], "--key") == 0 && i + 1 < argc) opt_key = argv[++i];
    else {
      cout << "Usage: " << argv[0] << " [--ca ca.pem] [--insecure]"
	   << " [--cert client.pem --key client.key]" << "\n" << flush;
      return 1;
    }
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
	SetupClient(gxSOCKET_DATAGRAM_CLIENT);
	break;
	
      case 'f' : case 'F' : 
	ClearInputStream(cin);
	SetupClient(gxSOCKET_LOCAL_FILE_SYSTEM);
	break;
	
      case 'p' : case 'P' :
	ClearInputStream(cin);
	SetupClient(gxSOCKET_SERIAL_PORT_CLIENT);
	break;
	
      case 's' : case 'S' :
	ClearInputStream(cin);
	SetupClient(gxSOCKET_STREAM_CLIENT);
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
