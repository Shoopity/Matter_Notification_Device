cmake_minimum_required(VERSION 3.16)

# 1. Define the Matter repo core paths
set(ESP_MATTER_PATH $ENV{ESP_MATTER_PATH})
set(MATTER_SDK_PATH ${ESP_MATTER_PATH}/connectedhomeip/connectedhomeip)

# 2. Include base ESP-IDF project build support
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 3. Include essential Espressif Matter build helper utilities
include(${ESP_MATTER_PATH}/examples/common/cmake_common/components_include.cmake)
include($ENV{ESP_MATTER_DEVICE_PATH}/esp_matter_device.cmake)

# 4. Register the comprehensive Matter component directories 
set(EXTRA_COMPONENT_DIRS
    "${ESP_MATTER_PATH}/examples/common"
    "${MATTER_SDK_PATH}/config/esp32/components"
    "${ESP_MATTER_PATH}/components"
    "${ESP_MATTER_PATH}/device_hal/device"
    ${extra_components_dirs_append}
)

# 5. Enforce the compilation flags required by the Matter SDK
idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17;-Os;-DCHIP_HAVE_CONFIG_H;-Wno-overloaded-virtual" APPEND)

# 6. Define the project name
project(button_device)