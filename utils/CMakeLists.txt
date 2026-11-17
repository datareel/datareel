# DataReel 7 database utilities

# 11/16/2026: dr_digest (gxddigest.h), the database digests gxrepl's "digest" printed
foreach(tool db_debug ix_debug vix_rebuild dr_digest)
  add_executable(${tool} ${tool}/${tool}.cpp)
  target_link_libraries(${tool} PRIVATE DataReel::datareel)
endforeach()

# 11/16/2026: gxrepl (page-shipping replication) went from 7.4 (replication is gxObjRepl, gxobjrepl.h); the database
# object server and its multiplex test stay (POSIX only)
if(NOT WIN32)
  # 11/08/2026: tests of the database object server's multiplex (gxdbmux.h)
  add_executable(gxdbmux_test dbserver/gxdbmux_test.cpp)
  target_link_libraries(gxdbmux_test PRIVATE DataReel::datareel)
  # 11/08/2026: database object server examples
  add_executable(object_server dbserver/object_server.cpp)
  target_link_libraries(object_server PRIVATE DataReel::datareel)
  add_executable(object_client dbserver/object_client.cpp)
  target_link_libraries(object_client PRIVATE DataReel::datareel)
  set(_dr_repl_tools gxdbmux_test object_server object_client)
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
# 11/15/2026: an old 4-byte layout data file rewritten in the canonical layout (gxd_upgrade IN OUT)
add_executable(gxd_upgrade gxd_upgrade/gxd_upgrade.cpp)
target_link_libraries(gxd_upgrade PRIVATE DataReel::datareel)
list(APPEND _dr_repl_tools gxd_upgrade)
# 11/15/2026: the numbers of docs/benchmarks.html (dr_bench [WORK_FOLDER] [--quick])
add_executable(dr_bench dr_bench/dr_bench.cpp)
target_link_libraries(dr_bench PRIVATE DataReel::datareel)
list(APPEND _dr_repl_tools dr_bench)

set_target_properties(db_debug ix_debug vix_rebuild dr_digest ${_dr_repl_tools} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/utils")
install(TARGETS db_debug ix_debug vix_rebuild dr_digest ${_dr_repl_tools}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
