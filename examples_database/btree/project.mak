$(PROJECT)$(OBJ_EXT):	$(PROJECT).cpp $(PROJECT_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) $(PROJECT).cpp

OBJECTS = $(PROJECT)$(OBJ_EXT)
