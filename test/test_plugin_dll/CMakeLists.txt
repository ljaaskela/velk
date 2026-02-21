add_library(test_plugin_dll SHARED test_plugin_dll.cpp)
target_link_libraries(test_plugin_dll PRIVATE velk)
