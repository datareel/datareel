#######################
#### Start of File ####
#######################
# --------------------------------------------------------------- 
# Makefile Contents: Makefile for the DataReel library components
# C/C++ Compiler Used: GNU, Intel, Cray
# Produced By: DataReel Software Development Team
# File Creation Date: 08/15/2000
# Date Last Modified: 06/29/2016
# --------------------------------------------------------------- 
# --------------- Makefile Description and Details -------------- 
# --------------------------------------------------------------- 
# This makefile contains all the build dependency rules for the
# DataReel library. 
# --------------------------------------------------------------- 

# Define the $(GCODE_LIB_DIR) macro in the main makefile for this application
# My directory path for the DataReel library
# GCODE_LIB_DIR =

# Define the $(PATHSEP) macro in the main makefile for this application
# Macro for path separator
# PATHSEP =

# Datareel include path
GLIB_INCLUDE_PATH = $(GCODE_LIB_DIR)$(PATHSEP)include$(PATHSEP)

# Build dependency rules
# ===============================================================
ASPRINT_DEP = $(GLIB_INCLUDE_PATH)asprint.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h

BSTREEI_DEP = $(GLIB_INCLUDE_PATH)bstnode.h \
	$(GLIB_INCLUDE_PATH)bstreei.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxlist.h

BTCACHE_DEP = $(GLIB_INCLUDE_PATH)btcache.h \
	$(GLIB_INCLUDE_PATH)btnode.h \
	$(GLIB_INCLUDE_PATH)btstack.h \
	$(GLIB_INCLUDE_PATH)dbasekey.h \
	$(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxbtree.h \
	$(GLIB_INCLUDE_PATH)gxcrc32.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdbase.h \
	$(GLIB_INCLUDE_PATH)gxderror.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)keytypes.h \
	$(GLIB_INCLUDE_PATH)strutil.h

BTNODE_DEP = $(GLIB_INCLUDE_PATH)btnode.h \
	$(GLIB_INCLUDE_PATH)dbasekey.h \
	$(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxcrc32.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdbase.h \
	$(GLIB_INCLUDE_PATH)gxderror.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)keytypes.h \
	$(GLIB_INCLUDE_PATH)strutil.h

BTSTACK_DEP = $(GLIB_INCLUDE_PATH)btstack.h \
	$(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxcrc32.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdbase.h \
	$(GLIB_INCLUDE_PATH)gxderror.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)keytypes.h \
	$(GLIB_INCLUDE_PATH)strutil.h

CDATE_DEP = $(GLIB_INCLUDE_PATH)cdate.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxuint16.h

DBASEKEY_DEP = $(GLIB_INCLUDE_PATH)dbasekey.h \
	$(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxcrc32.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdbase.h \
	$(GLIB_INCLUDE_PATH)gxderror.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)keytypes.h \
	$(GLIB_INCLUDE_PATH)strutil.h

DBFCACHE_DEP = $(GLIB_INCLUDE_PATH)dbfcache.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxderror.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)membuf.h

DBUGMGR_DEP = $(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxcrc32.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdbase.h \
	$(GLIB_INCLUDE_PATH)gxderror.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdstats.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)ustring.h \
	$(GLIB_INCLUDE_PATH)gxstring.h \
	$(GLIB_INCLUDE_PATH)dbugmgr.h

DEVCACHE_DEP = $(GLIB_INCLUDE_PATH)devcache.h \
	$(GLIB_INCLUDE_PATH)devtypes.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)membuf.h

DFILEB_DEP = $(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)ustring.h

EHANDLER_DEP = $(GLIB_INCLUDE_PATH)ehandler.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h

FSTRING_DEP = $(GLIB_INCLUDE_PATH)fstring.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)strutil.h

