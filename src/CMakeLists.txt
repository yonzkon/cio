file(GLOB SRC *.c)
file(GLOB INC cio.h cio-stream.h)

if (BUILD_STATIC)
    add_library(cio-static STATIC ${SRC} ${SRC_POSIX})
    set_target_properties(cio-static PROPERTIES OUTPUT_NAME cio)
    set_target_properties(cio-static PROPERTIES PUBLIC_HEADER "${INC}")
    set(TARGET_STATIC cio-static)
endif ()

if (BUILD_SHARED)
    add_library(cio SHARED ${SRC} ${SRC_POSIX})
    set_target_properties(cio PROPERTIES PUBLIC_HEADER "${INC}")
    set_target_properties(cio PROPERTIES VERSION 0.0.1 SOVERSION 0)
    set(TARGET_SHARED cio)
endif ()

install(TARGETS ${TARGET_STATIC} ${TARGET_SHARED}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)
