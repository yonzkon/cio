include_directories(../src)

add_executable(test-cio test_cio.c)
target_link_libraries(test-cio cmocka cio pthread)
add_test(test-apix ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test-cio)

add_executable(test-tcp-stream test_tcp_stream.c)
target_link_libraries(test-tcp-stream cmocka cio pthread)
add_test(test-apix ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test-tcp-stream)

add_executable(test-unix-stream test_unix_stream.c)
target_link_libraries(test-unix-stream cmocka cio pthread)
add_test(test-apix ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test-unix-stream)
