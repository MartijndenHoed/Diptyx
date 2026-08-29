cmake_minimum_required(VERSION 3.16.0)
set(PROJECT_VER "1.0.0")  # Avoid git errors
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
get_filename_component(configName "${CMAKE_BINARY_DIR}" NAME)
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/.pio/libdeps/${configName}/esp_littlefs")

project(ereader)