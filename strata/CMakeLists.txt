cmake_minimum_required(VERSION 3.14)

project(strata LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/Zc:preprocessor)
endif()

add_library(strata SHARED
    src/property.cpp
    src/property.h
src/function.cpp
    src/function.h
    src/strata_export.h
    include/array_view.h
    include/common.h
    include/uid.h
    src/metadata_container.cpp
    src/metadata_container.h
    src/future.cpp
    src/future.h
    src/strata_impl.cpp
    src/strata_impl.h
    src/strata.cpp
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
    include/interface/intf_strata.h
    include/interface/intf_metadata.h
    include/interface/types.h
    include/api/any.h
    include/api/callback.h
    include/api/event.h
    include/api/property.h
    include/api/future.h
    include/api/function.h
    include/api/function_context.h
    include/api/strata.h
    include/ext/any.h
    include/ext/core_object.h
    include/ext/metadata.h
    include/ext/object.h
    include/ext/event.h
    include/ext/interface_dispatch.h
    include/ext/refcounted_dispatch.h
    )

target_include_directories(strata
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(strata PRIVATE STRATA_EXPORTS)
