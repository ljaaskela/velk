cmake_minimum_required(VERSION 3.14)

# Extract vendored Google Benchmark tarball into build dir
set(BENCH_ARCHIVE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/benchmark-1.9.1.tar.gz)
set(BENCH_EXTRACT_DIR ${CMAKE_CURRENT_BINARY_DIR}/benchmark)

if(NOT EXISTS ${BENCH_EXTRACT_DIR}/benchmark-1.9.1/CMakeLists.txt)
    file(MAKE_DIRECTORY ${BENCH_EXTRACT_DIR})
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf ${BENCH_ARCHIVE}
        WORKING_DIRECTORY ${BENCH_EXTRACT_DIR}
    )
endif()

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(
    ${BENCH_EXTRACT_DIR}/benchmark-1.9.1
    ${BENCH_EXTRACT_DIR}/build
    EXCLUDE_FROM_ALL
)

add_executable(benchmarks main.cpp)
target_link_libraries(benchmarks PRIVATE strata benchmark::benchmark benchmark::benchmark_main)
