# DataReel 7 tests that are programs. run_tests.sh runs the rest.
add_executable(damaged_files damaged_files.cpp)
target_link_libraries(damaged_files PRIVATE DataReel::datareel)
set_target_properties(damaged_files PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# 11/16/2026: journal salts distinct across forked processes (POSIX: fork)
if(NOT WIN32)
  add_executable(journal_salt_test journal_salt_test.cpp)
  target_link_libraries(journal_salt_test PRIVATE DataReel::datareel)
  set_target_properties(journal_salt_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
endif()

# 11/16/2026: block reuse, best fit and first fit (ReclaimBestFit() reused only exact fits)
add_executable(reclaim_test reclaim_test.cpp)
target_link_libraries(reclaim_test PRIVATE DataReel::datareel)
set_target_properties(reclaim_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

add_executable(journal_test journal_test.cpp)
target_link_libraries(journal_test PRIVATE DataReel::datareel)
set_target_properties(journal_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# 11/16/2026 (7.4): the engine signature and the layout detection (layout_detect_test.cpp)
add_executable(layout_detect_test layout_detect_test.cpp)
target_link_libraries(layout_detect_test PRIVATE DataReel::datareel)
set_target_properties(layout_detect_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# 11/16/2026 (7.4): the database digests (gxddigest.h); derived_apply_test went with gxrepl
add_executable(digest_test digest_test.cpp)
target_link_libraries(digest_test PRIVATE DataReel::datareel)
set_target_properties(digest_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

add_executable(stream_security_test stream_security_test.cpp)
target_link_libraries(stream_security_test PRIVATE DataReel::datareel)
set_target_properties(stream_security_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

add_executable(proto_security_test proto_security_test.cpp)
target_link_libraries(proto_security_test PRIVATE DataReel::datareel)
set_target_properties(proto_security_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

add_executable(support_security_test support_security_test.cpp)
target_link_libraries(support_security_test PRIVATE DataReel::datareel)
set_target_properties(support_security_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# 11/16/2026 (7.4 beta5, G4-2): a plain text IMAP login needs AllowPlainLogin(); run_tests runs it on the mock server
add_executable(imap_login_test imap_login_test.cpp)
target_link_libraries(imap_login_test PRIVATE DataReel::datareel)
set_target_properties(imap_login_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# 11/17/2026: gxProcessLock (gxproclock.h), one instance at a time (POSIX: fork)
if(NOT WIN32)
  add_executable(proclock_test proclock_test.cpp)
  target_link_libraries(proclock_test PRIVATE DataReel::datareel)
  set_target_properties(proclock_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
endif()
