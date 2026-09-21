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
    test_animation.cpp
    test_array_property.cpp
    test_binding.cpp
    test_property.cpp
    test_future.cpp
    test_task_pool.cpp
    test_function.cpp
    test_object.cpp
    test_hierarchy.cpp
    test_plugin.cpp
    test_hive.cpp
    test_log.cpp
    test_perf.cpp
    test_uid.cpp
    test_string.cpp
    test_string_view.cpp
    test_shared_ptr.cpp
    test_vector.cpp
    test_c_api.cpp
    test_object_ref.cpp
    test_variant.cpp
    test_thread_context.cpp
    test_importer.cpp
    test_resource_store.cpp
    test_js.cpp
)

target_link_libraries(tests PRIVATE velk velk_c GTest::gtest_main)
target_include_directories(tests PRIVATE
    $<TARGET_PROPERTY:velk_animator,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:velk_importer,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:velk_js,INTERFACE_INCLUDE_DIRECTORIES>
)

# Pass the test plugin DLL path as a compile definition so tests can find it.
add_subdirectory(test_plugin_dll)
target_compile_definitions(tests PRIVATE
    TEST_PLUGIN_DLL_PATH="$<TARGET_FILE:test_plugin_dll>"
    TEST_ANIMATOR_DLL_PATH="$<TARGET_FILE:velk_animator>"
    TEST_IMPORTER_DLL_PATH="$<TARGET_FILE:velk_importer>"
    TEST_JS_DLL_PATH="$<TARGET_FILE:velk_js>"
)

# Stage test_plugin_dll under <tests_exe_dir>/plugins/ so the
# `plugin:<name>` URI resolver in load_plugin_from_uri can find it via the
# conventional layout that ships with the runtime.
add_custom_command(TARGET tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:tests>/plugins
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:test_plugin_dll>
        $<TARGET_FILE_DIR:tests>/plugins/$<TARGET_FILE_NAME:test_plugin_dll>
    COMMENT "Staging test_plugin_dll under tests/plugins/ for URI load tests"
    VERBATIM
)

include(GoogleTest)
gtest_discover_tests(tests)
