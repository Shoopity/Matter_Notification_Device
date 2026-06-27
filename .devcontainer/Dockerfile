FROM mcr.microsoft.com/devcontainers/base:ubuntu-22.04

ARG USERNAME=e

# Install core system prerequisites quickly
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    git gcc g++ pkg-config cmake curl libssl-dev libdbus-1-dev \
    libglib2.0-dev libavahi-client-dev ninja-build python3-venv python3-dev \
    python3.11 python3.11-venv python3.11-dev \
    python3-pip unzip libgirepository1.0-dev libcairo2-dev libreadline-dev \
    libevent-dev default-jre \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Force Python 3.11 as the default python3 alias
RUN mkdir -p /home/${USERNAME}/.local/bin \
    && ln -sf /usr/bin/python3.11 /home/${USERNAME}/.local/bin/python3 \
    && chown -R ${USERNAME}:${USERNAME} /home/${USERNAME}/.local

ENV PATH=/home/${USERNAME}/.local/bin:$PATH
