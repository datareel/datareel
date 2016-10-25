// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
// Date Last Modified: 10/25/2016
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

LDM protcol plug in for DR load balancer.

*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "../drlb_server/drlb_server.h"
#include "ldm_proto.h"

unsigned int LDMrequest::network_to_int(char net_int[4])
{
  unsigned int buf = 0;
  unsigned int d = (unsigned int)net_int[0];
  unsigned int c = (unsigned int)net_int[1];
  unsigned int b = (unsigned int)net_int[2];
  unsigned int a = (unsigned int)net_int[3];
  a = a & 0xFF;
  b = (b<<8) & 0xFF00;
  c = (c<<16) & 0xFF0000;
  d = (d<<24) & 0xFF000000;
  buf = a + b + c + d;
  return buf;
}

void LDMrequest::SetFeedType(char net_int[4]) 
{
  feed_type = network_to_int(net_int);
}

int set_ldm_request_string(int ldm_request, gxString &s)
{
  s.Clear();
  int error_level = 0;

  switch(ldm_request) {
    case 0:
      s = "NONE";
      break;
    case HEREIS:
      s = "HEREIS";
      break;
    case FEEDME:
      s = "FEEDME";
      break;
    case HIYA: 
      s = "HIYA";
      break;
    case NOTIFICATION:
      s = "NOTIFICATION";
      break;
    case NOTIFYME:
      s = "NOTIFYME";
      break;
    case COMINGSOON:
      s = "COMINGSOON";
      break;
    case BLKDATA:
      s = "BLKDATA";
      break;
    case IS_ALIVE:
      s = "IS_ALIVE";
      break;
    default:
      s = "UNKNOWN";
      error_level = -1;
      break;
  }

  return error_level;
}

