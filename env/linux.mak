#######################
#### Start of File ####
#######################
# --------------------------------------------------------------- 
# Makefile Contents: Makefile template for command line builds
# C/C++ Compiler Used: GNU, Intel, Cray
# Produced By: DataReel Software Development Team
# File Creation Date: 05/25/2001
# Date Last Modified: 06/06/2016
# --------------------------------------------------------------- 
# Setup my path to the gxcode library
GCODE_LIB_DIR = ../..

include $(GCODE_LIB_DIR)/env/linux.env

# Compile the files and build the executable
# ===============================================================
all:	$(PROJECT)

include project.mak

# Setup the path to the gxcode library
GXCODE_LIB_PATH = -L$(GCODE_LIB_DIR)$(PATHSEP)lib

# Link to shared libary
## LIB_GXCODE = -ldatareel

# Link to static library 
LIB_GXCODE = $(GCODE_LIB_DIR)$(PATHSEP)lib/libdatareel.a

ifeq ($(USE_OPEN_SSL),YES)
LIB_GXCODE = $(GCODE_LIB_DIR)$(PATHSEP)lib/libdatareel_ssl.a
endif


$(PROJECT):	$(OBJECTS)
	$(CXX) $(COMPILE_FLAGS) $(OBJECTS) \
	$(GXCODE_LIB_PATH) $(LIB_GXCODE) $(LINK_LIBRARIES) \
	$(OUTPUT) \
	$(PROJECT) $(LINKER_FLAGS)
# ===============================================================

# Remove object files and the executable after running make 
# ===============================================================
clean:
	echo Removing all OBJECT files from working directory...
	rm -f *.o 

	echo Removing EXECUTABLE file from working directory...
	rm -f $(PROJECT)

	echo Removing all test LOG files from working directory...
	rm -f *.log 

	echo Removing all test OUT files from working directory...
	rm -f *.out 

	echo Removing all test EDS files from working directory...
	rm -f *.eds 

	echo Removing all test DATABASE files from working directory...
	rm -f *.gxd 

	echo Removing all test INDEX files from working directory...
	rm -f *.btx 
	rm -f *.gix

	echo Removing all test InfoHog files from working directory...
	rm -f *.ihd 
	rm -f *.ihx 
# --------------------------------------------------------------- 
#####################
#### End of File ####
#####################