FUTILS_DEP = $(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h

GPERSIST_DEP = $(GLIB_INCLUDE_PATH)btcache.h \
	$(GLIB_INCLUDE_PATH)btnode.h \
	$(GLIB_INCLUDE_PATH)btstack.h \
	$(GLIB_INCLUDE_PATH)dbasekey.h \
	$(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gpersist.h \
	$(GLIB_INCLUDE_PATH)gxbtree.h \
	$(GLIB_INCLUDE_PATH)gxcrc32.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdbase.h \
	$(GLIB_INCLUDE_PATH)gxderror.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)keytypes.h \
	$(GLIB_INCLUDE_PATH)pod.h \
	$(GLIB_INCLUDE_PATH)strutil.h

GTHREADT_DEP = $(GLIB_INCLUDE_PATH)gthreadt.h \
	$(GLIB_INCLUDE_PATH)gxcond.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxmutex.h \
	$(GLIB_INCLUDE_PATH)gxthread.h \
	$(GLIB_INCLUDE_PATH)thelpers.h \
	$(GLIB_INCLUDE_PATH)thrpool.h \
	$(GLIB_INCLUDE_PATH)thrtypes.h

GXBTREE_DEP = $(GLIB_INCLUDE_PATH)btcache.h \
	$(GLIB_INCLUDE_PATH)btnode.h \
	$(GLIB_INCLUDE_PATH)btstack.h \
	$(GLIB_INCLUDE_PATH)dbasekey.h \
	$(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxbtree.h \
	$(GLIB_INCLUDE_PATH)gxcrc32.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdbase.h \
	$(GLIB_INCLUDE_PATH)gxderror.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)keytypes.h \
	$(GLIB_INCLUDE_PATH)strutil.h

GXCOND_DEP = $(GLIB_INCLUDE_PATH)gxcond.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxmutex.h \
	$(GLIB_INCLUDE_PATH)thelpers.h \
	$(GLIB_INCLUDE_PATH)thrtypes.h

GXCONFIG_DEP = $(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxconfig.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxlistb.h \
	$(GLIB_INCLUDE_PATH)gxstring.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)ustring.h

GXCRC32_DEP = $(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxcrc32.h \
	$(GLIB_INCLUDE_PATH)gxcrctab.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)strutil.h

GXDATAGM_DEP = $(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdatagm.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxsocket.h \
	$(GLIB_INCLUDE_PATH)gxstypes.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h

GXDBASE_DEP = $(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxcrc32.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdbase.h \
	$(GLIB_INCLUDE_PATH)gxderror.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)strutil.h

GXDERROR_DEP = $(GLIB_INCLUDE_PATH)gxderror.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h

GXDFP64_DEP = $(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h

GXDFPTR_DEP = $(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h

GXDLCODE_DEP = $(GLIB_INCLUDE_PATH)asprint.h \
	$(GLIB_INCLUDE_PATH)bstnode.h \
	$(GLIB_INCLUDE_PATH)bstreei.h \
	$(GLIB_INCLUDE_PATH)btcache.h \
	$(GLIB_INCLUDE_PATH)btnode.h \
	$(GLIB_INCLUDE_PATH)btstack.h \
	$(GLIB_INCLUDE_PATH)cdate.h \
	$(GLIB_INCLUDE_PATH)dbasekey.h \
	$(GLIB_INCLUDE_PATH)dbugmgr.h \
	$(GLIB_INCLUDE_PATH)devcache.h \
	$(GLIB_INCLUDE_PATH)devtypes.h \
	$(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)ehandler.h \
	$(GLIB_INCLUDE_PATH)fstring.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gpersist.h \
	$(GLIB_INCLUDE_PATH)gthreadt.h \
	$(GLIB_INCLUDE_PATH)gxbstree.h \
	$(GLIB_INCLUDE_PATH)gxbtree.h \
	$(GLIB_INCLUDE_PATH)gxcond.h \
	$(GLIB_INCLUDE_PATH)gxconfig.h \
	$(GLIB_INCLUDE_PATH)gxcrc32.h \
	$(GLIB_INCLUDE_PATH)gxcrctab.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdatagm.h \
	$(GLIB_INCLUDE_PATH)gxdbase.h \
	$(GLIB_INCLUDE_PATH)gxderror.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdlincs.h \
	$(GLIB_INCLUDE_PATH)gxdstats.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxfloat.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint16.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxip32.h \
	$(GLIB_INCLUDE_PATH)gxlist.h \
	$(GLIB_INCLUDE_PATH)gxlistb.h \
	$(GLIB_INCLUDE_PATH)gxmac48.h \
	$(GLIB_INCLUDE_PATH)gxmutex.h \
	$(GLIB_INCLUDE_PATH)gxrdbdef.h \
	$(GLIB_INCLUDE_PATH)gxrdbhdr.h \
	$(GLIB_INCLUDE_PATH)gxrdbms.h \
	$(GLIB_INCLUDE_PATH)gxrdbsql.h \
	$(GLIB_INCLUDE_PATH)gxs_b64.h \
	$(GLIB_INCLUDE_PATH)gxs_ver.h \
	$(GLIB_INCLUDE_PATH)gxscomm.h \
	$(GLIB_INCLUDE_PATH)gxsema.h \
	$(GLIB_INCLUDE_PATH)gxsftp.h \
	$(GLIB_INCLUDE_PATH)gxshtml.h \
	$(GLIB_INCLUDE_PATH)gxshttp.h \
	$(GLIB_INCLUDE_PATH)gxshttpc.h \
	$(GLIB_INCLUDE_PATH)gxsmtp.h \
	$(GLIB_INCLUDE_PATH)gxsocket.h \
	$(GLIB_INCLUDE_PATH)gxsping.h \
	$(GLIB_INCLUDE_PATH)gxspop3.h \
	$(GLIB_INCLUDE_PATH)gxstream.h \
	$(GLIB_INCLUDE_PATH)gxstring.h \
	$(GLIB_INCLUDE_PATH)gxstypes.h \
	$(GLIB_INCLUDE_PATH)gxsurl.h \
	$(GLIB_INCLUDE_PATH)gxsutils.h \
	$(GLIB_INCLUDE_PATH)gxt_ver.h \
	$(GLIB_INCLUDE_PATH)gxtelnet.h \
	$(GLIB_INCLUDE_PATH)gxthread.h \
	$(GLIB_INCLUDE_PATH)gxuint16.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)htmldrv.h \
	$(GLIB_INCLUDE_PATH)infohog.h \
	$(GLIB_INCLUDE_PATH)keytypes.h \
	$(GLIB_INCLUDE_PATH)leaktest.h \
	$(GLIB_INCLUDE_PATH)logfile.h \
	$(GLIB_INCLUDE_PATH)memblock.h \
	$(GLIB_INCLUDE_PATH)membuf.h \
	$(GLIB_INCLUDE_PATH)ostrbase.h \
	$(GLIB_INCLUDE_PATH)pod.h \
	$(GLIB_INCLUDE_PATH)pscript.h \
	$(GLIB_INCLUDE_PATH)scomserv.h \
	$(GLIB_INCLUDE_PATH)stdafx.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)systime.h \
	$(GLIB_INCLUDE_PATH)terminal.h \
	$(GLIB_INCLUDE_PATH)thelpers.h \
	$(GLIB_INCLUDE_PATH)thrapiw.h \
	$(GLIB_INCLUDE_PATH)thrpool.h \
	$(GLIB_INCLUDE_PATH)thrtypes.h \
	$(GLIB_INCLUDE_PATH)ustring.h \
	$(GLIB_INCLUDE_PATH)wserror.h \
	$(GLIB_INCLUDE_PATH)wx2incs.h \
	$(GLIB_INCLUDE_PATH)wxincs.h

GXDSTATS_DEP = $(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxcrc32.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdbase.h \
	$(GLIB_INCLUDE_PATH)gxderror.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdstats.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)ustring.h

GXFLOAT_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxfloat.h

GXINT16_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxint16.h

GXINT32_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxint32.h

GXINT64_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)strutil.h

GXIP32_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxip32.h

GXLISTB_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxlistb.h

GXMAC48_DEP = $(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxmac48.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)strutil.h

GXMUTEX_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxmutex.h \
	$(GLIB_INCLUDE_PATH)thelpers.h \
	$(GLIB_INCLUDE_PATH)thrtypes.h

GXRDBDEF_DEP =  $(GLIB_INCLUDE_PATH)btcache.h \
	$(GLIB_INCLUDE_PATH)btnode.h \
	$(GLIB_INCLUDE_PATH)btstack.h \
	$(GLIB_INCLUDE_PATH)dbasekey.h \
	$(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxbtree.h \
	$(GLIB_INCLUDE_PATH)gxcrc32.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdbase.h \
	$(GLIB_INCLUDE_PATH)gxderror.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)keytypes.h \
	$(GLIB_INCLUDE_PATH)pod.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)cdate.h \
	$(GLIB_INCLUDE_PATH)gxuint16.h \
	$(GLIB_INCLUDE_PATH)membuf.h \
	$(GLIB_INCLUDE_PATH)gxfloat.h \
	$(GLIB_INCLUDE_PATH)ustring.h \
	$(GLIB_INCLUDE_PATH)gxstring.h \
	$(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)bstnode.h \
	$(GLIB_INCLUDE_PATH)bstreei.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxlist.h \
	$(GLIB_INCLUDE_PATH)gxrdbhdr.h \
	$(GLIB_INCLUDE_PATH)gxrdbms.h \
	$(GLIB_INCLUDE_PATH)systime.h \
	$(GLIB_INCLUDE_PATH)gxrdbdef.h

