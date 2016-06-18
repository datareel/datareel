$(PROJECT)$(OBJ_EXT):	$(PROJECT).cpp $(PROJECT_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) $(PROJECT).cpp

m_thread$(OBJ_EXT):	m_thread.cpp $(M_THREAD_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) m_thread.cpp

OBJECTS = $(PROJECT)$(OBJ_EXT) m_thread$(OBJ_EXT)
