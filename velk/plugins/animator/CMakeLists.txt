add_library(velk_animator SHARED
    src/animation.h
    src/animation.cpp
    src/animator.h
    src/animator.cpp
    src/animator_plugin.h
    src/animator_plugin.cpp
    include/velk/plugins/animator/interface/intf_animation.h
    include/velk/plugins/animator/interface/intf_animator.h
    include/velk/plugins/animator/interface/intf_animator_plugin.h
    include/velk/plugins/animator/animator.h
    include/velk/plugins/animator/plugin.h
    include/velk/plugins/animator/easing.h
    include/velk/plugins/animator/interpolator_traits.h
    include/velk/plugins/animator/api/animation.h
    include/velk/plugins/animator/api/animator.h
)

target_link_libraries(velk_animator PRIVATE velk)

target_include_directories(velk_animator
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(velk_animator PRIVATE VELK_EXPORTS)