GXRDBHDR_DEP = $(GLIB_INCLUDE_PATH)btcache.h \
	$(GLIB_INCLUDE_PATH)btnode.h \
	$(GLIB_INCLUDE_PATH)btstack.h \
	$(GLIB_INCLUDE_PATH)dbasekey.h \
	$(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxbtree.h \
	$(GLIB_INCLUDE_PATH)gxcrc32.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdbase.h \
	$(GLIB_INCLUDE_PATH)gxderror.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)keytypes.h \
	$(GLIB_INCLUDE_PATH)pod.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)cdate.h \
	$(GLIB_INCLUDE_PATH)gxuint16.h \
	$(GLIB_INCLUDE_PATH)membuf.h \
	$(GLIB_INCLUDE_PATH)gxfloat.h \
	$(GLIB_INCLUDE_PATH)ustring.h \
	$(GLIB_INCLUDE_PATH)gxstring.h \
	$(GLIB_INCLUDE_PATH)gxrdbhdr.h

GXRDBMS_DEP = $(GLIB_INCLUDE_PATH)btcache.h \
	$(GLIB_INCLUDE_PATH)btnode.h \
	$(GLIB_INCLUDE_PATH)btstack.h \
	$(GLIB_INCLUDE_PATH)dbasekey.h \
	$(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxbtree.h \
	$(GLIB_INCLUDE_PATH)gxcrc32.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdbase.h \
	$(GLIB_INCLUDE_PATH)gxderror.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)keytypes.h \
	$(GLIB_INCLUDE_PATH)pod.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)cdate.h \
	$(GLIB_INCLUDE_PATH)gxuint16.h \
	$(GLIB_INCLUDE_PATH)membuf.h \
	$(GLIB_INCLUDE_PATH)gxfloat.h \
	$(GLIB_INCLUDE_PATH)ustring.h \
	$(GLIB_INCLUDE_PATH)gxstring.h \
	$(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)bstnode.h \
	$(GLIB_INCLUDE_PATH)bstreei.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxlist.h \
	$(GLIB_INCLUDE_PATH)gxrdbhdr.h \
	$(GLIB_INCLUDE_PATH)gxrdbms.h \
	$(GLIB_INCLUDE_PATH)dbugmgr.h

