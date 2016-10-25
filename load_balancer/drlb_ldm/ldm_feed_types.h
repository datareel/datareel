// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
// Date Last Modified: 10/25/2016
// Copyright (c) 2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ---------- Include File Description and Details  ---------- // 
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

LDM feed type for LDM load balancer

*/
// ----------------------------------------------------------- //   
#ifndef __LDM_FEED_TYPES_HPP__
#define __LDM_FEED_TYPES_HPP__

#include "gxdlcode.h"

#define	NONE 0x00
#define	FT0 0x01
#define	PPS 0x01
#define	FT1 0x02
#define	DDS 0x02
#define	DDPLUS 0x03
#define	FT2 0x04
#define	HDS 0x04
#define	HRS 0x04
#define	FT3 0x08
#define	IDS 0x08
#define	INTNL 0x08
#define	FT4 0x10
#define	SPARE 0x10
#define	WMO 0x0F
#define	FT5 0x20
#define	UNIWISC 0x20
#define	MCIDAS 0x20
#define	UNIDATA 0x2F
#define	FT6 0x40
#define	PCWS 0x40
#define	ACARS 0x40
#define	FT7 0x80
#define	FSL2 0x80
#define	PROFILER 0x80
#define	FT8 0x100
#define	FSL3 0x100
#define	FT9 0x200
#define	FSL4 0x200
#define	FT10 0x400
#define	FSL5 0x400
#define	FSL 0x7C0
#define	FT11 0x800
#define	AFOS 0x800
#define	GPSSRC 0x800
#define	FT12 0x1000
#define	NMC2 0x1000
#define	NCEPH 0x1000
#define	FT13 0x2000
#define	NMC3 0x2000
#define	FNEXRAD 0x2000
#define	NMC 0x3800
#define	FT14 0x4000
#define	NLDN 0x4000
#define	FT15 0x8000
#define	WSI 0x8000
#define	FT16 0x10000
#define	DIFAX 0x10000
#define	FT17 0x20000
#define	FAA604 0x20000
#define	FT18 0x40000
#define	GPS 0x40000
#define	FT19 0x80000
#define	SEISMIC 0x80000
#define	NOGAPS 0x80000
#define	FNMOC 0x80000
#define	FT20 0x100000
#define	CMC 0x100000
#define	GEM 0x100000
#define	FT21 0x200000
#define	NIMAGE 0x200000
#define	IMAGE 0x200000
#define	FT22 0x400000
#define	NTEXT 0x400000
#define	TEXT 0x400000
#define	FT23 0x800000
#define	NGRID 0x800000
#define	GRID 0x800000
#define	FT24 0x1000000
#define	NPOINT 0x1000000
#define	POINT 0x1000000
#define	NBUFR 0x1000000
#define	BUFR 0x1000000
#define	FT25 0x2000000
#define	NGRAPH 0x2000000
#define	GRAPH 0x2000000
#define	FT26 0x4000000
#define	NOTHER 0x4000000
#define	OTHER 0x4000000
#define	NPORT 0x7C00000
#define	FT27 0x8000000
#define NEXRAD3 0x8000000
#define	NNEXRAD 0x8000000
#define	NEXRAD 0x8000000
#define	FT28 0x10000000
#define CRAFT 0x10000000
#define	NEXRD2 0x10000000
#define	FT29 0x20000000
#define	NXRDSRC 0x20000000
#define	FT30 0x40000000
#define	EXP 0x40000000
#define	ANY 0xffffffff

#endif // __LDM_FEED_TYPES_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
