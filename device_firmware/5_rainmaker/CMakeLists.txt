cmake_minimum_required(VERSION 3.5)

# Firmware version
set(PROJECT_VER "1.7.0")

add_compile_options(-fdiagnostics-color=always)

# Check RAIMAKER_PATH
if(NOT DEFINED ENV{RAIMAKER_PATH})
    message(FATAL_ERROR "Please set RAIMAKER_PATH to esp-rainmaker repo")
endif()

# Extra components
set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/../components/light_driver
    ${CMAKE_CURRENT_LIST_DIR}/../components/button
    ${CMAKE_CURRENT_LIST_DIR}/../components/app_storage
    ${CMAKE_CURRENT_LIST_DIR}/../components/app_wifi
    $ENV{RAIMAKER_PATH}/components/esp_rainmaker
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(5_rainmaker)