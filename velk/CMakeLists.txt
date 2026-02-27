add_library(velk SHARED
    src/property.cpp
    src/property.h
    src/function.cpp
    src/function.h
    include/velk/velk_export.h
    include/velk/array_view.h
    include/velk/common.h
    include/velk/memory.h
    include/velk/string_view.h
    include/velk/uid.h
    src/metadata_container.cpp
    src/metadata_container.h
    src/future.cpp
    src/future.h
    src/type_registry.cpp
    src/type_registry.h
    src/plugin_registry.cpp
    src/plugin_registry.h
    src/velk_instance.cpp
    src/velk_instance.h
    src/library_handle.h
    src/platform.h
    src/velk.cpp
    include/velk/interface/intf_log.h
    include/velk/interface/intf_any.h
    include/velk/interface/intf_external_any.h
    include/velk/interface/intf_object.h
    include/velk/interface/intf_interface.h
    include/velk/interface/refcnt_ptr.h
    include/velk/interface/intf_function.h
    include/velk/interface/intf_event.h
    include/velk/interface/intf_future.h
    include/velk/interface/intf_property.h
    include/velk/interface/intf_object_factory.h
    include/velk/interface/intf_type_registry.h
    include/velk/interface/intf_plugin.h
    include/velk/interface/intf_plugin_registry.h
    include/velk/interface/intf_velk.h
    include/velk/interface/intf_metadata.h
    include/velk/interface/member_desc.h
    include/velk/interface/types.h
    include/velk/api/any.h
    include/velk/api/callback.h
    include/velk/api/event.h
    include/velk/api/property.h
    include/velk/api/future.h
    include/velk/api/function.h
    include/velk/api/function_context.h
    include/velk/api/state.h
    include/velk/api/traits.h
    include/velk/api/velk.h
    include/velk/ext/any.h
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

target_include_directories(velk
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/hive
)

option(VELK_ENABLE_BLOCK_POOL "Enable thread-local control_block pooling" ON)

target_compile_definitions(velk PRIVATE
    VELK_EXPORTS
    VELK_ENABLE_BLOCK_POOL=$<BOOL:${VELK_ENABLE_BLOCK_POOL}>
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
