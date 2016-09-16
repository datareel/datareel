// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
// Date Last Modified: 09/16/2016
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
  
  switch(ldm_feed_type) {
    
    case NONE:
      s = "NONE";
      break;

    case FT0: // case PPS:
	s = "FT0,PPS";
	break;
      
    case FT1: // case DDS:
      s = "FT1,DDS";
      break;
      
    case DDPLUS:
      s = "DDPLUS";
      break;
      
    case FT2: // case HDS: case HRS:
      s = "FT2,HDS,HRS";
      break;
      
    case FT3: // case IDS: case INTNL:
      s = "FT3,IDS,INTNL";
      break;
      
    case FT4: // case SPARE:
      s = "FT4,SPARE";
      break;
      
    case WMO:
      s = "WMO";
      break;
      
    case FT5: //case UNIWISC: case MCIDAS:
      s = "FT5,UNIWISC,MCIDAS";
      break;
      
    case UNIDATA:
      s = "UNIDATA";
      break;
      
    case FT6: // case PCWS: case ACARS:
      s = "FT6,PCWS,ACARS";
      break;
      
    case FT7: // case FSL2: case PROFILER:
      s = "FT7,FSL2,PROFILER";
      break;
      
    case FT8: // case FSL3:
      s = "FT8,FSL3";
      break;
      
    case FT9: // case FSL4:
      s = "FT9,FSL4";
      break;
      
    case FT10: // case FSL5:
      s = "FT10,FSL5";
      break;
      
    case FSL:
      s = "FSL";
      break;
      
    case FT11: // case AFOS: case GPSSRC:
      s = "FT11,AFOS,GPSSRC";
      break;
      
    case FT12: // case NMC2: case NCEPH:
      s = "FT12,NMC2,NCEPH";
      break;
      
    case FT13: // case NMC3: case FNEXRAD:
      s = "FT13,NMC3,FNEXRAD";
      break;
      
    case NMC:
      s = "NMC";
      break;
      
    case FT14: // case NLDN:
      s = "FT14,NLDN";
      break;
      
    case FT15: // case WSI:
      s = "FT15,WSI";
      break;
      
    case FT16: // case DIFAX:
      s = "FT16,DIFAX";
      break;
      
    case FT17: // case FAA604:
      s = "FT17,FAA604";
      break;
      
    case FT18: // case GPS:
      s = "FT18,GPS";
      break;
      
    case FT19: // case SEISMIC: case NOGAPS: case FNMOC:
      s = "FT19,SEISMIC,NOGAPS,FNMOC";
      break;
      
    case FT20: // case CMC: case GEM:
      s = "FT20,CMC,GEM";
      break;
      
    case FT21: // case NIMAGE: case IMAGE:
      s = "FT21,NIMAGE,IMAGE";
      break;
      
    case FT22: // case NTEXT: case TEXT:
      s = "FT22,NTEXT,TEXT";
      break;
      
    case FT23: // case NGRID: case GRID:
      s = "FT23,NGRID,GRID";
      break;
      
    case FT24: // case NPOINT: case POINT: case NBUFR: case BUFR:
      s = "FT24,NPOINT,POINT,NBUFR,BUFR";
      break;
      
    case FT25: // case NGRAPH: case GRAPH:
      s = "FT25,NGRAPH,GRAPH";
      break;
      
    case FT26: // case NOTHER: case OTHER:
      s = "FT26,NOTHER,OTHER";
      break;
      
    case NPORT:
      s = "NPORT";
      break;
      
    case FT27: // case NNEXRAD: case NEXRAD:
      s = "FT27,NNEXRAD,NEXRAD";
      break;
      
    case FT28: // case NEXRD2:
      s = "FT28,NEXRD2";
      break;
      
    case FT29: // case NXRDSRC:
      s = "FT29,NXRDSRC";
      break;
      
    case EXP: // case FT30:
      s = "EXP,FT30";
      break;
      
    case ANY:
      s = "ANY";
      break;

    default:
	s = "UNKNOWN";
      error_level = -1;
      break;
  }

  return error_level;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
