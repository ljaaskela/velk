cmake_minimum_required(VERSION 3.14)

project(velktest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Place the dll + exe in the same place
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin/)

add_subdirectory(velk)
add_subdirectory(demo)
add_subdirectory(test)
add_subdirectory(benchmark)

