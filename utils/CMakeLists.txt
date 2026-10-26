# DataReel 7 database utilities

foreach(tool db_debug ix_debug)
  add_executable(${tool} ${tool}/${tool}.cpp)
  target_link_libraries(${tool} PRIVATE DataReel::datareel)
endforeach()

set_target_properties(db_debug ix_debug PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/utils")
install(TARGETS db_debug ix_debug 
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
