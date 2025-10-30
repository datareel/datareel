#######################
#### Start of File ####
#######################
# --------------------------------------------------------------- 
# Makefile Contents: Project makefile for the MSVC datareel dynamic link library
# C/C++ Compiler Used: MSVC 6.0 SP5, Visual C++ .NET, and VS 2022
# Produced By: DataReel Software Development Team
# File Creation Date: 05/25/2001
# Date Last Modified: 10/30/2025
# --------------------------------------------------------------- 
# Setup library configuration
!IF "$(FINAL)" == ""
FINAL = 0 # Default to debug configuration
!MESSAGE No library configuration specified. Defaulting to debug configuration.
!ENDIF

!IF "$(64BITCFG)" == ""
64BITCFG = 0 # Default to 32-bit configuration
!MESSAGE No 64-bit configuration specified. Defaulting to 32-bit configuration.
!ENDIF

# Setup gxcode DLL library paths
GCODE_LIB_PATH = .. 
GCODE_LIB_DIR = $(GCODE_LIB_PATH)
GLIB_INCLUDE_PATH = ..$(PATHSEP)include$(PATHSEP)
GLIB_SRC_PATH = ..$(PATHSEP)src$(PATHSEP)

!IF "$(USESSL)" == "1"
!MESSAGE Using OpenSSL ENV to set path to OpenSSL LIB
!include openssl.env
!ENDIF

!include msvc.env

# Define a name for the static library
!IF "$(64BITCFG)" == "1" && "$(FINAL)" == "1"
PROJECT = gxcode64
!ENDIF
!IF "$(64BITCFG)" == "1" && "$(FINAL)" == "0"
PROJECT = gxcode64d
!ENDIF
!IF "$(64BITCFG)" == "0" && "$(FINAL)" == "1"
PROJECT = gxcode32
!ENDIF
!IF "$(64BITCFG)" == "0" && "$(FINAL)" == "0"
PROJECT = gxcode32d
!ENDIF

# Add compiler and linker flags needed to make the DLL
!IF "$(FINAL)" == "1"  # No debugging information
OUTDIR = release
COMPILE_FLAGS = $(COMPILE_FLAGS) /D "_MBCS" /D "_USRDLL" \
/D "GXDLCODE_EXPORTS" \
/Fp"./$(OUTDIR)\$(PROJECT).pch" /Fo"./$(OUTDIR)\\" \
/Fd"./$(OUTDIR)\\" # /FD # MSVC 6.0 SP3 flag
LINKER_FLAGS = $(LINKER_FLAGS) /dll \
/pdb:"./$(OUTDIR)\$(PROJECT).pdb" \
/out:"./$(OUTDIR)\$(PROJECT)$(SHAREDLIB_EXT)" \
/implib:"./$(OUTDIR)\$(PROJECT).lib" 
!ELSE # Making debug DLL
OUTDIR = debug
COMPILE_FLAGS = $(COMPILE_FLAGS) /D "_MBCS" /D "_USRDLL" \
/D "GXDLCODE_EXPORTS" \
/Fp"./$(OUTDIR)\\$(PROJECT).pch" /Fo"./$(OUTDIR)\\" \
/Fd"./$(OUTDIR)\\" # /FD /GZ # MSVC 6.0 SP3 flags
LINKER_FLAGS = $(LINKER_FLAGS) /dll \
/pdb:"./$(OUTDIR)\$(PROJECT).pdb" \
/out:"./$(OUTDIR)\$(PROJECT)$(SHAREDLIB_EXT)" \
/implib:"./$(OUTDIR)\$(PROJECT)$(LIB_EXT)" \
/pdbtype:sept
!ENDIF

# Build dependency rules
# ===============================================================
!include $(GLIB_SRC_PATH)..$(PATHSEP)env$(PATHSEP)glibdeps.mak
# ===============================================================

# Compile the object files and build the DLL, .exp, .pch, .lib files
# ===============================================================
all:	"./$(OUTDIR)\$(PROJECT)$(SHAREDLIB_EXT)"

# DLL entry point targets
$(OUTDIR)\stdafx$(OBJ_EXT):	$(GLIB_SRC_PATH)stdafx.cpp $(STDAFX_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)stdafx.cpp

