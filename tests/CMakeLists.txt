include_directories(../src)

add_executable(test-cio test-cio.c)
target_link_libraries(test-cio cmocka cio pthread)
add_test(test-cio ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test-cio)

add_executable(test-tcp-stream test-tcp-stream.c)
target_link_libraries(test-tcp-stream cmocka cio pthread)
add_test(test-tcp-stream ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test-tcp-stream)

add_executable(test-unix-stream test-unix-stream.c)
target_link_libraries(test-unix-stream cmocka cio pthread)
add_test(test-unix-stream ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test-unix-stream)
