PROJECT_DEP = drcm_server.h  m_app.h  m_config.h  m_dbase.h \
m_help.h  m_log.h  m_proc.h  m_socket.h  m_stats.h  m_thread.h 

M_THREAD_DEP = $(PROJECT_DEP)

M_SOCKET_DEP = $(PROJECT_DEP)

M_LOG_DEP = $(PROJECT_DEP)

M_HELP_DEP = $(PROJECT_DEP)

M_PROC_DEP = $(PROJECT_DEP)

M_DBASE_DEP = $(PROJECT_DEP)

M_APP_DEP = $(PROJECT_DEP)

M_CONFIG_DEP = $(PROJECT_DEP)

M_STATS_DEP = $(PROJECT_DEP)

$(PROJECT)$(OBJ_EXT):	$(PROJECT).cpp $(PROJECT_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) $(PROJECT).cpp

m_thread$(OBJ_EXT):	m_thread.cpp $(M_THREAD_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) m_thread.cpp

m_socket$(OBJ_EXT):	m_socket.cpp $(M_SOCKET_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) m_socket.cpp

m_log$(OBJ_EXT):	m_log.cpp $(M_LOG_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) m_log.cpp

m_help$(OBJ_EXT):	m_help.cpp $(M_HELP_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) m_help.cpp

m_proc$(OBJ_EXT):	m_proc.cpp $(M_PROC_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) m_proc.cpp

m_dbase$(OBJ_EXT):	m_dbase.cpp $(M_DBASE_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) m_dbase.cpp

m_app$(OBJ_EXT):	m_app.cpp $(M_APP_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) m_app.cpp

m_config$(OBJ_EXT):	m_config.cpp $(M_CONFIG_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) m_config.cpp

m_stats$(OBJ_EXT):	m_stats.cpp $(M_STATS_DEP)
	$(CXX) $(COMPILE_ONLY) $(COMPILE_FLAGS) m_stats.cpp

OBJECTS = $(PROJECT)$(OBJ_EXT) \
m_thread$(OBJ_EXT) \
m_socket$(OBJ_EXT) \
m_help$(OBJ_EXT) \
m_log$(OBJ_EXT) \
m_proc$(OBJ_EXT) \
m_dbase$(OBJ_EXT) \
m_app$(OBJ_EXT) \
m_config$(OBJ_EXT) \
m_stats$(OBJ_EXT)
