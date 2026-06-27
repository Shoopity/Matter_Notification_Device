#!/usr/bin/env bash
set -e

echo "========= Starting Espressif Toolchain Setup ========="

# Create folder to hold tools
mkdir -p ~/esp
cd ~/esp

# 1. ESP-IDF Setup
if [ ! -d "esp-idf" ]; then
    echo "-> Cloning ESP-IDF v5.4.1..."
    git clone --branch v5.4.1 --recursive https://github.com/espressif/esp-idf.git
    cd esp-idf
    ./install.sh
else
    echo "-> ESP-IDF already installed."
fi

# 2. Matter Setup
cd ~/esp
if [ ! -d "esp-matter" ]; then
    echo "-> Cloning ESP-Matter release/v1.5..."
    git clone --branch release/v1.5 --depth 1 https://github.com/espressif/esp-matter.git
    cd esp-matter
    git submodule update --init --depth 1
    
    echo "-> Fetching ConnectedHomeIP dependencies..."
    cd connectedhomeip/connectedhomeip
    ./scripts/checkout_submodules.py --platform esp32 linux --shallow
else
    echo "-> ESP-Matter already installed."
fi

# 3. Install Matter Tools
cd ~/esp/esp-matter
echo "-> Compiling Matter toolchain..."
./install.sh

# 4. Add persistent alias to bashrc if missing
if ! grep -q "alias load-matter" ~/.bashrc; then
    echo "-> Adding load-matter alias to profile..."
    echo "alias load-matter='export PATH=~/.local/bin:\$PATH && . ~/esp/esp-idf/export.sh && . ~/esp/esp-matter/export.sh'" >> ~/.bashrc
fi

# 5. Clear project cache artifacts in the workspace
echo "-> Cleaning up project workspace cache..."
cd /workspaces/Matter_Notification_Device/button_device
rm -rf build sdkconfig sdkconfig.old managed_components dependencies.lock .shadow-plugin-configured

echo "========= Setup Complete! Run 'load-matter' to start dev work ========="
