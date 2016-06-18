// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/23/2001
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

Test program for the embedded FTP client class.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include <stdlib.h>
#include "gxsftp.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

int CheckError(gxsFTPClient *ftp, int close_on_err = 1)
{
  if(ftp->GetSocketError() != gxSOCKET_NO_ERROR) {
    cout << ftp->SocketExceptionMessage() << "\n" << flush;
    if(ftp->reply_buf[0] != 0) { // Check the reply buffer
      cout << ftp->reply_buf << flush;
    }
    if(close_on_err == 1) {
      ftp->Close();
    }
    return 0;
  }
  return 1; // No errors reported
}

void ClearInputStream(istream &s)
// Used to clear istream
{
  char c;
  s.clear();
  while(s.get(c) && c != '\n') { ; }
}

void Menu()
{
  cout << "\n" << flush;
  cout << "(a)       Set image type to ASCII" << "\n" << flush;   
  cout << "(A)       Set active mode on (default)" << "\n" << flush;
  cout << "(b, B)    Set image type to binary" << "\n" << flush;   
  cout << "(c, C)    Change working directory" << "\n" << flush;
  cout << "(d, D)    Delete a file" << "\n" << flush;
  cout << "(h, H, ?) Display this menu" << "\n" << flush;
  cout << "(g, G)    Get a file" << "\n" << flush;
  cout << "(l)       Long directory listing" << "\n" << flush;
  cout << "(L)       Short directory listing" << "\n" << flush;
  cout << "(M, m)    Make a directory" << "\n" << flush;   
  cout << "(n)       Long named directory listing" << "\n" << flush;
  cout << "(N)       Short named directory listing" << "\n" << flush;
  cout << "(p)       Put a file" << "\n" << flush;
  cout << "(P)       Set passive FTP mode on" << "\n" << flush;
  cout << "(q, Q)    Quit this program" << "\n" << flush;
  cout << "(r, R)    Remove directory" << "\n" << flush;
  cout << "(s, S)    Display statistics" << "\n" << flush;
  cout << "(v, V)    Move a file" << "\n" << flush;   
  cout << "(w, W)    Get working directory" << "\n" << flush;   
  cout << "(z, Z)    Get a file size in bytes" << "\n" << flush;   
  cout << "\n" << flush;
}

void List(gxsFTPClient *ftp, int full)
{
  const int sbuf_size = 8192;
  char sbuf[sbuf_size];
  sbuf[0] = 0;
  ftp->FTPList(sbuf, sizeof(sbuf), full);
  if(!CheckError(ftp, 0)) return;
  cout << sbuf << flush << "\n" << flush;
  cout << ftp->reply_buf << flush;
}

void ListDir(gxsFTPClient *ftp, int full)
{
  char name[255];
  cout << "Enter file or directory name>";
  cin >> name;

  const int sbuf_size = 8192;
  char sbuf[sbuf_size];
  sbuf[0] = 0;
  ftp->FTPList(sbuf, sizeof(sbuf), full, name);
  if(!CheckError(ftp, 0)) return;
  cout << sbuf << flush << "\n" << flush;
  cout << ftp->reply_buf << flush;
}

void Stat(gxsFTPClient *ftp)
{
  ftp->FTPStat();
  if(!CheckError(ftp, 0)) return;
  cout << ftp->reply_buf << flush;
}

void PWD(gxsFTPClient *ftp)
{
  ftp->FTPPWD();
  if(!CheckError(ftp, 0)) return;
  cout << ftp->reply_buf << flush;
}

void ASC(gxsFTPClient *ftp)
{
  ftp->FTPImageType('A');
  if(!CheckError(ftp, 0)) return;
  cout << ftp->reply_buf << flush;
}

void Active(gxsFTPClient *ftp)
{
  ftp->FTPActive();
  cout << "Passive mode off" << "\n" << flush;
}

void Passive(gxsFTPClient *ftp)
{
  ftp->FTPPassive();
  cout << "Passive mode on" << "\n" << flush;
}

void BIN(gxsFTPClient *ftp)
{
  ftp->FTPImageType('B');
  if(!CheckError(ftp, 0)) return;
  cout << ftp->reply_buf << flush;
}

void ChDir(gxsFTPClient *ftp)
{
  char sbuf[255];
  cout << "Enter directory name>";
  cin >> sbuf;
  ftp->FTPChDir((const char *)sbuf);
  if(!CheckError(ftp, 0)) return;
  cout << ftp->reply_buf << flush;
}

void MkDir(gxsFTPClient *ftp)
{
  char sbuf[255];
  cout << "Enter directory to make>";
  cin >> sbuf;
  ftp->FTPMkDir((const char *)sbuf);
  if(!CheckError(ftp, 0)) return;
  cout << ftp->reply_buf << flush;
}

void RmDir(gxsFTPClient *ftp)
{
  char sbuf[255];
  cout << "Enter directory to remove>";
  cin >> sbuf;
  ftp->FTPRmDir((const char *)sbuf);
  if(!CheckError(ftp, 0)) return;
  cout << ftp->reply_buf << flush;
}

void Delete(gxsFTPClient *ftp)
{
  char sbuf[255];
  cout << "Enter file name to delete>";
  cin >> sbuf;
  ftp->FTPDelete((const char *)sbuf);
  if(!CheckError(ftp, 0)) return;
  cout << ftp->reply_buf << flush;
}

