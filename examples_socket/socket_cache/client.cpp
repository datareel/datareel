// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: client.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
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

This is a test program for the device cache classes.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "gxsfile.h"

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

  if(client.StreamClient(port, host) != 0) {
    cout << client.SocketExceptionMessage() << "\n" << flush;
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

int main()
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

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
