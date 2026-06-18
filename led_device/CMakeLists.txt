cmake_minimum_required(VERSION 3.16)

# 1. Define the Matter repo core paths
set(ESP_MATTER_PATH $ENV{ESP_MATTER_PATH})
set(MATTER_SDK_PATH ${ESP_MATTER_PATH}/connectedhomeip/connectedhomeip)

# 2. Include base ESP-IDF project build support
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 3. Register the exact Matter component directories 
set(EXTRA_COMPONENT_DIRS
    "${ESP_MATTER_PATH}/components"
    "${MATTER_SDK_PATH}/config/esp32/components"
)

# 4. Define the project name (this triggers the compilation mapping)
project(led_device)