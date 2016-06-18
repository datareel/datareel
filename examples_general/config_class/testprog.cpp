// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/19/1996
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

This is a test program for the gxConfig class.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include <stdio.h>
#include "gxconfig.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

// Data structure use to store program gxConfiguration values
struct ProgramgxConfig
{
  ProgramgxConfig() { // Set the default values
    s_val1 = "YES";
    s_val2 = "NO";
    s_val3 = "TRUE";
    s_val4 = "FALSE";
    i_val = 32768;
    l_val = 2147483647;
    d_val = 1.12345;
  }
  ProgramgxConfig(const ProgramgxConfig &ob) {
    s_val1 = ob.s_val1;
    s_val2 = ob.s_val2;
    s_val3 = ob.s_val3;
    s_val4 = ob.s_val4;
    i_val = ob.i_val; l_val = ob.l_val; d_val = ob.d_val;
  }

  gxString s_val1;
  gxString s_val2;
  gxString s_val3;
  gxString s_val4;
  int i_val;
  long l_val;
  double d_val; 
};

void PausePrg()
{
  cin.get();
  cout << "Press enter to continue..." << "\n";
  cout << "\n";
}

void DisplayConfig(gxConfig *CfgData, ProgramgxConfig *cfg)
// Display the program gxConfiguration and load gxConfig values
{
  char *s1 = CfgData->GetStrValue("String1");
  if(!s1) {
    cout << "String1 section missing from gxConfig file" << "\n";
  }
  else {
    cout << "String1 section of gxConfig file = " << s1 << "\n";
    cfg->s_val1 = s1;
  }

  char *s2 = CfgData->GetStrValue("String2");
  if(!s2) {
    cout << "String2 section missing from gxConfig file" << "\n";
  }
  else {
    cout << "String2 section of gxConfig file = " << s2 << "\n";
    cfg->s_val2 = s2;
  }

  char *s3 = CfgData->GetStrValue("String3");
  if(!s3) {
    cout << "String3 section missing from gxConfig file" << "\n";
  }
  else {
    cout << "String3 section of gxConfig file = " << s3 << "\n";
    cfg->s_val3 = s3;
  }

  char *s4 = CfgData->GetStrValue("String4");
  if(!s4) {
    cout << "String4 section missing from gxConfig file" << "\n";
  }
  else {
    cout << "String4 section of gxConfig file = " << s4 << "\n";
    cfg->s_val4 = s4;
  }

  int i1 = CfgData->GetIntValue("Int");
  if(!i1) {
    cout << "Int section missing from gxConfig file" << "\n";
  }
  else {
    cout << "Integer section of gxConfig file = " << i1 << "\n";
    cfg->i_val = i1;
  }

  double d1 = CfgData->GetFloatValue("Float");
  if(!d1) {
    cout << "Float section missing from gxConfig file" << "\n";
  }
  else {
    cout << "Floating Point number section of gxConfig file = "
	 << d1 << "\n";
    cfg->d_val = d1;
  }
  
  long l1 = CfgData->GetLongValue("Long");
  if(!l1) {
    cout << "Long int section missing from gxConfig file" << "\n";
  }
  else {
    cout << "Long int section of gxConfig file = " << l1 << "\n";
    cfg->l_val = l1;
  }

  cout << "\n";
}

void MultipleRead(gxConfig *CfgData)
{
  char *ServerName = CfgData->GetStrValue("ServerName");
  if(!ServerName) {
    cout << "ServerName value is not set in the gxConfig file" << "\n";
    return;
  }
  else
    cout << "Server name = " << ServerName << "\n";
  
  char *FullPathName = CfgData->GetStrValue("FullPathName0");
  if(!FullPathName) {
    cout << "No path names are set in the gxConfig file" << "\n";
    return;
  }

  char cfgValue[255];
  long file_num = -1;
  
  while(1) {
    file_num++;
    sprintf(cfgValue, "FullPathName%d", (int)file_num);
    char *buf = CfgData->GetStrValue(cfgValue);
    if(!buf) break;

    gxString sbuf(buf);
    int offset = 0;
    int index = 0;
    while(1) {
    offset = sbuf.Find("/", offset);
    if(offset != -1) index = offset;
    if(offset == -1) break;
    offset++;
    }
    if(index > 0) sbuf.DeleteAt(0, ++index);

    if(sbuf.length() == 0) {
      cout << "Invalid file name for " << cfgValue << "\n";
      return;
    }
    
    cout << "Reading: " << buf << "\n";
    cout << "Copying to: " << sbuf.c_str() << "\n";

    buf = 0;
    for(int i = 0; i < 255; i++) cfgValue[i] = 0;
  }
  cout << "\n";
}

