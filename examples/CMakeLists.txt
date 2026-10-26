# DataReel 7 examples. Each program is one source file.

function(datareel_example name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE DataReel::datareel)
  set_target_properties(${name} PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")
endfunction()

# Object database
datareel_example(db_simple         database/simple/testprog.cpp)
datareel_example(db_database       database/database/testprog.cpp)
datareel_example(db_btree          database/btree/testprog.cpp)
datareel_example(db_btree_cached   database/btree_cached/testprog.cpp)
datareel_example(db_btree_node     database/btree_node/testprog.cpp)
datareel_example(db_database_cache database/database_cache/testprog.cpp)
datareel_example(db_defrag         database/defrag/testprog.cpp)
datareel_example(db_pod            database/pod/testprog.cpp)
datareel_example(db_infohog        database/infohog/infohog.cpp)
datareel_example(db_rdbms          database/rdbms/rdbms.cpp)

# TLS networking
datareel_example(tls_echo_server   tls/tls_echo_server.cpp)
datareel_example(tls_echo_client   tls/tls_echo_client.cpp)
datareel_example(https_get         tls/https_get.cpp)
datareel_example(https_server      tls/https_server.cpp)
datareel_example(smtp_send         tls/smtp_send.cpp)
datareel_example(pop3_list         tls/pop3_list.cpp)
datareel_example(ftps_client       tls/ftps_client.cpp)
datareel_example(stream_server     tls/stream_server.cpp)
datareel_example(stream_client     tls/stream_client.cpp)

# General purpose
datareel_example(config_file       general/config_file.cpp)
datareel_example(logfile           general/logfile.cpp)
datareel_example(thread_pool       general/thread_pool.cpp)

# Plain text diagnostics (ICMP and telnet cannot use TLS)
datareel_example(ping              diagnostics/ping.cpp)
datareel_example(telnet_client     diagnostics/telnet_client.cpp)

# Sample programs from the AI guides in docs/ai, built so they stay correct
foreach(sample tls_client tls_server https_fetch send_mail 
    record_store indexed_records)
  datareel_example(ai_${sample} ${PROJECT_SOURCE_DIR}/docs/ai/samples/${sample}.cpp)
endforeach()
