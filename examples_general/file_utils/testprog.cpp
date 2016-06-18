// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 01/25/2000
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

Test program used to test the file and directory utility functions.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "futils.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

void PausePrg() 
{
  cout << "\n";
  cout << "Press enter to continue..." << "\n";
  cin.get();
}

void DirectoryFunctionTest()
{
  cout << "Testing general purpose directory functions" 
       << "\n";

  const char *dirname = "testdir"; // Directory to be created

  // Directory immediately above the current directory position
  const char *parentdir = "..";    
  
  cout << "Creating " << dirname << " directory" <<"\n";
  if(futils_mkdir(dirname) != 0) {
    cout << "Error creating the directory" << "\n";
    return;
  }

  if((futils_exists(dirname)) && (futils_isdirectory(dirname))) {
    cout << "The " << dirname << " directory was created" << "\n";
  }

  cout << "Changing directory to " << dirname << " directory" <<"\n";
  if(futils_chdir(dirname) != 0) {
    cout << "Error changing directory" << "\n";
    return;
  }

  char *pwd[futils_MAX_DIR_LENGTH]; char drive_letter;
  if(futils_getcwd((char *)pwd, futils_MAX_DIR_LENGTH) == 0) {
    cout << "Present working directory: " << (char *)pwd << "\n";
    if(futils_hasdriveletter((const char *)pwd, &drive_letter)) {
      cout << "DOS Drive letter = " << drive_letter << "\n";
    }
  }

  cout << "Changing directory to " << parentdir << " directory" <<"\n";
  if(futils_chdir(parentdir) != 0) {
    cout << "Error changing directory" << "\n";
    return;
  }

  if(futils_getcwd((char *)pwd, futils_MAX_DIR_LENGTH) == 0) {
    cout << "Present working directory: " << (char *)pwd << "\n";
    if(futils_hasdriveletter((const char *)pwd, &drive_letter)) {
      cout << "DOS Drive letter = " << drive_letter << "\n";
    }
  }

  cout << "Removing " << dirname << " directory" <<"\n";
  if(futils_rmdir(dirname) != 0) {
    cout << "Error removing directory" << "\n";
    return;
  }
}

int main(int argc, char *argv[])
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  DIR* dirp;
  dirent* direntp;
  char *directory = argv[1]; 

  if(argc < 2) {
    cout << "Usage: " << argv[0] << " directory" << "\n";
#if defined (__WIN32__)
    cout << "Example: " << argv[0] << " C:\\temp" << "\n";
#elif defined (__UNIX__)
    cout << "Example: " << argv[0] << " /tmp" << "\n";
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif
    return 1;
  }

  // Test the general-purpose directory stream functions  
  cout << "Testing general purpose directory stream functions" << "\n";
  cout << "Opening the specified directory and list all files" << "\n";
  cout << "\n";
  dirp = futils_opendir(directory);
  if(dirp == NULL) {
    cout << "Cannot open " << directory << "\n";
    return 1;
  } 
  else {
    for(;;) {
      dirent entry;
      direntp = futils_readdir(dirp, &entry);
      if(direntp == NULL) break;
      cout << direntp->d_name << "\n" << flush;
    }
  }
  cout << "\n";

  PausePrg();
  
  cout << "Testing the rewind function" << "\n";
  cout << "\n";
  futils_rewinddir(dirp);
  for(;;) {
    dirent entry;
    direntp = futils_readdir(dirp, &entry);
    if(direntp == NULL) break;
    cout << direntp->d_name << "\n" << flush;
  }
  cout << "\n";

  futils_closedir(dirp);

  PausePrg();
  
  // Test the general-purpose directory functions  
  DirectoryFunctionTest();

  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