void NewParmID(gxConfig *CfgData)
{
  CfgData->UnLoad();
  CfgData->SetParmID("://");
  gxString parmName("http");
  CfgData->ReadComments();
  CfgData->ReLoad();
  
  gxConfigListNode *ptr = (gxConfigListNode *)CfgData->GetHead();
  while(ptr) { // Scan until end of list
    int offset = 0;
    offset = ptr->node_data.Find(parmName);
    if(offset != -1) {
      gxConfigListNode *sptr = (gxConfigListNode *)ptr->next;
      gxString ServerName(sptr->node_data);
      gxString FullPathName(sptr->node_data);
      offset = ServerName.Find("/");
      if(offset != -1) {
	ServerName.DeleteAt(offset, (ServerName.length() - offset));
	FullPathName.DeleteAt(0, offset);
	cout << "Server: " << ServerName.c_str() << " Path: "
	     << FullPathName.c_str() << "\n";
      }
    }
    ptr = (gxConfigListNode *)ptr->next;
  }
}

int main()
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  gxConfig *CfgData = new gxConfig;
  ProgramgxConfig *cfg = new ProgramgxConfig;
  int Status;

  // Test for all three overloads
  const char *fname = "gxconfig.ini";
  
  cout << "Loading gxConfig File..." << "\n";
  Status = CfgData->Load(fname);
  if(!Status) {
    cout << "gxConfig file not found!" << "\n";
    delete CfgData;
    delete cfg;
    return 0;
  }
  else
    cout << "Processing file: " << CfgData->GetFileName() << "\n";

  DisplayConfig(CfgData, cfg);
  ProgramgxConfig org_gxConfig(*cfg); // Record the original values
  
  cout << "Testing reload function" << "\n";
  PausePrg();
  CfgData->ReLoad(fname);
  DisplayConfig(CfgData, cfg);

  cout << "Writing new values to the file" << "\n";
  PausePrg();
  CfgData->ChangeConfigValue("String1", "no");
  CfgData->ChangeConfigValue("String2", "yes");
  CfgData->ChangeConfigValue("String3", "false");
  CfgData->ChangeConfigValue("String4", "true");
  
  CfgData->ChangeConfigValue("Float", 1.98712);
  CfgData->ChangeConfigValue("Int", 12345);
  CfgData->ChangeConfigValue("Long", 23889);
  DisplayConfig(CfgData, cfg);
  
  cout << "Restoring original values" << "\n";
  PausePrg();
  CfgData->ChangeConfigValue("String1", org_gxConfig.s_val1);
  CfgData->ChangeConfigValue("String2", org_gxConfig.s_val2);
  CfgData->ChangeConfigValue("String3", org_gxConfig.s_val3);
  CfgData->ChangeConfigValue("String4", org_gxConfig.s_val4);
  CfgData->ChangeConfigValue("Int", org_gxConfig.i_val);
  CfgData->ChangeConfigValue("Float", org_gxConfig.d_val);
  CfgData->ChangeConfigValue("Long", org_gxConfig.l_val);
  DisplayConfig(CfgData, cfg);

  cout << "Performing a multiple read and parsing operation" << "\n";
  PausePrg();
  MultipleRead(CfgData);

  cout << "Reloading the file with a new parameter ID string" << "\n";
  PausePrg();
  NewParmID(CfgData);
  
  CfgData->UnLoad();

  delete CfgData;
  delete cfg;
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
