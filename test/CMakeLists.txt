cmake_minimum_required(VERSION 3.14)

# Extract vendored GoogleTest tarball into build dir
set(GTEST_ARCHIVE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest-1.14.0.tar.gz)
set(GTEST_EXTRACT_DIR ${CMAKE_CURRENT_BINARY_DIR}/googletest)

if(NOT EXISTS ${GTEST_EXTRACT_DIR}/googletest-1.14.0/CMakeLists.txt)
    file(MAKE_DIRECTORY ${GTEST_EXTRACT_DIR})
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf ${GTEST_ARCHIVE}
        WORKING_DIRECTORY ${GTEST_EXTRACT_DIR}
    )
endif()

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(
    ${GTEST_EXTRACT_DIR}/googletest-1.14.0
    ${GTEST_EXTRACT_DIR}/build
    EXCLUDE_FROM_ALL
)

add_executable(tests
    test_any.cpp
    test_property.cpp
    test_future.cpp
    test_function.cpp
    test_object.cpp
    test_uid.cpp
)

target_link_libraries(tests PRIVATE strata GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(tests)
