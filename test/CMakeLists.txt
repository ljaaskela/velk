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
    test_array_property.cpp
    test_property.cpp
    test_future.cpp
    test_function.cpp
    test_object.cpp
    test_plugin.cpp
    test_hive.cpp
    test_log.cpp
    test_uid.cpp
    test_string.cpp
    test_string_view.cpp
    test_shared_ptr.cpp
    test_vector.cpp
)

target_link_libraries(tests PRIVATE velk GTest::gtest_main)

# Pass the test plugin DLL path as a compile definition so tests can find it.
add_subdirectory(test_plugin_dll)
target_compile_definitions(tests PRIVATE
    TEST_PLUGIN_DLL_PATH="$<TARGET_FILE:test_plugin_dll>"
)

include(GoogleTest)
gtest_discover_tests(tests)
