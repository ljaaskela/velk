cmake_minimum_required(VERSION 3.5)

project(demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(demo main.cpp)
target_link_libraries(demo PRIVATE strata)
