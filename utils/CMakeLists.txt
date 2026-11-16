# DataReel 7 database utilities

foreach(tool db_debug ix_debug vix_rebuild)
  add_executable(${tool} ${tool}/${tool}.cpp)
  target_link_libraries(${tool} PRIVATE DataReel::datareel)
endforeach()

# 11/07/2026: gxrepl, database replication over gxStream (POSIX only)
if(NOT WIN32)
  # 11/16/2026: objrepl, the object replication prototype (objects only over the wire, independent copies)
  add_executable(objrepl objrepl/objrepl.cpp)
  target_link_libraries(objrepl PRIVATE DataReel::datareel)
  add_executable(gxrepl gxrepl/gxrepl.cpp)
  target_link_libraries(gxrepl PRIVATE DataReel::datareel)
  # 11/14/2026: derived index extractors loaded from a .so take DataReel from gxrepl (one copy of the library, so
  # their writes go through gxrepl's staged scope); the test build also has the built-in test extractor
  set_target_properties(gxrepl PROPERTIES ENABLE_EXPORTS ON)
  if(GXREPL_TEST_HOOKS)
    target_sources(gxrepl PRIVATE gxrepl/gxrepl_testextractor.cpp)
    target_compile_definitions(gxrepl PRIVATE GXREPL_TEST_EXTRACTOR=1)
    add_library(gxrepl_testextractor MODULE gxrepl/gxrepl_testextractor.cpp)
    target_include_directories(gxrepl_testextractor PRIVATE $<TARGET_PROPERTY:DataReel::datareel,INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(gxrepl_testextractor PRIVATE $<TARGET_PROPERTY:DataReel::datareel,INTERFACE_COMPILE_DEFINITIONS>)
    set_target_properties(gxrepl_testextractor PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/utils")
  endif()
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
  set(_dr_repl_tools gxrepl gxrepl_testwriter gxdbmux_test object_server object_client)
endif()

# 11/12/2026: tests of the vector index (gxvindex.h), DataReel 7.4 (11/14/2026: on Windows too; its crash
# rounds are POSIX only)
add_executable(vix_test vix_test/vix_test.cpp)
target_link_libraries(vix_test PRIVATE DataReel::datareel)
# 11/14/2026 (phase D): files made by the phase A-C library, for the compatibility test
target_compile_definitions(vix_test PRIVATE VIX_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/vix_test/data")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(vix_test PRIVATE -ffp-contract=off)
elseif(MSVC)
  target_compile_options(vix_test PRIVATE /fp:precise)
endif()
list(APPEND _dr_repl_tools vix_test)
# 11/15/2026: the numbers of docs/benchmarks.html (dr_bench [WORK_FOLDER] [--quick])
add_executable(dr_bench dr_bench/dr_bench.cpp)
target_link_libraries(dr_bench PRIVATE DataReel::datareel)
list(APPEND _dr_repl_tools dr_bench)

set_target_properties(db_debug ix_debug vix_rebuild ${_dr_repl_tools} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/utils")
install(TARGETS db_debug ix_debug vix_rebuild ${_dr_repl_tools}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
