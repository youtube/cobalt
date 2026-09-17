ARG BASE_IMAGE=base
FROM ${BASE_IMAGE}

# Install any necessary dependencies for headless web tests
RUN apt-get update && apt-get install -y --no-install-recommends \
    libatk-bridge2.0-0 \
    libatk1.0-0 \
    libgles2-mesa-dev \
    libpangocairo-1.0-0 \
    libxdamage1 \
    libxkbcommon-x11-0 \
    xvfb \
    x11-utils \
    fuse-overlayfs \
    && rm -rf /var/lib/apt/lists/*