int set_ldm_feed_type_strings(unsigned int ldm_feed_type, gxString &s)
{
  s.Clear();
  int error_level = 0;
  int has_feed_type = 0;

  if(ldm_feed_type == 0) {
    s = "NONE";
    return 1; // Cannot have NONE as a feed type
  }

  if(ldm_feed_type == ANY) {
    s = "ANY";
    return 0;
  }

  if((FT0 & ldm_feed_type) == FT0) {
    if(!has_feed_type) {
      s = "PPS,FT0";
    }
    else {
      s << "," << "PPS,FT0";
    }
    has_feed_type = 1;
  }

  if((FT1 & ldm_feed_type) == FT1) {
    if(!has_feed_type) {
      s = "DDS,FT1,DOMESTIC";
    }
    else {
      s << "," << "DDS,FT1,DOMESTIC";
    }
    has_feed_type = 1;
  }

  if((DDPLUS & ldm_feed_type) == DDPLUS) {
    if(!has_feed_type) {
      s = "DDPLUS";
    }
    else {
      s << "," << "DDPLUS";
    }
    has_feed_type = 1;
  }
  
  if((FT2 & ldm_feed_type) == FT2) {
    if(!has_feed_type) {
      s = "HDS,FT2,HRS";
    }
    else {
      s << "," << "HDS,FT2,HRS";
    }
    has_feed_type = 1;
  }
      
  if((FT3 & ldm_feed_type) == FT3) {
    if(!has_feed_type) {
      s = "IDS,FT3,INTNL";
    }
    else {
      s << "," << "IDS,FT3,INTNL";
    }
    has_feed_type = 1;
  }
      
  if((FT4 & ldm_feed_type) == FT4) {
    if(!has_feed_type) {
      s = "SPARE,FT4";
    }
    else {
      s << "," << "SPARE,FT4";
    }
    has_feed_type = 1;
  }
      
  if((WMO & ldm_feed_type) == WMO) {
    if(!has_feed_type) {
      s = "WMO";
    }
    else {
      s << "," << "WMO";
    }
    has_feed_type = 1;
  }
      
  if((FT5 & ldm_feed_type) == FT5) {
    if(!has_feed_type) {
      s = "UNIWISC,FT5,MCIDAS";
    }
    else {
      s << "," << "UNIWISC,FT5,MCIDAS";
    }
    has_feed_type = 1;
  }
  
  if((UNIDATA & ldm_feed_type) == UNIDATA) {
    if(!has_feed_type) {
      s = "UNIDATA";
    }
    else {
      s << "," << "UNIDATA";
    }
    has_feed_type = 1;
  }

  if((FT6 & ldm_feed_type) == FT6) {
    if(!has_feed_type) {
      s = "PCWS,FT6,ACARS";
    }
    else {
      s << "," << "PCWS,FT6,ACARS";
    }
    has_feed_type = 1;
  }
      
  if((FT7 & ldm_feed_type) == FT7) {
    if(!has_feed_type) {
      s = "FSL2,FT7,PROFILER";
    }
    else {
      s << "," << "FSL2,FT7,PROFILER";
    }
    has_feed_type = 1;
  }
      
  if((FT8 & ldm_feed_type) == FT8) {
    if(!has_feed_type) {
      s = "FSL3,FT8";
    }
    else {
      s << "," << "FSL3,FT8";
    }
    has_feed_type = 1;
  }
      
  if((FT9 & ldm_feed_type) == FT9) {
    if(!has_feed_type) {
      s = "FSL4,FT9";
    }
    else {
      s << "," << "FSL4,FT9";
    }
    has_feed_type = 1;
  }
      
  if((FT10 & ldm_feed_type) == FT10) {
    if(!has_feed_type) {
      s = "FSL5,FT10";
    }
    else {
      s << "," << "FSL5,FT10";
    }
    has_feed_type = 1;
  }
  
  if((FSL & ldm_feed_type) == FSL) {
    if(!has_feed_type) {
      s = "FSL";
    }
    else {
      s << "," << "FSL";
    }
    has_feed_type = 1;
  }
      
  if((FT11 & ldm_feed_type) == FT11) {
    if(!has_feed_type) {
      s = "GPSSRC,FT11,NMC1,AFOS";
    }
    else {
      s << "," << "GPSSRC,FT11,NMC1,AFOS";
    }
    has_feed_type = 1;
  }
      
  if((FT12 & ldm_feed_type) == FT12) {
    if(!has_feed_type) {
      s = "CONDUIT,FT12,NMC2,NCEPH";
    }
    else {
      s << "," << "CONDUIT,FT12,NMC2,NCEPH";
    }
    has_feed_type = 1;
  }
      
  if((FT13 & ldm_feed_type) == FT13) {
    if(!has_feed_type) {
      s = "FNEXRAD,FT13,NMC3";
    }
    else {
      s << "," << "FNEXRAD,FT13,NMC3";
    }
    has_feed_type = 1;
  }
      
  if((NMC & ldm_feed_type) == NMC) {
    if(!has_feed_type) {
      s = "NMC";
    }
    else {
      s << "," << "NMC";
    }
    has_feed_type = 1;
  }
      
  if((FT14 & ldm_feed_type) == FT14) {
    if(!has_feed_type) {
      s = "LIGHTNING,FT14,NLDN";
    }
    else {
      s << "," << "LIGHTNING,FT14,NLDN";
    }
    has_feed_type = 1;
  }
      
  if((FT15 & ldm_feed_type) == FT15) {
    if(!has_feed_type) {
      s = "WSI,FT15";
    }
    else {
      s << "," << "WSI,FT15";
    }
    has_feed_type = 1;
  }
      
  if((FT16 & ldm_feed_type) == FT16) {
    if(!has_feed_type) {
      s = "DIFAX,FT16";
    }
    else {
      s << "," << "DIFAX,FT16";
    }
    has_feed_type = 1;
  }
  
  if((FT17 & ldm_feed_type) == FT17) {
    if(!has_feed_type) {
      s = "FAA604,FT17,FAA,604";
    }
    else {
      s << "," << "FAA604,FT17,FAA,604";
    }
    has_feed_type = 1;
  }
  
  if((FT18 & ldm_feed_type) == FT18) {
    if(!has_feed_type) {
      s = "GPS,FT18";
    }
    else {
      s << "," << "GPS,FT18";
    }
    has_feed_type = 1;
  }
      
  if((FT19 & ldm_feed_type) == FT19) {
    if(!has_feed_type) {
      s = "FNMOC,FT19,SEISMIC,NOGAPS";
    }
    else {
      s << "," << "FNMOC,FT19,SEISMIC,NOGAPS";
    }
    has_feed_type = 1;
  }
      
  if((FT20 & ldm_feed_type) == FT20) {
    if(!has_feed_type) {
      s = "GEM,FT20,CMC";
    }
    else {
      s << "," << "GEM,FT20,CMC";
    }
    has_feed_type = 1;
  }
      
  if((FT21 & ldm_feed_type) == FT21) {
    if(!has_feed_type) {
      s = "NIMAGE,FT21,IMAGE";
    }
    else {
      s << "," << "NIMAGE,FT21,IMAGE";
    }
    has_feed_type = 1;
  }
  
  if((FT22 & ldm_feed_type) == FT22) {
    if(!has_feed_type) {
      s = "NTEXT,FT22,TEXT";
    }
    else {
      s << "," << "NTEXT,FT22,TEXT";
    }
    has_feed_type = 1;
  }
      
  if((FT23 & ldm_feed_type) == FT23) {
    if(!has_feed_type) {
      s = "NGRID,FT23,GRID";
    }
    else {
      s << "," << "NGRID,FT23,GRID";
    }
    has_feed_type = 1;
  }
      
  if((FT24 & ldm_feed_type) == FT24) {
    if(!has_feed_type) {
      s = "NPOINT,FT24,POINT,NBUFR,BUFR";
    }
    else {
      s << "," << "NPOINT,FT24,POINT,NBUFR,BUFR";
    }
    has_feed_type = 1;
  }
      
  if((FT25 & ldm_feed_type) == FT25) {
    if(!has_feed_type) {
      s = "NGRAPH,FT25,GRAPH";
    }
    else {
      s << "," << "NGRAPH,FT25,GRAPH";
    }
    has_feed_type = 1;
  }
      
  if((FT26 & ldm_feed_type) == FT26) {
    if(!has_feed_type) {
      s = "NOTHER,FT26,OTHER";
    }
    else {
      s << "," << "NOTHER,FT26,OTHER";
    }
    has_feed_type = 1;
  }
      
  if((NPORT & ldm_feed_type) == NPORT) {
    if(!has_feed_type) {
      s = "NPORT";
    }
    else {
      s << "," << "NPORT";
    }
    has_feed_type = 1;
  }
      
  if((FT27 & ldm_feed_type) == FT27) {
    if(!has_feed_type) {
      s = "NEXRAD3,FT27,NNEXRAD,NEXRAD";
    }
    else {
      s << "," << "NEXRAD3,FT27,NNEXRAD,NEXRAD";
    }
    has_feed_type = 1;
  }
      
  if((FT28 & ldm_feed_type) == FT28) {
    if(!has_feed_type) {
      s = "NEXRAD2,FT28,CRAFT,NEXRD2";
    }
    else {
      s << "," << "NEXRAD2,FT28,CRAFT,NEXRD2";
    }
    has_feed_type = 1;
  }
  
  if((FT29 & ldm_feed_type) == FT29) { 
    if(!has_feed_type) {
      s = "NXRDSRC,FT29";
    }
    else {
      s << "," << "NXRDSRC,FT29";
    }
    has_feed_type = 1;
  }
      
  if((EXP & ldm_feed_type) == EXP) { 
    if(!has_feed_type) {
      s = "EXP,FT30";
    }
    else {
      s << "," << "EXP,FT30";
    }
    has_feed_type = 1;
  }
      
  if(!has_feed_type) {
    s = "UNKNOWN";
    error_level = -1;
  }

  return error_level;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
