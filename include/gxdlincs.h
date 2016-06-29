// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxdlincs.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 05/25/2001  
// Date Last Modified: 06/29/2016
// Copyright (c) 2001-2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ---------- Include File Description and Details  ---------- // 
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

GX code include files referenced in the stdafx.h file.

Changes:
==============================================================
01/23/2002: Added the __MSVC_DLL__ preprocessor directive, which 
is now required to compile the DLL library. This directive was added 
to allow the DLL code base to be combined with the UNIX library and 
static library code bases. 
==============================================================
*/
// ----------------------------------------------------------- // 
#ifndef __GX_DLL_CODE_INCLUDE_HPP__
#define __GX_DLL_CODE_INCLUDE_HPP__

#include "gxdlcode.h"

#ifdef __MSVC_DLL__

#include "asprint.h"
#include "bstnode.h"
#include "bstreei.h"
#include "btnode.h"
#include "btstack.h"
#include "cdate.h"
#include "dbasekey.h"
#include "dbugmgr.h"
#include "devcache.h"
#include "devtypes.h"
#include "dfileb.h"
#include "ehandler.h"
#include "fstring.h"
#include "futils.h"
#include "gpersist.h"
#include "gthreadt.h"
#include "gxbstree.h"
#include "gxbtree.h"
#include "gxcond.h"
#include "gxconfig.h"
#include "gxcrc32.h"
#include "gxcrctab.h"
#include "gxdatagm.h"
#include "gxdbase.h"
#include "gxderror.h"
#include "gxdfp64.h"
#include "gxdfptr.h"
#include "gxdstats.h"
#include "gxdtyp64.h"
#include "gxdtypes.h"
#include "gxd_ver.h"
#include "gxfloat.h"
#include "gxheader.h"
#include "gxint16.h"
#include "gxint32.h"
#include "gxint64.h"
#include "gxip32.h"
#include "gxlist.h"
#include "gxlistb.h"
#include "gxmac48.h"
#include "gxmutex.h"
#include "gxrdbdef.h"
#include "gxrdbhdr.h"
#include "gxrdbms.h"
#include "gxrdbsql.h"
#include "gxscomm.h"
#include "gxsema.h"
#include "gxsftp.h"
#include "gxshtml.h"
#include "gxshttp.h"
#include "gxshttpc.h"
#include "gxsmtp.h"
#include "gxsocket.h"
#include "gxsping.h"
#include "gxspop3.h"
#include "gxstream.h"
#include "gxstring.h"
#include "gxstypes.h"
#include "gxsurl.h"
#include "gxsutils.h"
#include "gxs_b64.h"
#include "gxs_ver.h"
#include "gxtelnet.h"
#include "gxthread.h"
#include "gxt_ver.h"
#include "gxuint16.h"
#include "gxuint32.h"
#include "gxuint64.h"
#include "htmldrv.h"
#include "infohog.h"
#include "keytypes.h"
#include "logfile.h"
#include "memblock.h"
#include "membuf.h"
#include "ostrbase.h"
#include "pod.h"
#include "pscript.h"
#include "scomserv.h"
#include "strutil.h"
#include "systime.h"
#include "terminal.h"
#include "thelpers.h"
#include "thrapiw.h"
#include "thrpool.h"
#include "thrtypes.h"
#include "ustring.h"
#include "wserror.h"
#include "wx3incs.h"
#include "wx2incs.h"
#include "wxincs.h"

#endif // __MSVC_DLL__

#endif // __GX_DLL_CODE_INCLUDE_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
