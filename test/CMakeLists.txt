cmake_minimum_required(VERSION 3.5)

project(test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR})
include_directories(../strata)

add_executable(test main.cpp)
target_link_libraries(test PRIVATE strata)
add_dependencies(test strata)
