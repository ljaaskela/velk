cmake_minimum_required(VERSION 3.14)

project(velk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/Zc:preprocessor /wd5105)
endif()

add_library(velk SHARED
    src/property.cpp
    src/property.h
    src/function.cpp
    src/function.h
    include/velk/velk_export.h
    include/velk/array_view.h
    include/velk/common.h
    include/velk/memory.h
    include/velk/string_view.h
    include/velk/uid.h
    src/metadata_container.cpp
    src/metadata_container.h
    src/future.cpp
    src/future.h
    src/velk_instance.cpp
    src/velk_instance.h
    src/velk.cpp
    include/velk/interface/intf_any.h
    include/velk/interface/intf_external_any.h
    include/velk/interface/intf_object.h
    include/velk/interface/intf_interface.h
    include/velk/interface/refcnt_ptr.h
    include/velk/interface/intf_function.h
    include/velk/interface/intf_event.h
    include/velk/interface/intf_future.h
    include/velk/interface/intf_property.h
    include/velk/interface/intf_object_factory.h
    include/velk/interface/intf_type_registry.h
    include/velk/interface/intf_velk.h
    include/velk/interface/intf_metadata.h
    include/velk/interface/types.h
    include/velk/api/any.h
    include/velk/api/callback.h
    include/velk/api/event.h
    include/velk/api/property.h
    include/velk/api/future.h
    include/velk/api/function.h
    include/velk/api/function_context.h
    include/velk/api/traits.h
    include/velk/api/velk.h
    include/velk/ext/any.h
    include/velk/ext/core_object.h
    include/velk/ext/metadata.h
    include/velk/ext/object.h
    include/velk/ext/event.h
    include/velk/ext/interface_dispatch.h
    include/velk/ext/refcounted_dispatch.h
    )

target_include_directories(velk
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

option(VELK_ENABLE_BLOCK_POOL "Enable thread-local control_block pooling" ON)

target_compile_definitions(velk PRIVATE
    VELK_EXPORTS
    VELK_ENABLE_BLOCK_POOL=$<BOOL:${VELK_ENABLE_BLOCK_POOL}>
)

# No RTTI, no exceptions for the library itself.
# Strip CMake's default /EHsc and /GR to avoid D9025 override warnings.
# These string(REPLACE) calls are directory-scoped and only affect targets in this subdirectory.
if(MSVC)
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    target_compile_options(velk PRIVATE /GR- /EHs-c-)
    target_compile_definitions(velk PRIVATE _HAS_EXCEPTIONS=0)
else()
    target_compile_options(velk PRIVATE -fno-rtti -fno-exceptions)
endif()
