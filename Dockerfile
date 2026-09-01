# Tasia Viewer - Docker Build Environment
# Based on Ubuntu 22.04 LTS (as recommended by Firestorm build docs)
# Cross-platform: produces Linux x86_64 builds

FROM ubuntu:22.04

LABEL maintainer="Tasia <tasia@easierit.org>"
LABEL description="Docker build environment for Tasia Viewer (Firestorm-based OpenSim viewer)"

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# ------------------------------------------------------------------
# System dependencies
# ------------------------------------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
    # Build tools
    build-essential \
    gcc-11 \
    g++-11 \
    cmake \
    ninja-build \
    # Python
    python3 \
    python3-pip \
    python3-venv \
    # Version control
    git \
    ca-certificates \
    # Graphics & GUI deps
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libxinerama-dev \
    libxrandr-dev \
    libfontconfig-dev \
    libfreetype6-dev \
    libdbus-1-dev \
    # Audio
    libpulse-dev \
    libssl-dev \
    # Utilities
    pkg-config \
    xz-utils \
    zstd \
    curl \
    file \
    && rm -rf /var/lib/apt/lists/*

# Set GCC 11 as default
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 100 \
    && update-alternatives --install /usr/bin/cc  cc  /usr/bin/gcc-11 100 \
    && update-alternatives --install /usr/bin/cxx cxx /usr/bin/g++-11 100

# ------------------------------------------------------------------
# Python + Autobuild
# ------------------------------------------------------------------
RUN python3 -m pip install --upgrade pip setuptools wheel

# Copy requirements first for layer caching
COPY requirements.txt /tmp/tasia-viewer/requirements.txt
RUN pip3 install -r /tmp/tasia-viewer/requirements.txt

# ------------------------------------------------------------------
# Create build user (non-root)
# ------------------------------------------------------------------
RUN useradd -m -s /bin/bash builder && \
    chown -R builder:builder /home/builder

# Switch to builder user
USER builder
WORKDIR /home/builder

# ------------------------------------------------------------------
# Source code will be mounted at runtime:
#   docker run -v /mnt/c/new/tasia-viewer:/home/builder/tasia-viewer ...
# Or clone inside the container at build time:
#   git clone <repo> tasia-viewer
# ------------------------------------------------------------------

# Default command: drop into bash shell ready to build
CMD ["/bin/bash", "--login"]
