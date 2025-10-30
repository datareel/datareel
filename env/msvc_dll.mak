#######################
#### Start of File ####
#######################
# --------------------------------------------------------------- 
# Makefile Contents: Makefile for command line builds
# C/C++ Compiler Used: MSVC 6.0 SP5, Visual C++ .NET, and VS 2022
# Produced By: DataReel Software Development Team
# File Creation Date: 05/25/2001
# Date Last Modified: 10/30/2025
# --------------------------------------------------------------- 
# Setup my path to the gxcode DLL library
GCODE_LIB_PATH = ../..

# Setup library configuration
!IF "$(FINAL)" == ""
#FINAL = 0 # Default to debug configuration
FINAL = 1 # Default to release configuration
!MESSAGE No library configuration specified. Defaulting to debug configuration.
!ENDIF

!IF "$(64BITCFG)" == ""
64BITCFG = 0 # Default to 32-bit configuration
#64BITCFG = 1 # Default to 64-bit configuration
!MESSAGE No 64-bit configuration specified. Defaulting to 32-bit configuration.
!ENDIF

!include $(GCODE_LIB_PATH)/dll/msvc.env

# Define a name for the executable
# PROJECT = 

# Compile the files and build the executable
# ===============================================================
all:	$(PROJECT)$(EXE_EXT)

!include project.mak

# Setup the path to the gxcode library
!IF "$(FINAL)" == "1"
GXCODE_LIB_PATH = $(GCODE_LIB_PATH)$(PATHSEP)dll$(PATHSEP)release
!ELSE
GXCODE_LIB_PATH = $(GCODE_LIB_PATH)$(PATHSEP)dll$(PATHSEP)debug
!ENDIF
!IF "$(64BITCFG)" == "1" && "$(FINAL)" == "1"
LIB_GXCODE = $(GXCODE_LIB_PATH)$(PATHSEP)gxcode64.lib
!ENDIF
!IF "$(64BITCFG)" == "1" && "$(FINAL)" == "0"
LIB_GXCODE = $(GXCODE_LIB_PATH)$(PATHSEP)gxcode64d.lib
!ENDIF
!IF "$(64BITCFG)" == "0" && "$(FINAL)" == "1"
LIB_GXCODE = $(GXCODE_LIB_PATH)$(PATHSEP)gxcode32.lib
!ENDIF
!IF "$(64BITCFG)" == "0" && "$(FINAL)" == "0"
LIB_GXCODE = $(GXCODE_LIB_PATH)$(PATHSEP)gxcode32d.lib
!ENDIF

# Library files
LIBRARIES = $(LIB_GXCODE) $(LINK_LIBRARIES) setargv.obj # Expand wildcards

$(PROJECT)$(EXE_EXT):	$(OBJECTS) 
	$(LINKER) $(LINK_FLAGS) $(OBJECTS) $(LIBRARIES) /OUT:$@ 
# ===============================================================

# Remove OBJS, debug files, and executable after running nmake 
# ===============================================================
clean:
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

	@echo Removing the EXECUTABLE file from working directory...
	if exist $(PROJECT).exe del $(PROJECT).exe 

	@echo Removing all test LOG files from working directory...
	if exist *.log del *.log 

	@echo Removing all test OUT files from working directory...
	if exist *.out del *.out 

	@echo Removing all temporary ARP cache files from working directory...
	if exist arpcache.txt del arpcache.txt

	@echo Removing all test EDS files from working directory...
	if exist *.eds del *.eds 

	@echo Removing all test DATABASE files from working directory...
	if exist *.gxd del *.gxd 

	@echo Removing all test INDEX files from working directory...
	if exist *.btx del *.btx 
	if exist *.gix del *.gix

	@echo Removing all test InfoHog files from working directory...
	if exist *.ihd del *.ihd 
	if exist *.ihx del *.ihx 
# --------------------------------------------------------------- 
#####################
#### End of File ####
#####################


