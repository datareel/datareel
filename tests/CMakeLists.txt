# DataReel 7 tests that are programs. run_tests.sh runs the rest.
add_executable(damaged_files damaged_files.cpp)
target_link_libraries(damaged_files PRIVATE DataReel::datareel)
set_target_properties(damaged_files PROPERTIES
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