GXRDBSQL_DEP =  $(GLIB_INCLUDE_PATH)btcache.h \
	$(GLIB_INCLUDE_PATH)btnode.h \
	$(GLIB_INCLUDE_PATH)btstack.h \
	$(GLIB_INCLUDE_PATH)dbasekey.h \
	$(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxbtree.h \
	$(GLIB_INCLUDE_PATH)gxcrc32.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdbase.h \
	$(GLIB_INCLUDE_PATH)gxderror.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)keytypes.h \
	$(GLIB_INCLUDE_PATH)pod.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)cdate.h \
	$(GLIB_INCLUDE_PATH)gxuint16.h \
	$(GLIB_INCLUDE_PATH)membuf.h \
	$(GLIB_INCLUDE_PATH)gxfloat.h \
	$(GLIB_INCLUDE_PATH)ustring.h \
	$(GLIB_INCLUDE_PATH)gxstring.h \
	$(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)bstnode.h \
	$(GLIB_INCLUDE_PATH)bstreei.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxlist.h \
	$(GLIB_INCLUDE_PATH)gxrdbhdr.h \
	$(GLIB_INCLUDE_PATH)gxrdbms.h \
	$(GLIB_INCLUDE_PATH)systime.h \
	$(GLIB_INCLUDE_PATH)gxrdbsql.h

GXS_B64_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxs_b64.h \
	$(GLIB_INCLUDE_PATH)gxstring.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)ustring.h

GXSCOMM_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxscomm.h

GXSEMA_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxsema.h \
	$(GLIB_INCLUDE_PATH)thelpers.h \
	$(GLIB_INCLUDE_PATH)thrtypes.h

