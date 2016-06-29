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
# This makefile contains all the build rules needed to compile 
# the DataReel library.
# --------------------------------------------------------------- 

# Define the $(GCODE_LIB_DIR) macro in the main makefile for this application
# My directory path for the DataReel library
# GCODE_LIB_DIR =

# Define the $(PATHSEP) macro in the main makefile for this application
# Macro for path separator
# PATHSEP =

# Define the $(OBJ_EXT) macro in the main makefile for this application
# Object file dot extension
# OBJ_EXT =

# Datareel source code path
GLIB_SRC_PATH = $(GCODE_LIB_DIR)$(PATHSEP)src$(PATHSEP)

# Rules to compile library components 
# ===============================================================
asprint$(OBJ_EXT):	$(GLIB_SRC_PATH)asprint.cpp $(ASPRINT_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)asprint.cpp

bstreei$(OBJ_EXT):	$(GLIB_SRC_PATH)bstreei.cpp $(GXSTREEI_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)bstreei.cpp

btcache$(OBJ_EXT):	$(GLIB_SRC_PATH)btcache.cpp $(BTCACHE_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)btcache.cpp

btnode$(OBJ_EXT):	$(GLIB_SRC_PATH)btnode.cpp $(BTNODE_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)btnode.cpp

btstack$(OBJ_EXT):	$(GLIB_SRC_PATH)btstack.cpp $(BTSTACK_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)btstack.cpp

cdate$(OBJ_EXT):	$(GLIB_SRC_PATH)cdate.cpp $(CDATE_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)cdate.cpp

dbfcache$(OBJ_EXT):	$(GLIB_SRC_PATH)dbfcache.cpp $(DBFCACHE_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)dbfcache.cpp

dbasekey$(OBJ_EXT):	$(GLIB_SRC_PATH)dbasekey.cpp $(DBASEKEY_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)dbasekey.cpp

dbugmgr$(OBJ_EXT):	$(GLIB_SRC_PATH)dbugmgr.cpp $(DBUGMGR_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)dbugmgr.cpp

devcache$(OBJ_EXT):	$(GLIB_SRC_PATH)devcache.cpp $(DEVCACHE_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)devcache.cpp

dfileb$(OBJ_EXT):	$(GLIB_SRC_PATH)dfileb.cpp $(DFILEB_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)dfileb.cpp

ehandler$(OBJ_EXT):	$(GLIB_SRC_PATH)ehandler.cpp $(EHANDLER_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)ehandler.cpp

fstring$(OBJ_EXT):	$(GLIB_SRC_PATH)fstring.cpp $(FSTRING_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)fstring.cpp

futils$(OBJ_EXT):	$(GLIB_SRC_PATH)futils.cpp $(FUTILS_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)futils.cpp

