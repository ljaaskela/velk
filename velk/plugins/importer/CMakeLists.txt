add_library(velk_importer SHARED
    src/importer_plugin.h
    src/importer_plugin.cpp
    src/json_import_data.h
    src/json_importer.h
    src/json_importer.cpp
    src/json_value_codec.cpp
    src/json_writer.h
    src/json_writer.cpp
    src/tracking_importer.h
    src/tracking_importer.cpp
    include/velk/plugins/importer/api/importer.h
    include/velk/plugins/importer/interface/intf_importer_plugin.h
    include/velk/plugins/importer/plugin.h
)

target_link_libraries(velk_importer PRIVATE velk)

target_include_directories(velk_importer
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(velk_importer PRIVATE VELK_EXPORTS)
