cmake_minimum_required(VERSION 3.5)

project(ltk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(ltk SHARED
    src/property.cpp
    src/property.h
    src/event.cpp
    src/event.h
    src/function.cpp
    src/function.h
    src/ltk_export.h
    common.h
    src/registry.cpp
    src/registry.h
    src/plugin.cpp
    interface/intf_any.h
    interface/intf_external_any.h
    interface/intf_object.h
    interface/intf_interface.h
    interface/refcnt_ptr.h
    interface/intf_function.h
    interface/intf_event.h
    interface/intf_property.h
    interface/intf_object_factory.h
    interface/intf_registry.h
    interface/intf_metadata.h
    interface/types.h
    api/any.h
    api/property.h
    api/function.h
    api/ltk.h
    ext/any.h
    ext/object.h
    ext/event.h
    )
