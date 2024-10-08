#######################
#### Start of File ####
#######################
# --------------------------------------------------------------- 
# Makefile Contents: Makefile for the MSVC gxcode library
# C/C++ Compiler Used: MSVC 6.0 SP5, Visual C++ .NET, and VS 2022
# Produced By: DataReel Software Development Team
# File Creation Date: 05/25/2001
# Date Last Modified: 10/08/2024
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

# Setup my path to the gxcode library
GCODE_LIB_DIR = ..

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

# Build dependency rules
# ===============================================================
!include ../env/glibdeps.mak
# ===============================================================

# Compile the object files and build the library
# ===============================================================
all:	$(PROJECT)$(LIB_EXT)

!include ../env/glibobjs.mak

# Core database library components
GLIB_DATABASE_CORE_OBJECTS =  gxint32$(OBJ_EXT) \
	gxint64$(OBJ_EXT) \
	gxuint32$(OBJ_EXT) \
	gxuint64$(OBJ_EXT) \
	gxcrc32$(OBJ_EXT) \
	gxdbase$(OBJ_EXT) \
	gxderror$(OBJ_EXT) \
	gxdfp64$(OBJ_EXT) \
	gxdfptr$(OBJ_EXT)

# Extra database library components
GLIB_DATABASE_EX_OBJECTS = btstack$(OBJ_EXT) \
	btcache$(OBJ_EXT) \
	btnode$(OBJ_EXT) \
	dbasekey$(OBJ_EXT) \
	gpersist$(OBJ_EXT) \
	gxbtree$(OBJ_EXT) \
	pod$(OBJ_EXT) \
	gxint16$(OBJ_EXT) \
	gxuint16$(OBJ_EXT) \
	gxfloat$(OBJ_EXT) \
	dbfcache$(OBJ_EXT) \
	dbugmgr$(OBJ_EXT)

# RDBMS library components
GLIB_RDBMS_OBJECTS =  gxrdbdef$(OBJ_EXT) \
	gxrdbhdr$(OBJ_EXT) \
	gxrdbms$(OBJ_EXT) \
	gxrdbsql$(OBJ_EXT)

# Core socket library components
GLIB_SOCKET_CORE_OBJECTS = gxsocket$(OBJ_EXT)

# Extra socket library components
GLIB_SOCKET_EX_OBJECTS = gxshttp$(OBJ_EXT) \
	gxshttpc$(OBJ_EXT) \
	gxsmtp$(OBJ_EXT) \
	gxsping$(OBJ_EXT) \
	gxspop3$(OBJ_EXT) \
	gxsftp$(OBJ_EXT) \
	gxshtml$(OBJ_EXT) \
	gxsurl$(OBJ_EXT) \
	gxsutils$(OBJ_EXT) \
	gxs_b64$(OBJ_EXT) \
	gxtelnet$(OBJ_EXT) \
	wserror$(OBJ_EXT) \
	gxsrss$(OBJ_EXT) \
	gxsxml$(OBJ_EXT) \
	gxssl$(OBJ_EXT)

# Database socket library components
GLIB_SOCKET_DB_OBJECTS = gxdatagm$(OBJ_EXT) \
	gxstream$(OBJ_EXT)

# Core serial comm library components
GLIB_SERIAL_CORE_OBJECTS = gxscomm$(OBJ_EXT)

# Database serial com library components
GLIB_SERIAL_DB_OBJECTS = scomserv$(OBJ_EXT)

# Thread library components
GLIB_THREAD_OBJECTS = gthreadt$(OBJ_EXT) \
	gxmutex$(OBJ_EXT) \
	thelpers$(OBJ_EXT) \
	thrapiw$(OBJ_EXT) \
	gxthread$(OBJ_EXT) \
	gxsema$(OBJ_EXT) \
	gxcond$(OBJ_EXT) \
	thrpool$(OBJ_EXT)

# General purpose library components
GLIB_GP_OBJECTS = asprint$(OBJ_EXT) \
	bstreei$(OBJ_EXT)  \
	cdate$(OBJ_EXT)  \
	devcache$(OBJ_EXT) \
	dfileb$(OBJ_EXT) \
	fstring$(OBJ_EXT) \
	futils$(OBJ_EXT) \
	gxconfig$(OBJ_EXT) \
	gxip32$(OBJ_EXT) \
	gxlistb$(OBJ_EXT) \
	gxmac48$(OBJ_EXT) \
	htmldrv$(OBJ_EXT) \
	logfile$(OBJ_EXT) \
	memblock$(OBJ_EXT) \
	membuf$(OBJ_EXT) \
	ostrbase$(OBJ_EXT) \
	pscript$(OBJ_EXT)  \
	strutil$(OBJ_EXT) \
	systime$(OBJ_EXT) \
	ustring$(OBJ_EXT) 

# Optional debug objects
# NOTE: The leak test functions requires the /D__MSVC_DEBUG__ compiler
# flag and the /MDd or /MTd compiler flag. 
# GLIB_DEBUG_OBJECTS = leaktest$(OBJ_EXT)

# Term I/O objects
GLIB_TERM_OBJECTS = terminal$(OBJ_EXT)

# Console/GUI messaging gxcode objects
GLIB_MSG_OBJECTS = ehandler$(OBJ_EXT) \
	gxdstats$(OBJ_EXT)

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
OBJECTS = $(GLIB_OBJECTS)

$(PROJECT).lib:	$(OBJECTS) 
	$(LIBUTIL) $(LINK_FLAGS) $(OBJECTS) /OUT:$@ 
# ===============================================================

# install the library files
# ===============================================================
install:
	@echo Removing any old library files
	if exist $(LIB_INSTALL_DIR)\$(PROJECT)$(LIB_EXT) \
	del $(LIB_INSTALL_DIR)\$(PROJECT)$(LIB_EXT)

	@echo Copying static library file
	copy $(PROJECT)$(LIB_EXT) $(LIB_INSTALL_DIR)\$(PROJECT)$(LIB_EXT)

# Remove object files
# ===============================================================
clean:
	@echo Removing library files...
	if exist $(PROJECT)$(LIB_EXT) del $(PROJECT)$(LIB_EXT)

	@echo Removing all .SBR files from working directory...
	if exist *.sbr del *.sbr  

	@echo Removing all .VCW files from working directory...
	if exist *.vcw del *.vcw 

	@echo Removing all .PDB files from working directory...
	if exist *.pdb del *.pdb 

	@echo Removing all .WSP files from working directory...
	if exist *.wsp del *.wsp 

	@echo Removing all .BSC files from working directory...
	if exist *.bsc del *.bsc 

	@echo Removing all .SBT files from working directory...
	if exist *.sbt del *.sbt 

	@echo Removing all .ILK files from working directory...
	if exist *.ilk del *.ilk 

	@echo Removing all .IDB files from working directory...
	if exist *.idb del *.idb 

	@echo Removing all .MDP files from working directory...
	if exist *.mdp del *.mdp 

	@echo Removing all .PCH files from working directory...
	if exist *.pch del *.pch 

	@echo Removing all .NCB files from working directory...
	if exist *.ncb del *.ncb 

	@echo Removing all .OBJ files from working directory...
	if exist *.obj del *.obj 
# --------------------------------------------------------------- 
#####################
#### End of File ####
#####################


