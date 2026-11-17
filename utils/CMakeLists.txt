# DataReel 7 database utilities

# 11/16/2026: dr_digest (gxddigest.h), the database digests gxrepl's "digest" printed
foreach(tool db_debug ix_debug vix_rebuild dr_digest)
  add_executable(${tool} ${tool}/${tool}.cpp)
  target_link_libraries(${tool} PRIVATE DataReel::datareel)
endforeach()

# 11/16/2026: objrepl (gxObjRepl's tool, gxobjrepl.h) and its workload simulator: every platform. The test scripts
# (utils/objrepl/test_objrepl*.sh) need a build with -DGXREPL_TEST_HOOKS=ON for their kill and drop cases
add_executable(objrepl objrepl/objrepl.cpp)
target_link_libraries(objrepl PRIVATE DataReel::datareel)
add_executable(objrepl_sim objrepl/objrepl_sim.cpp)
target_link_libraries(objrepl_sim PRIVATE DataReel::datareel)

# 11/16/2026: gxObjRepl's API test, on every platform (Linux <-> Windows replication is a 7.4 feature: the same
# tests on both)
add_executable(test_gxobjrepl_api objrepl/test_gxobjrepl_api.cpp)
target_link_libraries(test_gxobjrepl_api PRIVATE DataReel::datareel)
if(WIN32)
  target_link_libraries(test_gxobjrepl_api PRIVATE ws2_32)
endif()

# 11/16/2026: gxObjRepl's property test and fuzzer (fork, sockets): POSIX only
if(NOT WIN32)
  # 11/16/2026: a randomized property test (two nodes, kills, drops, takeovers, rejoins) and a wire fuzzer for the copy
  # (run_prop_objrepl.sh, run_fuzz_objrepl.sh; the fuzzer is meant for a -fsanitize=address,undefined build)
  add_executable(prop_objrepl objrepl/prop_objrepl.cpp)
  target_link_libraries(prop_objrepl PRIVATE DataReel::datareel)
  add_executable(fuzz_objrepl objrepl/fuzz_objrepl.cpp)
  target_link_libraries(fuzz_objrepl PRIVATE DataReel::datareel)
endif()

# 11/16/2026: gxrepl (page-shipping replication) went from 7.4 (replication is gxObjRepl); the database object
# server and its multiplex test stay. 11/17/2026 (7.4): on Windows too (Winsock, Schannel)
# 11/08/2026: tests of the database object server's multiplex (gxdbmux.h)
add_executable(gxdbmux_test dbserver/gxdbmux_test.cpp)
target_link_libraries(gxdbmux_test PRIVATE DataReel::datareel)
# 11/08/2026: database object server examples
add_executable(object_server dbserver/object_server.cpp)
target_link_libraries(object_server PRIVATE DataReel::datareel)
add_executable(object_client dbserver/object_client.cpp)
target_link_libraries(object_client PRIVATE DataReel::datareel)
set(_dr_repl_tools gxdbmux_test object_server object_client)

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
