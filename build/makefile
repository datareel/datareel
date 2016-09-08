#######################
#### Start of File ####
#######################
# --------------------------------------------------------------- 
# Makefile Contents: make file to build datareel library
# C/C++ Compiler Used: GNU, Intel, Cray
# Produced By: DataReel Software Development Team
# File Creation Date: 05/25/2001
# Date Last Modified: 06/11/2016
# --------------------------------------------------------------- 
# Setup my path to the gxcode library
GCODE_LIB_DIR = ..

include ../env/linux.env

# Define a name for the static library
PROJECT = libdatareel

ifeq ($(USE_OPEN_SSL),YES)
PROJECT = libdatareel_ssl
endif

# Build dependency rules
# ===============================================================
include ../env/glibdeps.mak
# ===============================================================

# Compile the object files and build the library
# ===============================================================
all: CREATE_ALL_LIBS

include ../env/glibobjs.mak

COMPILE_FLAGS := $(COMPILE_FLAGS) $(LIBCOMPILE_FLAGS)

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
GLIB_DEBUG_OBJECTS = leaktest$(OBJ_EXT)

# Term I/O objects
GLIB_TERM_OBJECTS = terminal$(OBJ_EXT)

# Console/GUI messaging gxcode objects
GLIB_MSG_OBJECTS = ehandler$(OBJ_EXT) \
	gxdstats$(OBJ_EXT)

# Set the library components to compile here
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

# Add additional object files here
OBJECTS = $(GLIB_OBJECTS) 

CREATE_ALL_LIBS:	$(OBJECTS)
	echo Building static libary
	$(LIBUTIL) $(LIBUTIL_FLAGS) ./$(PROJECT)$(LIB_EXT) $(OBJECTS)
	$(RANLIB_UTIL) ./$(PROJECT)$(LIB_EXT)

	echo Building shared library
	$(SHARED_LIBUTIL) $(SHAREDLIBUTIL_FLAGS) \
	-o ./$(PROJECT)$(SHAREDLIB_EXT) $(OBJECTS) $(LINKER_FLAGS)
# ===============================================================

# install the library files
# ===============================================================
install:
	echo Removing any old library files
	rm -f $(LIB_INSTALL_DIR)/$(PROJECT)$(LIB_EXT)
	rm -f $(LIB_INSTALL_DIR)/$(PROJECT)$(DLIB_EXT)
	rm -f $(LIB_INSTALL_DIR)/$(PROJECT)$(SHAREDLIB_EXT)

	echo Copying library files
	cp $(PROJECT)$(LIB_EXT) $(LIB_INSTALL_DIR)/$(PROJECT)$(LIB_EXT)
	cp $(PROJECT)$(SHAREDLIB_EXT) \
	$(LIB_INSTALL_DIR)/$(PROJECT)$(SHAREDLIB_EXT)

	echo Creating symbolic links
	$(LN_S) $(LIB_INSTALL_DIR)/$(PROJECT)$(SHAREDLIB_EXT) \
	$(LIB_INSTALL_DIR)/$(PROJECT)$(DLIB_EXT)

# Remove object files and libary files from current directory 
# ===============================================================
clean:
	echo Removing all OBJECT files from working directory...
	rm -f *.o 

	echo Removing static and shared libary files
	rm -f $(PROJECT)$(LIB_EXT)
	rm -f $(PROJECT)$(SHAREDLIB_EXT)

clean_lib:
	rm -fv ../lib/libdatareel.a
	rm -fv ../lib/libdatareel.so
	rm -fv ../lib/libdatareel.so.5.0
# --------------------------------------------------------------- 
#####################
#### End of File ####
#####################