$(OUTDIR)\gxdlcode$(OBJ_EXT):	$(GLIB_SRC_PATH)gxdlcode.cpp $(GXDLCODE_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxdlcode.cpp

# GX code targets
$(OUTDIR)\asprint$(OBJ_EXT):	$(GLIB_SRC_PATH)asprint.cpp $(ASPRINT_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)asprint.cpp

$(OUTDIR)\bstreei$(OBJ_EXT):	$(GLIB_SRC_PATH)bstreei.cpp $(GXSTREEI_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)bstreei.cpp

$(OUTDIR)\btnode$(OBJ_EXT):	$(GLIB_SRC_PATH)btnode.cpp $(BTNODE_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)btnode.cpp

$(OUTDIR)\btcache$(OBJ_EXT):	$(GLIB_SRC_PATH)btcache.cpp $(BTCACHE_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)btcache.cpp

$(OUTDIR)\btstack$(OBJ_EXT):	$(GLIB_SRC_PATH)btstack.cpp $(BTSTACK_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)btstack.cpp

$(OUTDIR)\cdate$(OBJ_EXT):	$(GLIB_SRC_PATH)cdate.cpp $(CDATE_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)cdate.cpp

$(OUTDIR)\dbasekey$(OBJ_EXT):	$(GLIB_SRC_PATH)dbasekey.cpp $(DBASEKEY_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)dbasekey.cpp

$(OUTDIR)\dbfcache$(OBJ_EXT):	$(GLIB_SRC_PATH)dbfcache.cpp $(DBFCACHE_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)dbfcache.cpp

$(OUTDIR)\dbugmgr$(OBJ_EXT):	$(GLIB_SRC_PATH)dbugmgr.cpp $(DBUGMGR_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)dbugmgr.cpp

$(OUTDIR)\devcache$(OBJ_EXT):	$(GLIB_SRC_PATH)devcache.cpp $(DEVCACHE_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)devcache.cpp

$(OUTDIR)\dfileb$(OBJ_EXT):	$(GLIB_SRC_PATH)dfileb.cpp $(DFILEB_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)dfileb.cpp

$(OUTDIR)\ehandler$(OBJ_EXT):	$(GLIB_SRC_PATH)ehandler.cpp $(EHANDLER_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)ehandler.cpp

$(OUTDIR)\fstring$(OBJ_EXT):	$(GLIB_SRC_PATH)fstring.cpp $(FSTRING_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)fstring.cpp

$(OUTDIR)\futils$(OBJ_EXT):	$(GLIB_SRC_PATH)futils.cpp $(FUTILS_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)futils.cpp

$(OUTDIR)\gpersist$(OBJ_EXT):	$(GLIB_SRC_PATH)gpersist.cpp $(GPERSIST_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gpersist.cpp

$(OUTDIR)\gthreadt$(OBJ_EXT):	$(GLIB_SRC_PATH)gthreadt.cpp $(GTHREADT_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gthreadt.cpp

$(OUTDIR)\gxbtree$(OBJ_EXT):	$(GLIB_SRC_PATH)gxbtree.cpp $(GXTREE_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxbtree.cpp

$(OUTDIR)\gxcond$(OBJ_EXT):	$(GLIB_SRC_PATH)gxcond.cpp $(GXCOND_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxcond.cpp

$(OUTDIR)\gxconfig$(OBJ_EXT):	$(GLIB_SRC_PATH)gxconfig.cpp $(GXCONFIG_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxconfig.cpp

$(OUTDIR)\gxcrc32$(OBJ_EXT):	$(GLIB_SRC_PATH)gxcrc32.cpp $(GXCRC32_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxcrc32.cpp

$(OUTDIR)\gxdatagm$(OBJ_EXT):	$(GLIB_SRC_PATH)gxdatagm.cpp $(GXDATAGM_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxdatagm.cpp

$(OUTDIR)\gxdbase$(OBJ_EXT):    $(GLIB_SRC_PATH)gxdbase.cpp $(GXDBASE_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxdbase.cpp

$(OUTDIR)\gxderror$(OBJ_EXT):	$(GLIB_SRC_PATH)gxderror.cpp $(GXDERROR_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxderror.cpp

$(OUTDIR)\gxdfp64$(OBJ_EXT):    $(GLIB_SRC_PATH)gxdfp64.cpp $(GXDFP64_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxdfp64.cpp

$(OUTDIR)\gxdfptr$(OBJ_EXT):    $(GLIB_SRC_PATH)gxdfptr.cpp $(GXDFPTR_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxdfptr.cpp

$(OUTDIR)\gxdstats$(OBJ_EXT):	$(GLIB_SRC_PATH)gxdstats.cpp $(GXDSTATS_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxdstats.cpp

$(OUTDIR)\gxfloat$(OBJ_EXT):	$(GLIB_SRC_PATH)gxfloat.cpp $(GXFLOAT_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxfloat.cpp

$(OUTDIR)\gxint16$(OBJ_EXT):	$(GLIB_SRC_PATH)gxint16.cpp $(GXINT16_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxint16.cpp

$(OUTDIR)\gxint32$(OBJ_EXT):	$(GLIB_SRC_PATH)gxint32.cpp $(GXINT32_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxint32.cpp

$(OUTDIR)\gxint64$(OBJ_EXT):	$(GLIB_SRC_PATH)gxint64.cpp $(GXINT64_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxint64.cpp

$(OUTDIR)\gxip32$(OBJ_EXT):	$(GLIB_SRC_PATH)gxip32.cpp $(GXIP32_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxip32.cpp

$(OUTDIR)\gxlistb$(OBJ_EXT):	$(GLIB_SRC_PATH)gxlistb.cpp $(GXLISTB_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxlistb.cpp

$(OUTDIR)\gxmac48$(OBJ_EXT):	$(GLIB_SRC_PATH)gxmac48.cpp $(GXMAC48_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxmac48.cpp

$(OUTDIR)\gxmutex$(OBJ_EXT):	$(GLIB_SRC_PATH)gxmutex.cpp $(GXMUTEX_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxmutex.cpp

$(OUTDIR)\gxrdbdef$(OBJ_EXT):	$(GLIB_SRC_PATH)gxrdbdef.cpp $(GXRDBDEF_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxrdbdef.cpp

$(OUTDIR)\gxrdbhdr$(OBJ_EXT):	$(GLIB_SRC_PATH)gxrdbhdr.cpp $(GXRDBHDR_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxrdbhdr.cpp

$(OUTDIR)\gxrdbms$(OBJ_EXT):	$(GLIB_SRC_PATH)gxrdbms.cpp $(GXRDBMS_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxrdbms.cpp

$(OUTDIR)\gxrdbsql$(OBJ_EXT):	$(GLIB_SRC_PATH)gxrdbsql.cpp $(GXRDBSQL_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxrdbsql.cpp

$(OUTDIR)\gxscomm$(OBJ_EXT):	$(GLIB_SRC_PATH)gxscomm.cpp $(GXSCOMM_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxscomm.cpp

$(OUTDIR)\gxsema$(OBJ_EXT):	$(GLIB_SRC_PATH)gxsema.cpp $(GXSEMA_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxsema.cpp

$(OUTDIR)\gxsftp$(OBJ_EXT):	$(GLIB_SRC_PATH)gxsftp.cpp $(GXSFTP_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxsftp.cpp

$(OUTDIR)\gxshtml$(OBJ_EXT):	$(GLIB_SRC_PATH)gxshtml.cpp $(GXSHTML_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxshtml.cpp

$(OUTDIR)\gxsrss$(OBJ_EXT):	$(GLIB_SRC_PATH)gxsrss.cpp $(GXSRSS_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxsrss.cpp

$(OUTDIR)\gxsxml$(OBJ_EXT):	$(GLIB_SRC_PATH)gxsxml.cpp $(GXSXML_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxsxml.cpp

$(OUTDIR)\gxshttp$(OBJ_EXT): $(GLIB_SRC_PATH)gxshttp.cpp
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxshttp.cpp

$(OUTDIR)\gxshttpc$(OBJ_EXT):	$(GLIB_SRC_PATH)gxshttpc.cpp $(GXSHTTPC_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxshttpc.cpp

$(OUTDIR)\gxsmtp$(OBJ_EXT):	$(GLIB_SRC_PATH)gxsmtp.cpp $(GXSMTP_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxsmtp.cpp

$(OUTDIR)\gxsocket$(OBJ_EXT):	$(GLIB_SRC_PATH)gxsocket.cpp $(GXSOCKET_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxsocket.cpp

$(OUTDIR)\gxsping$(OBJ_EXT):	$(GLIB_SRC_PATH)gxsping.cpp $(GXSPING_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxsping.cpp

$(OUTDIR)\gxspop3$(OBJ_EXT):	$(GLIB_SRC_PATH)gxspop3.cpp $(GXSPOP3_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxspop3.cpp

$(OUTDIR)\gxstream$(OBJ_EXT):	$(GLIB_SRC_PATH)gxstream.cpp $(GXSTREAM_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxstream.cpp

$(OUTDIR)\gxsurl$(OBJ_EXT):	$(GLIB_SRC_PATH)gxsurl.cpp $(GXSURL_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxsurl.cpp

$(OUTDIR)\gxsutils$(OBJ_EXT):	$(GLIB_SRC_PATH)gxsutils.cpp $(GXSUTILS_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxsutils.cpp

$(OUTDIR)\gxs_b64$(OBJ_EXT):	$(GLIB_SRC_PATH)gxs_b64.cpp $(GXS_B64_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxs_b64.cpp

$(OUTDIR)\gxtelnet$(OBJ_EXT):	$(GLIB_SRC_PATH)gxtelnet.cpp $(GXTELNET_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxtelnet.cpp

$(OUTDIR)\gxthread$(OBJ_EXT):	$(GLIB_SRC_PATH)gxthread.cpp $(GXTHREAD_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxthread.cpp

$(OUTDIR)\gxuint16$(OBJ_EXT):	$(GLIB_SRC_PATH)gxuint16.cpp $(GXUINT16_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxuint16.cpp

$(OUTDIR)\gxuint32$(OBJ_EXT):	$(GLIB_SRC_PATH)gxuint32.cpp $(GXUINT32_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxuint32.cpp

$(OUTDIR)\gxuint64$(OBJ_EXT):	$(GLIB_SRC_PATH)gxuint64.cpp $(GXUINT64_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxuint64.cpp

$(OUTDIR)\htmldrv$(OBJ_EXT):	$(GLIB_SRC_PATH)htmldrv.cpp $(HTMLDRV_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)htmldrv.cpp

$(OUTDIR)\httpgrab$(OBJ_EXT):	$(GLIB_SRC_PATH)httpgrab.cpp $(HTTPGRAB_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)httpgrab.cpp

$(OUTDIR)\leaktest$(OBJ_EXT):	$(GLIB_SRC_PATH)leaktest.cpp $(LEAKTEST_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)leaktest.cpp

$(OUTDIR)\logfile$(OBJ_EXT):	$(GLIB_SRC_PATH)logfile.cpp $(LOGFILE_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)logfile.cpp

$(OUTDIR)\memblock$(OBJ_EXT):	$(GLIB_SRC_PATH)memblock.cpp $(MEMBLOCK_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)memblock.cpp

$(OUTDIR)\membuf$(OBJ_EXT):	$(GLIB_SRC_PATH)membuf.cpp $(MEMBUF_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)membuf.cpp

$(OUTDIR)\ostrbase$(OBJ_EXT):	$(GLIB_SRC_PATH)ostrbase.cpp $(OSTRBASE_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)ostrbase.cpp

$(OUTDIR)\pod$(OBJ_EXT):	$(GLIB_SRC_PATH)pod.cpp $(POD_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)pod.cpp

$(OUTDIR)\pscript$(OBJ_EXT):	$(GLIB_SRC_PATH)pscript.cpp $(PSCRIPT_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)pscript.cpp

$(OUTDIR)\scomserv$(OBJ_EXT):	$(GLIB_SRC_PATH)scomserv.cpp $(SCOMSERV_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)scomserv.cpp

$(OUTDIR)\strutil$(OBJ_EXT):	$(GLIB_SRC_PATH)strutil.cpp $(STRUTIL_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)strutil.cpp

$(OUTDIR)\systime$(OBJ_EXT):	$(GLIB_SRC_PATH)systime.cpp $(SYSTIME_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)systime.cpp

$(OUTDIR)\terminal$(OBJ_EXT):	$(GLIB_SRC_PATH)terminal.cpp $(TERMINAL_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)terminal.cpp

$(OUTDIR)\thelpers$(OBJ_EXT):	$(GLIB_SRC_PATH)thelpers.cpp $(THELPERS_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)thelpers.cpp

$(OUTDIR)\thrapiw$(OBJ_EXT):	$(GLIB_SRC_PATH)thrapiw.cpp $(THRAPIW_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)thrapiw.cpp

$(OUTDIR)\thrpool$(OBJ_EXT):	$(GLIB_SRC_PATH)thrpool.cpp $(THRPOOL_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)thrpool.cpp

$(OUTDIR)\ustring$(OBJ_EXT):	$(GLIB_SRC_PATH)ustring.cpp $(USTRING_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)ustring.cpp

$(OUTDIR)\wserror$(OBJ_EXT):	$(GLIB_SRC_PATH)wserror.cpp $(WSERROR_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)wserror.cpp

$(OUTDIR)\gxssl$(OBJ_EXT):	$(GLIB_SRC_PATH)gxssl.cpp $(GXSSL_DEP)
	$(CPP) $(COMPILE_ONLY) $(COMPILE_FLAGS) \
	$(GLIB_SRC_PATH)gxssl.cpp

# DLL entry point objects
GLIB_DLL_OBJECTS = $(OUTDIR)\stdafx$(OBJ_EXT) \
	$(OUTDIR)\gxdlcode$(OBJ_EXT)

# Core database library components
GLIB_DATABASE_CORE_OBJECTS =  $(OUTDIR)\gxint32$(OBJ_EXT) \
	$(OUTDIR)\gxint64$(OBJ_EXT) \
	$(OUTDIR)\gxuint32$(OBJ_EXT) \
	$(OUTDIR)\gxuint64$(OBJ_EXT) \
	$(OUTDIR)\gxcrc32$(OBJ_EXT) \
	$(OUTDIR)\gxdbase$(OBJ_EXT) \
	$(OUTDIR)\gxderror$(OBJ_EXT) \
	$(OUTDIR)\gxdfp64$(OBJ_EXT) \
	$(OUTDIR)\gxdfptr$(OBJ_EXT)

# Extra database library components
GLIB_DATABASE_EX_OBJECTS = $(OUTDIR)\btstack$(OBJ_EXT) \
	$(OUTDIR)\btcache$(OBJ_EXT) \
	$(OUTDIR)\btnode$(OBJ_EXT) \
	$(OUTDIR)\dbasekey$(OBJ_EXT) \
	$(OUTDIR)\gpersist$(OBJ_EXT) \
	$(OUTDIR)\gxbtree$(OBJ_EXT) \
	$(OUTDIR)\pod$(OBJ_EXT) \
	$(OUTDIR)\gxint16$(OBJ_EXT) \
	$(OUTDIR)\gxuint16$(OBJ_EXT) \
	$(OUTDIR)\gxfloat$(OBJ_EXT) \
	$(OUTDIR)\dbfcache$(OBJ_EXT) \
	$(OUTDIR)\dbugmgr$(OBJ_EXT)

# RDBMS library components
GLIB_RDBMS_OBJECTS =  $(OUTDIR)\gxrdbdef$(OBJ_EXT) \
	$(OUTDIR)\gxrdbhdr$(OBJ_EXT) \
	$(OUTDIR)\gxrdbms$(OBJ_EXT) \
	$(OUTDIR)\gxrdbsql$(OBJ_EXT)

# Core socket library components
GLIB_SOCKET_CORE_OBJECTS = $(OUTDIR)\gxsocket$(OBJ_EXT)

# Extra socket library components
GLIB_SOCKET_EX_OBJECTS = $(OUTDIR)\gxshttp$(OBJ_EXT) \
	$(OUTDIR)\gxshttpc$(OBJ_EXT) \
	$(OUTDIR)\gxsmtp$(OBJ_EXT) \
	$(OUTDIR)\gxsping$(OBJ_EXT) \
	$(OUTDIR)\gxspop3$(OBJ_EXT) \
	$(OUTDIR)\gxsftp$(OBJ_EXT) \
	$(OUTDIR)\gxshtml$(OBJ_EXT) \
	$(OUTDIR)\gxsurl$(OBJ_EXT) \
	$(OUTDIR)\gxsutils$(OBJ_EXT) \
	$(OUTDIR)\gxs_b64$(OBJ_EXT) \
	$(OUTDIR)\gxtelnet$(OBJ_EXT) \
	$(OUTDIR)\wserror$(OBJ_EXT) \
	$(OUTDIR)\gxsrss$(OBJ_EXT) \
	$(OUTDIR)\gxsxml$(OBJ_EXT) \
	$(OUTDIR)\gxssl$(OBJ_EXT)

# Database socket library components
GLIB_SOCKET_DB_OBJECTS = $(OUTDIR)\gxdatagm$(OBJ_EXT) \
	$(OUTDIR)\gxstream$(OBJ_EXT)

# Core serial comm library components
GLIB_SERIAL_CORE_OBJECTS = $(OUTDIR)\gxscomm$(OBJ_EXT)

# Database serial com library components
GLIB_SERIAL_DB_OBJECTS = $(OUTDIR)\scomserv$(OBJ_EXT)

# Thread library components
GLIB_THREAD_OBJECTS = $(OUTDIR)\gthreadt$(OBJ_EXT) \
	$(OUTDIR)\gxmutex$(OBJ_EXT) \
	$(OUTDIR)\thelpers$(OBJ_EXT) \
	$(OUTDIR)\thrapiw$(OBJ_EXT) \
	$(OUTDIR)\gxthread$(OBJ_EXT) \
	$(OUTDIR)\gxsema$(OBJ_EXT) \
	$(OUTDIR)\gxcond$(OBJ_EXT) \
	$(OUTDIR)\thrpool$(OBJ_EXT)

# General purpose library components
GLIB_GP_OBJECTS = $(OUTDIR)\asprint$(OBJ_EXT) \
	$(OUTDIR)\bstreei$(OBJ_EXT)  \
	$(OUTDIR)\cdate$(OBJ_EXT)  \
	$(OUTDIR)\devcache$(OBJ_EXT) \
	$(OUTDIR)\dfileb$(OBJ_EXT) \
	$(OUTDIR)\fstring$(OBJ_EXT) \
	$(OUTDIR)\futils$(OBJ_EXT) \
	$(OUTDIR)\gxconfig$(OBJ_EXT) \
	$(OUTDIR)\gxip32$(OBJ_EXT) \
	$(OUTDIR)\gxlistb$(OBJ_EXT) \
	$(OUTDIR)\gxmac48$(OBJ_EXT) \
	$(OUTDIR)\htmldrv$(OBJ_EXT) \
	$(OUTDIR)\logfile$(OBJ_EXT) \
	$(OUTDIR)\memblock$(OBJ_EXT) \
	$(OUTDIR)\membuf$(OBJ_EXT) \
	$(OUTDIR)\ostrbase$(OBJ_EXT) \
	$(OUTDIR)\pscript$(OBJ_EXT)  \
	$(OUTDIR)\strutil$(OBJ_EXT) \
	$(OUTDIR)\systime$(OBJ_EXT) \
	$(OUTDIR)\ustring$(OBJ_EXT) 

# Optional debug objects
# NOTE: The leak test functions requires the /D__MSVC_DEBUG__ compiler
# flag and the /MDd or /MTd compiler flag. 
GLIB_DEBUG_OBJECTS = $(OUTDIR)\leaktest$(OBJ_EXT)

# Term I/O objects
GLIB_TERM_OBJECTS = $(OUTDIR)\terminal$(OBJ_EXT)

# Console/GUI messaging gxcode objects
GLIB_MSG_OBJECTS = $(OUTDIR)\ehandler$(OBJ_EXT) \
	$(OUTDIR)\gxdstats$(OBJ_EXT)

# TODO: Set the library components to compile here
GLIB_OBJECTS = $(GLIB_DATABASE_CORE_OBJECTS) \
	$(GLIB_DATABASE_EX_OBJECTS) \
	$(GLIB_RDBMS_OBJECTS) \
	$(GLIB_SOCKET_CORE_OBJECTS) \
	$(GLIB_SOCKET_EX_OBJECTS) \
	$(GLIB_SOCKET_DB_OBJECTS) \
	$(GLIB_SERIAL_CORE_OBJECTS) \
	$(GLIB_SERIAL_DB_OBJECTS) \
	$(GLIB_THREAD_OBJECTS) \
	$(GLIB_GP_OBJECTS) \
	$(GLIB_TERM_OBJECTS) \
	$(GLIB_MSG_OBJECTS) \
	$(GLIB_DEBUG_OBJECTS)

# Add all additional object files here
OBJECTS = $(GLIB_DLL_OBJECTS) $(GLIB_OBJECTS)

"./$(OUTDIR)\$(PROJECT)$(SHAREDLIB_EXT)" :  $(OBJECTS)
	$(LINKER) $(LINKER_FLAGS) $(OBJECTS) $(LINK_LIBRARIES)
# ===============================================================

# install the library files
# ===============================================================
install:
	@echo Removing any old library files
	if exist $(LIB_INSTALL_DIR)\$(PROJECT)$(SHAREDLIB_EXT) \
	del $(LIB_INSTALL_DIR)\$(PROJECT)$(SHAREDLIB_EXT)

	@echo Copying DLL file
	copy $(OUTDIR)\$(PROJECT)$(SHAREDLIB_EXT) \
	$(LIB_INSTALL_DIR)\$(PROJECT)$(SHAREDLIB_EXT)

# Remove object files
# ===============================================================
clean:
	@echo Removing library files...
	if exist $(OUTDIR)\*.lib del $(OUTDIR)\*.lib  
	if exist $(OUTDIR)\*.dll del $(OUTDIR)\*.dll  

	@echo Removing all .EXP files from working directory...
	if exist $(OUTDIR)\*.exp del $(OUTDIR)\*.exp  

	@echo Removing all .PDB files from working directory...
	if exist $(OUTDIR)\*.pdb del $(OUTDIR)\*.pdb

	@echo Removing all .PCH files from working directory...
	if exist $(OUTDIR)\*.pch del $(OUTDIR)\*.pch  

	@echo Removing all .SBR files from working directory...
	if exist $(OUTDIR)\*.sbr del $(OUTDIR)\*.sbr  

	@echo Removing all .VCW files from working directory...
	if exist $(OUTDIR)\*.vcw del $(OUTDIR)\*.vcw 

	@echo Removing all .WSP files from working directory...
	if exist $(OUTDIR)\*.wsp del $(OUTDIR)\*.wsp 

	@echo Removing all .BSC files from working directory...
	if exist $(OUTDIR)\*.bsc del $(OUTDIR)\*.bsc 

	@echo Removing all .SBT files from working directory...
	if exist $(OUTDIR)\*.sbt del $(OUTDIR)\*.sbt 

	@echo Removing all .ILK files from working directory...
	if exist $(OUTDIR)\*.ilk del $(OUTDIR)\*.ilk 

	@echo Removing all .IDB files from working directory...
	if exist $(OUTDIR)\*.idb del $(OUTDIR)\*.idb 

	@echo Removing all .MDP files from working directory...
	if exist $(OUTDIR)\*.mdp del $(OUTDIR)\*.mdp 

	@echo Removing all .NCB files from working directory...
	if exist $(OUTDIR)\*.ncb del $(OUTDIR)\*.ncb 

	@echo Removing all .OBJ files from working directory...
	if exist $(OUTDIR)\*.obj del $(OUTDIR)\*.obj 
# --------------------------------------------------------------- 
#####################
#### End of File ####
#####################


