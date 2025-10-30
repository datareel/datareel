#######################
#### Start of File ####
#######################
# --------------------------------------------------------------- 
# Makefile Contents: Makefile for the MSVC gxcode dynamic link library
# C/C++ Compiler Used: MSVC 6.0 SP5, Visual C++ .NET, and VS 2022
# Produced By: DataReel Software Development Team
# File Creation Date: 05/25/2001
# Date Last Modified: 10/30/2025
# --------------------------------------------------------------- 

# Compile the object files and build the DLL, .exp, .pch, .lib files
# ===============================================================
all:
	nmake -f project.mak all FINAL=0 64BITCFG=0 USESSL=0
	nmake -f project.mak all FINAL=1 64BITCFG=0 USESSL=0

# install the library files
# ===============================================================
install:
	nmake -f project.mak install FINAL=0 64BITCFG=0 USESSL=0
	nmake -f project.mak install FINAL=1 64BITCFG=0 USESSL=0

# Remove library and object files
# ===============================================================
clean:
	nmake -f project.mak clean FINAL=0 64BITCFG=0 USESSL=0
	nmake -f project.mak clean FINAL=1 64BITCFG=0 USESSL=0
# --------------------------------------------------------------- 
#####################
#### End of File ####
#####################


