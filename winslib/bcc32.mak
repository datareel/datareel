#######################
#### Start of File ####
#######################
# --------------------------------------------------------------- 
# Makefile Contents: Makefile for the BCC32 gxcode library
# C/C++ Compiler Used: Borland C++ 5.5 for WIN32
# Produced By: DataReel Software Development Team
# File Creation Date: 05/25/2001 
# Date Last Modified: 10/17/2005
# --------------------------------------------------------------- 
# Setup my path to the gxcode library
GCODE_LIB_DIR = ..

!include bcc32.env

# Define a name for the executable
PROJECT = gxcode32

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
	wserror$(OBJ_EXT)

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
GLIB_DEBUG_OBJECTS = leaktest$(OBJ_EXT)

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

# Add additional library files here
LIBRARIES = 

$(PROJECT).lib:	$(OBJECTS)
	$(LIBUTIL) $(PROJECT).lib /P2048 @&&!
	+$(OBJECTS:.obj =.obj +) \
	+$(LIBRARIES:.lib =.lib +)
!
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

	@echo Removing all .tds files from working directory...
	if exist *.tds del *.tds 

	@echo Removing all .ils files from working directory...
	if exist *.ils del *.ils 

	@echo Removing all .ilf files from working directory...
	if exist *.ilf del *.ilf 

	@echo Removing all .ilc files from working directory...
	if exist *.ilc del *.ilc 

	@echo Removing all .ild files from working directory...
	if exist *.ild del *.ild 

	@echo Removing all .map files from working directory...
	if exist *.map del *.map 

	@echo Removing all .OBJ files from working directory...
	if exist *.obj del *.obj 
# --------------------------------------------------------------- 
#####################
#### End of File ####
#####################