void Move(gxsFTPClient *ftp)
{
  char from[255]; char to[255];
  cout << "Enter file name to rename>";
  cin >> from;
  cout << "Enter new name>";
  cin >> to;
  ftp->FTPMove((const char *)from, (const char *)to);
  if(!CheckError(ftp, 0)) return;
  cout << ftp->reply_buf << flush;
}

void Size(gxsFTPClient *ftp)
{
  char sbuf[255];
  cout << "Enter file name>";
  cin >> sbuf;
  ftp->FTPSize((const char *)sbuf);
  if(!CheckError(ftp, 0)) return;
  cout << ftp->reply_buf << flush;
}

void Get(gxsFTPClient *ftp)
{
  char rfile[255]; char lfile[255];
  cout << "Enter file name to download>";
  cin >> rfile;
  cout << "Enter local file name>";
  cin >> lfile;

  // Open the output and truncate if it already exists
  DiskFileB stream(lfile, DiskFileB::df_READWRITE, DiskFileB::df_CREATE);
  if(!stream) {
    cout << "Could not open the " << lfile << " output file" << "\n" << flush;
    return;
  }

  FAU_t bytes;
  ftp->FTPGet((const char *)rfile, stream, bytes);
  if(!CheckError(ftp, 0)) return;
  cout << ftp->reply_buf << flush;
  cout << (long)bytes << " bytes transferred" << "\n" << flush;  
}

void Put(gxsFTPClient *ftp)
{
  char rfile[255]; char lfile[255];
  cout << "Enter file name to upload>";
  cin >> lfile;
  cout << "Enter remote file name>";
  cin >> rfile;


  DiskFileB stream(lfile);
  if(!stream) {
    cout << "Could not open the " << lfile << " input file" << "\n" << flush;
    return;
  }

  FAU_t bytes;
  ftp->FTPPut((const char *)rfile, stream, bytes);
  if(!CheckError(ftp, 0)) return;
  cout << ftp->reply_buf << flush;
  cout << (long)bytes << " bytes transfered" << "\n" << flush;
}

int main(int argc, char **argv)
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  if(argc < 4) {
    cout << "Usage: " << argv[0] << " server username password [port]" 
         << "\n" << flush;
    cout << "server   = Your FTP server" << "\n" << flush;
    cout << "username = Your FTP account user name" << "\n" << flush;
    cout << "username = Your FTP account password" << "\n" << flush;
    cout << "port = Optional port number" << "\n" << flush;
    cout << "\n" << flush;
    cout << "Example: " << argv[0] 
         << " ftp.xyz.com myname mypassword" << "\n" << flush;
    return 1;
  }
  char *server = argv[1];
  char *username = argv[2];
  char *password = argv[3];
  unsigned short port = (unsigned short)21;
  if(argc == 5) port = atoi(argv[4]);

  gxsFTPClient ftp;
  ftp.SetTimeOut(60, 0); // Set a long time for remote servers

  cout << "\n" << flush;
  cout << "Connecting to ftp server" << "\n" << flush;
  ftp.ConnectClient((const char *)server, port);
  if(!CheckError(&ftp)) return 1;
  cout << ftp.reply_buf << flush;

  ftp.FTPLogin((const char *)username, (const char *)password);
  if(!CheckError(&ftp)) return 1;
  cout << ftp.reply_buf << flush;

  char key;
  Menu();
  int rv = 1;

  while(rv) {
    if (!cin) { 
      ClearInputStream(cin); 
      if (!cin) {  
        cout << "Input stream error" << "\n" << flush;
        return 0;
      }
    }
    cout << '>';
    cin >> key;
    if (!cin) continue;
    switch(key) {
      case '?' : Menu(); break;
      case 'a' : ClearInputStream(cin); ASC(&ftp); break;
      case 'A' : ClearInputStream(cin); Active(&ftp); break;
      case 'b' : case 'B' : ClearInputStream(cin); BIN(&ftp); break;
      case 'c' : case 'C' : ClearInputStream(cin); ChDir(&ftp); break;
      case 'd' : case 'D' : ClearInputStream(cin); Delete(&ftp); break;
      case 'g' : case 'G' : ClearInputStream(cin); Get(&ftp); break;
      case 'h' : case 'H' : Menu(); break;
      case 'l' : ClearInputStream(cin); List(&ftp, 1); break;
      case 'L' : ClearInputStream(cin); List(&ftp, 0); break;
      case 'm' : case 'M' : ClearInputStream(cin); MkDir(&ftp); break;
      case 'n' : ClearInputStream(cin); ListDir(&ftp, 1); break;
      case 'N' : ClearInputStream(cin); ListDir(&ftp, 0); break;
      case 'p' : ClearInputStream(cin); Put(&ftp); break;
      case 'P' : ClearInputStream(cin); Passive(&ftp); break;
      case 'q' : case 'Q' : rv = 0; break;
      case 'r' : case 'R' : ClearInputStream(cin); RmDir(&ftp); break;
      case 's' : case 'S' : ClearInputStream(cin); Stat(&ftp); break;
      case 'v' : case 'V' : ClearInputStream(cin); Move(&ftp); break;
      case 'w' : case 'W' : ClearInputStream(cin); PWD(&ftp); break;
      case 'z' : case 'Z' : ClearInputStream(cin); Size(&ftp); break;
      default:
        cout << "Unrecognized command" << "\n" << flush;
        break;
    }
  }

  ftp.FTPLogout();
  if(!CheckError(&ftp)) return 1;
  cout << ftp.reply_buf << flush;
  cout << "\n" << flush;
  ftp.Close();
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
