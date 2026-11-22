add_library(velk_data SHARED
    src/data_plugin.h
    src/data_plugin.cpp
    src/object_list.h
    src/object_list.cpp
    src/hierarchy_source.h
    src/hierarchy_source.cpp
    include/velk/plugins/data/plugin.h
    include/velk/plugins/data/interface/intf_item_source.h
    include/velk/plugins/data/interface/intf_object_list.h
    include/velk/plugins/data/interface/intf_hierarchy_source.h
    include/velk/plugins/data/api/data.h
)

target_link_libraries(velk_data PRIVATE velk)

target_include_directories(velk_data
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(velk_data PRIVATE VELK_EXPORTS)