gpersist$(OBJ_EXT):	$(GLIB_SRC_PATH)gpersist.cpp $(GPERSIST_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gpersist.cpp

gthreadt$(OBJ_EXT):	$(GLIB_SRC_PATH)gthreadt.cpp $(GTHREADT_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gthreadt.cpp

gxbtree$(OBJ_EXT):	$(GLIB_SRC_PATH)gxbtree.cpp $(GXTREE_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxbtree.cpp

gxcond$(OBJ_EXT):	$(GLIB_SRC_PATH)gxcond.cpp $(GXCOND_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxcond.cpp

gxconfig$(OBJ_EXT):	$(GLIB_SRC_PATH)gxconfig.cpp $(GXCONFIG_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxconfig.cpp

gxcrc32$(OBJ_EXT):	$(GLIB_SRC_PATH)gxcrc32.cpp $(GXCRC32_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxcrc32.cpp

gxdatagm$(OBJ_EXT):	$(GLIB_SRC_PATH)gxdatagm.cpp $(GXDATAGM_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxdatagm.cpp

gxdbase$(OBJ_EXT):    $(GLIB_SRC_PATH)gxdbase.cpp $(GXDBASE_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxdbase.cpp

gxderror$(OBJ_EXT):	$(GLIB_SRC_PATH)gxderror.cpp $(GXDERROR_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxderror.cpp

gxdfp64$(OBJ_EXT):    $(GLIB_SRC_PATH)gxdfp64.cpp $(GXDFP64_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxdfp64.cpp

gxdfptr$(OBJ_EXT):    $(GLIB_SRC_PATH)gxdfptr.cpp $(GXDFPTR_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxdfptr.cpp

gxdlcode$(OBJ_EXT):	$(GLIB_SRC_PATH)gxdlcode.cpp $(GXDLCODE_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxdlcode.cpp

gxdstats$(OBJ_EXT):	$(GLIB_SRC_PATH)gxdstats.cpp $(GXDSTATS_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxdstats.cpp

gxfloat$(OBJ_EXT):	$(GLIB_SRC_PATH)gxfloat.cpp $(GXFLOAT_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxfloat.cpp

gxint16$(OBJ_EXT):	$(GLIB_SRC_PATH)gxint16.cpp $(GXINT16_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxint16.cpp

gxint32$(OBJ_EXT):	$(GLIB_SRC_PATH)gxint32.cpp $(GXINT32_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxint32.cpp

gxint64$(OBJ_EXT):	$(GLIB_SRC_PATH)gxint64.cpp $(GXINT64_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxint64.cpp

gxip32$(OBJ_EXT):	$(GLIB_SRC_PATH)gxip32.cpp $(GXIP32_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxip32.cpp

gxlistb$(OBJ_EXT):	$(GLIB_SRC_PATH)gxlistb.cpp $(GXLISTB_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxlistb.cpp

gxmac48$(OBJ_EXT):	$(GLIB_SRC_PATH)gxmac48.cpp $(GXMAC48_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxmac48.cpp

gxmutex$(OBJ_EXT):	$(GLIB_SRC_PATH)gxmutex.cpp $(GXMUTEX_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxmutex.cpp

gxrdbdef$(OBJ_EXT):	$(GLIB_SRC_PATH)gxrdbdef.cpp $(GXRDBDEF_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxrdbdef.cpp

gxrdbhdr$(OBJ_EXT):	$(GLIB_SRC_PATH)gxrdbhdr.cpp $(GXRDBHDR_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxrdbhdr.cpp

gxrdbms$(OBJ_EXT):	$(GLIB_SRC_PATH)gxrdbms.cpp $(GXRDBMS_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxrdbms.cpp

gxrdbsql$(OBJ_EXT):	$(GLIB_SRC_PATH)gxrdbsql.cpp $(GXRDBSQL_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxrdbsql.cpp

gxscomm$(OBJ_EXT):	$(GLIB_SRC_PATH)gxscomm.cpp $(GXSCOMM_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxscomm.cpp

gxsema$(OBJ_EXT):	$(GLIB_SRC_PATH)gxsema.cpp $(GXSEMA_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxsema.cpp

gxsftp$(OBJ_EXT):	$(GLIB_SRC_PATH)gxsftp.cpp $(GXSFTP_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxsftp.cpp

gxshtml$(OBJ_EXT):	$(GLIB_SRC_PATH)gxshtml.cpp $(GXSHTML_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxshtml.cpp

gxsrss$(OBJ_EXT):	$(GLIB_SRC_PATH)gxsrss.cpp $(GXSRSS_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxsrss.cpp

gxsxml$(OBJ_EXT):	$(GLIB_SRC_PATH)gxsxml.cpp $(GXSXML_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxsxml.cpp

gxshttp$(OBJ_EXT):	$(GLIB_SRC_PATH)gxshttp.cpp $(GXSHTTP_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxshttp.cpp

gxshttpc$(OBJ_EXT):	$(GLIB_SRC_PATH)gxshttpc.cpp $(GXSHTTPC_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxshttpc.cpp

gxsmtp$(OBJ_EXT):	$(GLIB_SRC_PATH)gxsmtp.cpp $(GXSMTP_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxsmtp.cpp

gxsocket$(OBJ_EXT):	$(GLIB_SRC_PATH)gxsocket.cpp $(GXSOCKET_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxsocket.cpp

gxsping$(OBJ_EXT):	$(GLIB_SRC_PATH)gxsping.cpp $(GXSPING_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxsping.cpp

gxspop3$(OBJ_EXT):	$(GLIB_SRC_PATH)gxspop3.cpp $(GXSPOP3_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxspop3.cpp

gxssl$(OBJ_EXT):	$(GLIB_SRC_PATH)gxssl.cpp $(GXSSL_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxssl.cpp

gxstream$(OBJ_EXT):	$(GLIB_SRC_PATH)gxstream.cpp $(GXSTREAM_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxstream.cpp

gxsurl$(OBJ_EXT):	$(GLIB_SRC_PATH)gxsurl.cpp $(GXSURL_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxsurl.cpp

gxsutils$(OBJ_EXT):	$(GLIB_SRC_PATH)gxsutils.cpp $(GXSUTILS_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxsutils.cpp

gxs_b64$(OBJ_EXT):	$(GLIB_SRC_PATH)gxs_b64.cpp $(GXS_B64_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxs_b64.cpp

gxtelnet$(OBJ_EXT):	$(GLIB_SRC_PATH)gxtelnet.cpp $(GXTELNET_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxtelnet.cpp

gxthread$(OBJ_EXT):	$(GLIB_SRC_PATH)gxthread.cpp $(GXTHREAD_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxthread.cpp

gxuint16$(OBJ_EXT):	$(GLIB_SRC_PATH)gxuint16.cpp $(GXUINT16_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxuint16.cpp

gxuint32$(OBJ_EXT):	$(GLIB_SRC_PATH)gxuint32.cpp $(GXUINT32_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxuint32.cpp

gxuint64$(OBJ_EXT):	$(GLIB_SRC_PATH)gxuint64.cpp $(GXUINT64_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxuint64.cpp

htmldrv$(OBJ_EXT):	$(GLIB_SRC_PATH)htmldrv.cpp $(HTMLDRV_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)htmldrv.cpp

httpgrab$(OBJ_EXT):	$(GLIB_SRC_PATH)httpgrab.cpp $(HTTPGRAB_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)httpgrab.cpp

leaktest$(OBJ_EXT):	$(GLIB_SRC_PATH)leaktest.cpp $(LEAKTEST_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)leaktest.cpp

logfile$(OBJ_EXT):	$(GLIB_SRC_PATH)logfile.cpp $(LOGFILE_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)logfile.cpp

memblock$(OBJ_EXT):	$(GLIB_SRC_PATH)memblock.cpp $(MEMBLOCK_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)memblock.cpp

membuf$(OBJ_EXT):	$(GLIB_SRC_PATH)membuf.cpp $(MEMBUF_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)membuf.cpp

ostrbase$(OBJ_EXT):	$(GLIB_SRC_PATH)ostrbase.cpp $(OSTRBASE_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)ostrbase.cpp

pod$(OBJ_EXT):	$(GLIB_SRC_PATH)pod.cpp $(POD_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)pod.cpp

pscript$(OBJ_EXT):	$(GLIB_SRC_PATH)pscript.cpp $(PSCRIPT_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)pscript.cpp

scomserv$(OBJ_EXT):	$(GLIB_SRC_PATH)scomserv.cpp $(SCOMSERV_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)scomserv.cpp

stdafx$(OBJ_EXT):	$(GLIB_SRC_PATH)stdafx.cpp $(STDAFX_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)stdafx.cpp

strutil$(OBJ_EXT):	$(GLIB_SRC_PATH)strutil.cpp $(STRUTIL_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)strutil.cpp

systime$(OBJ_EXT):	$(GLIB_SRC_PATH)systime.cpp $(SYSTIME_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)systime.cpp

terminal$(OBJ_EXT):	$(GLIB_SRC_PATH)terminal.cpp $(TERMINAL_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)terminal.cpp

thelpers$(OBJ_EXT):	$(GLIB_SRC_PATH)thelpers.cpp $(THELPERS_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)thelpers.cpp

thrapiw$(OBJ_EXT):	$(GLIB_SRC_PATH)thrapiw.cpp $(THRAPIW_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)thrapiw.cpp

thrpool$(OBJ_EXT):	$(GLIB_SRC_PATH)thrpool.cpp $(THRPOOL_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)thrpool.cpp

ustring$(OBJ_EXT):	$(GLIB_SRC_PATH)ustring.cpp $(USTRING_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)ustring.cpp

wserror$(OBJ_EXT):	$(GLIB_SRC_PATH)wserror.cpp $(WSERROR_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)wserror.cpp
# --------------------------------------------------------------- 
#####################
#### End of File ####
#####################
