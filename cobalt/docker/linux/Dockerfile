ARG BASE_IMAGE=base
FROM ${BASE_IMAGE}

# Install dependencies required for Linux builds and V8 snapshot binary
RUN apt-get update && apt-get install -y --no-install-recommends \
    gperf \
    libdbus-1-3 \
    libxcomposite1 \
    libxdamage1 \
    libxext6 \
    libxfixes3 \
    libxrandr2 \
    libxtst6 \
    libgbm1 \
    libxkbcommon0 \
    libpango-1.0-0 \
    gcc-multilib \
    bzip2 \
    fuse-overlayfs \
    && rm -rf /var/lib/apt/lists/*
