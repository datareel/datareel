# DataReel 7 database utilities

foreach(tool db_debug ix_debug)
  add_executable(${tool} ${tool}/${tool}.cpp)
  target_link_libraries(${tool} PRIVATE DataReel::datareel)
endforeach()

# 11/07/2026: gxrepl, database replication over gxStream (POSIX only)
if(NOT WIN32)
  add_executable(gxrepl gxrepl/gxrepl.cpp)
  target_link_libraries(gxrepl PRIVATE DataReel::datareel)
  add_executable(gxrepl_testwriter gxrepl/gxrepl_testwriter.cpp)
  target_link_libraries(gxrepl_testwriter PRIVATE DataReel::datareel)
  set(_dr_repl_tools gxrepl gxrepl_testwriter)
endif()

set_target_properties(db_debug ix_debug ${_dr_repl_tools} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/utils")
install(TARGETS db_debug ix_debug ${_dr_repl_tools}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