GXSFTP_DEP = $(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxsftp.h \
	$(GLIB_INCLUDE_PATH)gxsocket.h \
	$(GLIB_INCLUDE_PATH)gxstypes.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)strutil.h

GXSHTML_DEP = $(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxlist.h \
	$(GLIB_INCLUDE_PATH)gxshtml.h \
	$(GLIB_INCLUDE_PATH)gxstring.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)membuf.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)ustring.h

GXSRSS_DEP = $(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxlist.h \
	$(GLIB_INCLUDE_PATH)gxshtml.h \
	$(GLIB_INCLUDE_PATH)gxstring.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)membuf.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)ustring.h \
	$(GLIB_INCLUDE_PATH)gxsrss.h \
	$(GLIB_INCLUDE_PATH)gxsxml.h

GXSXML_DEP = $(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxlist.h \
	$(GLIB_INCLUDE_PATH)gxshtml.h \
	$(GLIB_INCLUDE_PATH)gxstring.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)membuf.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)ustring.h \
	$(GLIB_INCLUDE_PATH)gxsxml.h

GXSHTTP_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxlist.h \
	$(GLIB_INCLUDE_PATH)gxshttp.h \
	$(GLIB_INCLUDE_PATH)gxstring.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)ustring.h

GXSHTTPC_DEP = $(GLIB_INCLUDE_PATH)devcache.h \
	$(GLIB_INCLUDE_PATH)devtypes.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxlist.h \
	$(GLIB_INCLUDE_PATH)gxs_b64.h \
	$(GLIB_INCLUDE_PATH)gxshttp.h \
	$(GLIB_INCLUDE_PATH)gxshttpc.h \
	$(GLIB_INCLUDE_PATH)gxsocket.h \
	$(GLIB_INCLUDE_PATH)gxstring.h \
	$(GLIB_INCLUDE_PATH)gxstypes.h \
	$(GLIB_INCLUDE_PATH)gxsurl.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)membuf.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)ustring.h \
	$(GLIB_INCLUDE_PATH)gxs_ver.h \
	$(GLIB_INCLUDE_PATH)gxssl.h \

GXSMTP_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxsmtp.h \
	$(GLIB_INCLUDE_PATH)gxsocket.h \
	$(GLIB_INCLUDE_PATH)gxstypes.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)systime.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)ustring.h

GXSOCKET_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxsocket.h \
	$(GLIB_INCLUDE_PATH)gxstypes.h

GXSPING_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxsocket.h \
	$(GLIB_INCLUDE_PATH)gxsping.h \
	$(GLIB_INCLUDE_PATH)gxstypes.h \
	$(GLIB_INCLUDE_PATH)gxsutils.h

GXSPOP3_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxsocket.h \
	$(GLIB_INCLUDE_PATH)gxspop3.h \
	$(GLIB_INCLUDE_PATH)gxstypes.h

GXSSL_DEP = $(GLIB_INCLUDE_PATH)gxssl.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxsocket.h \
	$(GLIB_INCLUDE_PATH)gxstring.h \
	$(GLIB_INCLUDE_PATH)gxstypes.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)membuf.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)ustring.h \
	$(GLIB_INCLUDE_PATH)gxs_ver.h

GXSTREAM_DEP = $(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxsocket.h \
	$(GLIB_INCLUDE_PATH)gxstream.h \
	$(GLIB_INCLUDE_PATH)gxstypes.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h

GXSURL_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxstring.h \
	$(GLIB_INCLUDE_PATH)gxsurl.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)ustring.h

GXSUTILS_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxsocket.h \
	$(GLIB_INCLUDE_PATH)gxstypes.h \
	$(GLIB_INCLUDE_PATH)gxsutils.h

GXTELNET_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxsocket.h \
	$(GLIB_INCLUDE_PATH)gxstypes.h \
	$(GLIB_INCLUDE_PATH)gxtelnet.h

GXTHREAD_DEP = $(GLIB_INCLUDE_PATH)gthreadt.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxthread.h \
	$(GLIB_INCLUDE_PATH)thrapiw.h \
	$(GLIB_INCLUDE_PATH)thrpool.h \
	$(GLIB_INCLUDE_PATH)thrtypes.h

GXUINT16_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxuint16.h

GXUINT32_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h

GXUINT64_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)strutil.h

HTMLDRV_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)htmldrv.h \
	$(GLIB_INCLUDE_PATH)systime.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)ustring.h

