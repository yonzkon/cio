include_directories(../src)

add_executable(test-cio test_cio.c)
target_link_libraries(test-cio cmocka cio pthread)
add_test(test-apix ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test-cio)

add_executable(test-cio-stream test_cio_stream.c)
target_link_libraries(test-cio-stream cmocka cio pthread)
add_test(test-apix ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test-cio-stream)
