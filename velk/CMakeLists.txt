cmake_minimum_required(VERSION 3.14)

project(velk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/Zc:preprocessor)
endif()

add_library(velk SHARED
    src/property.cpp
    src/property.h
src/function.cpp
    src/function.h
    src/velk_export.h
    include/array_view.h
    include/common.h
    include/uid.h
    src/metadata_container.cpp
    src/metadata_container.h
    src/future.cpp
    src/future.h
    src/velk_impl.cpp
    src/velk_impl.h
    src/velk.cpp
    include/interface/intf_any.h
    include/interface/intf_external_any.h
    include/interface/intf_object.h
    include/interface/intf_interface.h
    include/interface/refcnt_ptr.h
    include/interface/intf_function.h
    include/interface/intf_event.h
    include/interface/intf_future.h
    include/interface/intf_property.h
    include/interface/intf_object_factory.h
    include/interface/intf_velk.h
    include/interface/intf_metadata.h
    include/interface/types.h
    include/api/any.h
    include/api/callback.h
    include/api/event.h
    include/api/property.h
    include/api/future.h
    include/api/function.h
    include/api/function_context.h
    include/api/velk.h
    include/ext/any.h
    include/ext/core_object.h
    include/ext/metadata.h
    include/ext/object.h
    include/ext/event.h
    include/ext/interface_dispatch.h
    include/ext/refcounted_dispatch.h
    )

target_include_directories(velk
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(velk PRIVATE VELK_EXPORTS)

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
