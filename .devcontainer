# Use the official Microsoft Ubuntu base image
FROM mcr.microsoft.com/devcontainers/base:ubuntu-22.04

ARG USERNAME=e

# Install system prerequisites
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    git gcc g++ pkg-config cmake curl libssl-dev libdbus-1-dev \
    libglib2.0-dev libavahi-client-dev ninja-build python3-venv python3-dev \
    python3.11 python3.11-venv python3.11-dev \
    python3-pip unzip libgirepository1.0-dev libcairo2-dev libreadline-dev \
    libevent-dev default-jre \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Enforce Python 3.11 as default python3 alias
RUN mkdir -p /home/${USERNAME}/.local/bin \
    && ln -sf /usr/bin/python3.11 /home/${USERNAME}/.local/bin/python3 \
    && chown -R ${USERNAME}:${USERNAME} /home/${USERNAME}/.local

ENV PATH=/home/${USERNAME}/.local/bin:$PATH

USER ${USERNAME}
WORKDIR /home/${USERNAME}

# Install ESP-IDF (v5.4.1)
RUN mkdir -p ~/esp && cd ~/esp \
    && git clone --branch v5.4.1 --recursive https://github.com/espressif/esp-idf.git \
    && cd esp-idf \
    && ./install.sh

# Install ESP-Matter (release/v1.5)
RUN cd ~/esp \
    && git clone --branch release/v1.5 --depth 1 https://github.com/espressif/esp-matter.git \
    && cd esp-matter \
    && git submodule update --init --depth 1 \
    && cd connectedhomeip/connectedhomeip \
    && ./scripts/checkout_submodules.py --platform esp32 linux --shallow

# Build Matter toolchain
RUN cd ~/esp/esp-matter \
    && ./install.sh

# Add environment aliases to bashrc
RUN echo "alias load-matter='export PATH=~/.local/bin:\$PATH && . ~/esp/esp-idf/export.sh && . ~/esp/esp-matter/export.sh'" >> ~/.bashrc