LEAKTEST_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)leaktest.h

LOGFILE_DEP = $(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)logfile.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)systime.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)ustring.h

MEMBLOCK_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)memblock.h

MEMBUF_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)membuf.h

OSTRBASE_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)ostrbase.h

POD_DEP = $(GLIB_INCLUDE_PATH)btcache.h \
	$(GLIB_INCLUDE_PATH)btnode.h \
	$(GLIB_INCLUDE_PATH)btstack.h \
	$(GLIB_INCLUDE_PATH)dbasekey.h \
	$(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxbtree.h \
	$(GLIB_INCLUDE_PATH)gxcrc32.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdbase.h \
	$(GLIB_INCLUDE_PATH)gxderror.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)keytypes.h \
	$(GLIB_INCLUDE_PATH)pod.h \
	$(GLIB_INCLUDE_PATH)strutil.h

PSCRIPT_DEP = $(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)pscript.h \
	$(GLIB_INCLUDE_PATH)strutil.h

SCOMSERV_DEP = $(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxscomm.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)scomserv.h

STDAFX_DEP = $(GLIB_INCLUDE_PATH)asprint.h \
	$(GLIB_INCLUDE_PATH)bstnode.h \
	$(GLIB_INCLUDE_PATH)bstreei.h \
	$(GLIB_INCLUDE_PATH)btcache.h \
	$(GLIB_INCLUDE_PATH)btnode.h \
	$(GLIB_INCLUDE_PATH)btstack.h \
	$(GLIB_INCLUDE_PATH)cdate.h \
	$(GLIB_INCLUDE_PATH)dbasekey.h \
	$(GLIB_INCLUDE_PATH)dbugmgr.h \
	$(GLIB_INCLUDE_PATH)devcache.h \
	$(GLIB_INCLUDE_PATH)devtypes.h \
	$(GLIB_INCLUDE_PATH)dfileb.h \
	$(GLIB_INCLUDE_PATH)ehandler.h \
	$(GLIB_INCLUDE_PATH)fstring.h \
	$(GLIB_INCLUDE_PATH)futils.h \
	$(GLIB_INCLUDE_PATH)gpersist.h \
	$(GLIB_INCLUDE_PATH)gthreadt.h \
	$(GLIB_INCLUDE_PATH)gxbstree.h \
	$(GLIB_INCLUDE_PATH)gxbtree.h \
	$(GLIB_INCLUDE_PATH)gxcond.h \
	$(GLIB_INCLUDE_PATH)gxconfig.h \
	$(GLIB_INCLUDE_PATH)gxcrc32.h \
	$(GLIB_INCLUDE_PATH)gxcrctab.h \
	$(GLIB_INCLUDE_PATH)gxd_ver.h \
	$(GLIB_INCLUDE_PATH)gxdatagm.h \
	$(GLIB_INCLUDE_PATH)gxdbase.h \
	$(GLIB_INCLUDE_PATH)gxderror.h \
	$(GLIB_INCLUDE_PATH)gxdfp64.h \
	$(GLIB_INCLUDE_PATH)gxdfptr.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdlincs.h \
	$(GLIB_INCLUDE_PATH)gxdstats.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)gxdtypes.h \
	$(GLIB_INCLUDE_PATH)gxfloat.h \
	$(GLIB_INCLUDE_PATH)gxheader.h \
	$(GLIB_INCLUDE_PATH)gxint16.h \
	$(GLIB_INCLUDE_PATH)gxint32.h \
	$(GLIB_INCLUDE_PATH)gxint64.h \
	$(GLIB_INCLUDE_PATH)gxip32.h \
	$(GLIB_INCLUDE_PATH)gxlist.h \
	$(GLIB_INCLUDE_PATH)gxlistb.h \
	$(GLIB_INCLUDE_PATH)gxmac48.h \
	$(GLIB_INCLUDE_PATH)gxmutex.h \
	$(GLIB_INCLUDE_PATH)gxrdbdef.h \
	$(GLIB_INCLUDE_PATH)gxrdbhdr.h \
	$(GLIB_INCLUDE_PATH)gxrdbms.h \
	$(GLIB_INCLUDE_PATH)gxrdbsql.h \
	$(GLIB_INCLUDE_PATH)gxs_b64.h \
	$(GLIB_INCLUDE_PATH)gxs_ver.h \
	$(GLIB_INCLUDE_PATH)gxscomm.h \
	$(GLIB_INCLUDE_PATH)gxsema.h \
	$(GLIB_INCLUDE_PATH)gxsftp.h \
	$(GLIB_INCLUDE_PATH)gxshtml.h \
	$(GLIB_INCLUDE_PATH)gxshttp.h \
	$(GLIB_INCLUDE_PATH)gxshttpc.h \
	$(GLIB_INCLUDE_PATH)gxsmtp.h \
	$(GLIB_INCLUDE_PATH)gxsocket.h \
	$(GLIB_INCLUDE_PATH)gxsping.h \
	$(GLIB_INCLUDE_PATH)gxspop3.h \
	$(GLIB_INCLUDE_PATH)gxstream.h \
	$(GLIB_INCLUDE_PATH)gxstring.h \
	$(GLIB_INCLUDE_PATH)gxstypes.h \
	$(GLIB_INCLUDE_PATH)gxsurl.h \
	$(GLIB_INCLUDE_PATH)gxsutils.h \
	$(GLIB_INCLUDE_PATH)gxt_ver.h \
	$(GLIB_INCLUDE_PATH)gxtelnet.h \
	$(GLIB_INCLUDE_PATH)gxthread.h \
	$(GLIB_INCLUDE_PATH)gxuint16.h \
	$(GLIB_INCLUDE_PATH)gxuint32.h \
	$(GLIB_INCLUDE_PATH)gxuint64.h \
	$(GLIB_INCLUDE_PATH)htmldrv.h \
	$(GLIB_INCLUDE_PATH)infohog.h \
	$(GLIB_INCLUDE_PATH)keytypes.h \
	$(GLIB_INCLUDE_PATH)leaktest.h \
	$(GLIB_INCLUDE_PATH)logfile.h \
	$(GLIB_INCLUDE_PATH)memblock.h \
	$(GLIB_INCLUDE_PATH)membuf.h \
	$(GLIB_INCLUDE_PATH)ostrbase.h \
	$(GLIB_INCLUDE_PATH)pod.h \
	$(GLIB_INCLUDE_PATH)pscript.h \
	$(GLIB_INCLUDE_PATH)scomserv.h \
	$(GLIB_INCLUDE_PATH)stdafx.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)systime.h \
	$(GLIB_INCLUDE_PATH)terminal.h \
	$(GLIB_INCLUDE_PATH)thelpers.h \
	$(GLIB_INCLUDE_PATH)thrapiw.h \
	$(GLIB_INCLUDE_PATH)thrpool.h \
	$(GLIB_INCLUDE_PATH)thrtypes.h \
	$(GLIB_INCLUDE_PATH)ustring.h \
	$(GLIB_INCLUDE_PATH)wserror.h \
	$(GLIB_INCLUDE_PATH)wx2incs.h \
	$(GLIB_INCLUDE_PATH)wxincs.h

