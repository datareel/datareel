#######################
#### Start of File ####
#######################
# --------------------------------------------------------------- 
# Makefile Contents: Linux makefile for the DataReel library
# C/C++ Compiler Used: GNU g++ 8 or higher (C++17)
# Produced By: DataReel Software Development Team
# File Creation Date: 05/25/2001
# Date Last Modified: 10/26/2026
# --------------------------------------------------------------- 
# 10/26/2026: DataReel 7 compiles every source file in ../src and
# tracks header dependencies with -MMD, so this makefile no longer
# lists the objects. CMakeLists.txt is the main build; use this file
# for "cd build; make; make install" style builds.
#
#   make            Build libdatareel.a and libdatareel.so.7.0
#   make install    Copy the libraries to ../lib
#   make LARGEFILES=1 FINAL=1    64-bit database engine, release build
# --------------------------------------------------------------- 
GCODE_LIB_DIR = ..

include ../env/linux.env

PROJECT = libdatareel

# gxsschan.cpp is the Windows Schannel version of gxssl.cpp and is
# compiled as part of it
SOURCES = $(filter-out ../src/gxsschan.cpp, $(wildcard ../src/*.cpp))
OBJECTS = $(notdir $(SOURCES:.cpp=$(OBJ_EXT)))
DEPENDS = $(OBJECTS:$(OBJ_EXT)=.d)

all: $(PROJECT)$(LIB_EXT) $(PROJECT)$(SHAREDLIB_EXT)

%$(OBJ_EXT): ../src/%.cpp
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) $(LIBCOMPILE_FLAGS) -MMD -MP $< \
	$(OUTPUT) $@

$(PROJECT)$(LIB_EXT): $(OBJECTS)
	$(LIBUTIL) $(LIBUTIL_FLAGS) $@ $(OBJECTS)
	$(RANLIB_UTIL) $@

$(PROJECT)$(SHAREDLIB_EXT): $(OBJECTS)
	$(SHARED_LIBUTIL) -Wl,-soname,$(PROJECT)$(SHAREDLIB_EXT) \
	-o $@ $(OBJECTS) $(LINK_LIBRARIES) $(LINKER_FLAGS)

-include $(DEPENDS)

install: all
	mkdir -p $(LIB_INSTALL_DIR)
	cp -f $(PROJECT)$(LIB_EXT) $(LIB_INSTALL_DIR)/$(PROJECT)$(LIB_EXT)
	cp -f $(PROJECT)$(SHAREDLIB_EXT) $(LIB_INSTALL_DIR)/$(PROJECT)$(SHAREDLIB_EXT)
	$(LN_S) $(PROJECT)$(SHAREDLIB_EXT) $(LIB_INSTALL_DIR)/$(PROJECT)$(DLIB_EXT)

clean:
	rm -f *$(OBJ_EXT) *.d $(PROJECT)$(LIB_EXT) $(PROJECT)$(SHAREDLIB_EXT)

clean_lib:
	rm -f $(LIB_INSTALL_DIR)/$(PROJECT)$(LIB_EXT)
	rm -f $(LIB_INSTALL_DIR)/$(PROJECT)$(DLIB_EXT)
	rm -f $(LIB_INSTALL_DIR)/$(PROJECT)$(SHAREDLIB_EXT)

.PHONY: all install clean clean_lib
# --------------------------------------------------------------- 
#####################
#### End of File ####
#####################
