# DataReel 7 database utilities

foreach(tool db_debug ix_debug)
  add_executable(${tool} ${tool}/${tool}.cpp)
  target_link_libraries(${tool} PRIVATE DataReel::datareel)
endforeach()

# 11/07/2026: gxrepl, database replication over gxStream (POSIX only)
if(NOT WIN32)
  add_executable(gxrepl gxrepl/gxrepl.cpp)
  target_link_libraries(gxrepl PRIVATE DataReel::datareel)
  include(${CMAKE_CURRENT_SOURCE_DIR}/gxrepl/gxrepl_version.cmake)
  gxrepl_stamp(gxrepl)
  add_executable(gxrepl_testwriter gxrepl/gxrepl_testwriter.cpp)
  target_link_libraries(gxrepl_testwriter PRIVATE DataReel::datareel)
  # 11/08/2026: tests of the database object server's multiplex (gxdbmux.h)
  add_executable(gxdbmux_test gxrepl/gxdbmux_test.cpp)
  target_link_libraries(gxdbmux_test PRIVATE DataReel::datareel)
  # 11/08/2026: database object server examples
  add_executable(object_server dbserver/object_server.cpp)
  target_link_libraries(object_server PRIVATE DataReel::datareel)
  add_executable(object_client dbserver/object_client.cpp)
  target_link_libraries(object_client PRIVATE DataReel::datareel)
  # 11/12/2026: tests of the vector index (gxvindex.h), DataReel 7.4
  add_executable(vix_test vix_test/vix_test.cpp)
  target_link_libraries(vix_test PRIVATE DataReel::datareel)
  # 11/14/2026 (phase D): files made by the phase A-C library, for the compatibility test
  target_compile_definitions(vix_test PRIVATE VIX_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/vix_test/data")
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(vix_test PRIVATE -ffp-contract=off)
  endif()
  set(_dr_repl_tools gxrepl gxrepl_testwriter gxdbmux_test object_server object_client vix_test)
endif()

set_target_properties(db_debug ix_debug ${_dr_repl_tools} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/utils")
install(TARGETS db_debug ix_debug ${_dr_repl_tools}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
