add_library(velk_c SHARED
    src/velk_c.cpp
    include/velk_c.h
)

target_link_libraries(velk_c PRIVATE velk)

target_include_directories(velk_c
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(velk_c PRIVATE VELK_C_EXPORTS)