STRUTIL_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)strutil.h

SYSTIME_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)systime.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)ustring.h

TERMINAL_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)terminal.h

THELPERS_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)thelpers.h \
	$(GLIB_INCLUDE_PATH)thrtypes.h

THRAPIW_DEP = $(GLIB_INCLUDE_PATH)gthreadt.h \
	$(GLIB_INCLUDE_PATH)gxcond.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxmutex.h \
	$(GLIB_INCLUDE_PATH)gxthread.h \
	$(GLIB_INCLUDE_PATH)thelpers.h \
	$(GLIB_INCLUDE_PATH)thrapiw.h \
	$(GLIB_INCLUDE_PATH)thrpool.h \
	$(GLIB_INCLUDE_PATH)thrtypes.h

THRPOOL_DEP = $(GLIB_INCLUDE_PATH)gthreadt.h \
	$(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)thrpool.h \
	$(GLIB_INCLUDE_PATH)thrtypes.h

USTRING_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxdtyp64.h \
	$(GLIB_INCLUDE_PATH)strutil.h \
	$(GLIB_INCLUDE_PATH)ustring.h

WSERROR_DEP = $(GLIB_INCLUDE_PATH)gxdlcode.h \
	$(GLIB_INCLUDE_PATH)gxstypes.h \
	$(GLIB_INCLUDE_PATH)wserror.h
# ===============================================================

# --------------------------------------------------------------- 
#####################
#### End of File ####
#####################
