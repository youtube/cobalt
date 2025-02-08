# Use the official Ubuntu image as the base
FROM debian:12

# Install any necessary dependencies
RUN apt-get update && apt-get install -y \
    curl python3-dev git \
    xz-utils \
    pkgconf \
  #  libglib2.0-0 \
   # libnss3 \
    gperf \
   # libdbus-1-3 \
   # gcc-multilib \
    bzip2 \
   # python3-certifi \
    xauth \
    xvfb \
    jq \
    && rm -rf /var/lib/apt/lists/*
