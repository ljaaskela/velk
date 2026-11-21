add_library(velk_remote SHARED
    src/codec.h
    src/codec.cpp
    src/connection.h
    src/connection.cpp
    src/driver.h
    src/remote_client.h
    src/remote_client.cpp
    src/remote_plugin.h
    src/remote_plugin.cpp
    src/remote_server.h
    src/remote_server.cpp
    src/socket.h
    src/socket.cpp
    src/values.h
    src/values.cpp
    include/velk/plugins/remote/plugin.h
    include/velk/plugins/remote/types.h
    include/velk/plugins/remote/interface/intf_remote.h
    include/velk/plugins/remote/api/remote.h
)

find_package(Threads REQUIRED)
target_link_libraries(velk_remote PRIVATE velk Threads::Threads)
if(WIN32)
    target_link_libraries(velk_remote PRIVATE ws2_32)
endif()

target_include_directories(velk_remote
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(velk_remote PRIVATE VELK_EXPORTS)
