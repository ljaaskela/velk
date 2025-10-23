cmake_minimum_required(VERSION 3.5)

project(propertytest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR})

add_executable(propertytest
    main.cpp
    src/property.cpp
    src/property.h
    src/event.cpp
    src/event.h
    src/function.cpp
    src/function.h
    common.h
    src/registry.cpp
    src/registry.h
    interface/intf_any.h
    interface/intf_external_any.h
    interface/intf_object.h
    interface/intf_interface.h
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
    ext/any.h
    ext/object.h
    ext/event.h
    )
