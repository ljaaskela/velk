add_library(velk SHARED
    src/array_property.cpp
    src/array_property.h
    src/binding.cpp
    src/binding.h
    src/dependency_tracker.cpp
    src/dependency_tracker.h
    src/property.cpp
    src/property.h
    src/standalone_property.cpp
    src/standalone_property.h
    src/event.cpp
    src/event.h
    src/function.cpp
    src/function.h
    include/velk/velk_export.h
    include/velk/array_view.h
    include/velk/common.h
    include/velk/memory.h
    include/velk/string_view.h
    include/velk/string.h
    include/velk/vector.h
    include/velk/uid.h
    include/velk/duration.h
    include/velk/thread.h
    src/store.cpp
    src/store.h
    src/object_storage.cpp
    src/object_storage.h
    src/object_ref.cpp
    src/object_ref.h
    src/variant.cpp
    src/variant.h
    src/hierarchy.cpp
    src/hierarchy.h
    src/generic_object.h
    src/future.cpp
    src/future.h
    src/manual_task_pool.cpp
    src/manual_task_pool.h
    src/threaded_task_pool.cpp
    src/threaded_task_pool.h
    src/type_registry.cpp
    src/type_registry.h
    src/plugin_registry.cpp
    src/plugin_registry.h
    src/perf_log.cpp
    src/perf_log.h
    src/velk_instance.cpp
    src/velk_instance.h
    src/resource/resource_store.cpp
    src/resource/resource_store.h
    src/resource/file_protocol.cpp
    src/resource/file_protocol.h
    src/resource/file_resource.cpp
    src/resource/file_resource.h
    src/resource/memory_protocol.cpp
    src/resource/memory_protocol.h
    src/library_handle.h
    src/platform.h
    src/velk.cpp
    include/velk/interface/intf_log.h
    include/velk/interface/intf_perf_log.h
    include/velk/interface/intf_any.h
    include/velk/interface/intf_binding.h
    include/velk/interface/intf_external_any.h
    include/velk/interface/intf_object.h
    include/velk/interface/intf_interface.h
    include/velk/interface/refcnt_ptr.h
    include/velk/interface/intf_array_any.h
    include/velk/interface/intf_array_property.h
    include/velk/interface/intf_function.h
    include/velk/interface/intf_event.h
    include/velk/interface/intf_future.h
    include/velk/interface/intf_task_pool.h
    include/velk/interface/intf_any_extension.h
    include/velk/interface/intf_property.h
    include/velk/interface/intf_object_factory.h
    include/velk/interface/intf_type_registry.h
    include/velk/interface/intf_plugin.h
    include/velk/interface/intf_plugin_registry.h
    include/velk/interface/intf_object_ref.h
    include/velk/interface/intf_variant.h
    include/velk/interface/intf_velk.h
    include/velk/interface/intf_metadata.h
    include/velk/interface/intf_object_storage.h
    include/velk/interface/intf_store.h
    include/velk/interface/intf_hierarchy.h
    include/velk/interface/resource/intf_resource.h
    include/velk/interface/resource/intf_resource_decoder.h
    include/velk/interface/resource/intf_resource_protocol.h
    include/velk/interface/resource/intf_resource_store.h
    include/velk/interface/intf_importer_extension.h
    include/velk/interface/intf_thread_context.h
    include/velk/interface/member_desc.h
    include/velk/interface/types.h
    include/velk/api/any.h
    include/velk/api/binding.h
    include/velk/api/callback.h
    include/velk/api/event.h
    include/velk/api/property.h
    include/velk/api/future.h
    include/velk/api/task_pool.h
    include/velk/api/function.h
    include/velk/api/function_context.h
    include/velk/api/state.h
    include/velk/api/traits.h
    include/velk/api/math_types.h
    include/velk/api/velk.h
    include/velk/api/perf.h
    include/velk/api/attachment.h
    include/velk/api/hierarchy.h
    include/velk/api/thread_context.h
    include/velk/api/object.h
    include/velk/api/object_ref.h
    include/velk/api/store.h
    include/velk/api/variant.h
    include/velk/ext/any.h
    include/velk/ext/any_extension.h
    include/velk/ext/core_object.h
    include/velk/ext/member_traits.h
    include/velk/ext/metadata.h
    include/velk/ext/object.h
    include/velk/ext/event.h
    include/velk/ext/plugin.h
    include/velk/api/hive/hive.h
    include/velk/api/hive/object_hive.h
    include/velk/api/hive/raw_hive.h
    include/velk/ext/interface_dispatch.h
    include/velk/ext/refcounted_dispatch.h
    include/velk/interface/hive/intf_hive.h
    include/velk/interface/hive/intf_hive_store.h
    src/hive/page_allocator.h
    src/hive/object_hive.cpp
    src/hive/object_hive.h
    src/hive/raw_hive.cpp
    src/hive/raw_hive.h
    src/hive/hive_store.cpp
    src/hive/hive_store.h
    )

configure_file(
    include/velk/version.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/include/velk/version.h"
    @ONLY
)

target_include_directories(velk
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/hive
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/resource
)

option(VELK_ENABLE_BLOCK_POOL "Enable thread-local control_block pooling" ON)
option(VELK_ENABLE_PERF "Enable performance instrumentation (VELK_PERF_SCOPE)" ON)

target_compile_definitions(velk PRIVATE
    VELK_EXPORTS
    VELK_ENABLE_BLOCK_POOL=$<BOOL:${VELK_ENABLE_BLOCK_POOL}>
)

target_compile_definitions(velk PUBLIC
    VELK_PERF_ENABLED=$<BOOL:${VELK_ENABLE_PERF}>
)

# No RTTI, no exceptions for the library itself.
# Strip CMake's default /EHsc and /GR to avoid D9025 override warnings.
# These string(REPLACE) calls are directory-scoped and only affect targets in this subdirectory.
if(MSVC)
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    target_compile_options(velk PRIVATE /GR- /EHs-c-)
    target_compile_definitions(velk PRIVATE _HAS_EXCEPTIONS=0)
else()
    target_compile_options(velk PRIVATE -fno-rtti -fno-exceptions)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS velk EXPORT velk-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/velk
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/velk/version.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/velk
)

install(EXPORT velk-targets
    FILE velk-config.cmake
    NAMESPACE velk::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/velk
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/velk-config-version.cmake"
    COMPATIBILITY SameMajorVersion
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/velk-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/velk
)
