ARG BASE_IMAGE_SHA=sha256:4abf773f2a570e6873259c4e3ba16de6c6268fb571fd46ec80be7c67822823b3

FROM debian:12@${BASE_IMAGE_SHA}

# Install any necessary dependencies
RUN apt-get update && apt-get install -y \
    python3-dev \
    git \
    xz-utils \
    curl \
    jq \
    libatk1.0-0 \
    libatk-bridge2.0-0 \
    libasound2 \
    libgles2-mesa-dev \
    libglapi-mesa \
    libegl-mesa0 \
    libdbus-1-3 \
    libgbm-dev \
    libglib2.0-0 \
    libnss3 \
    libpangocairo-1.0-0 \
    libxcomposite-dev \
    libxdamage1 \
    libxkbcommon-x11-0 \
    xvfb \
    x11-utils \
    && rm -rf /var/lib/apt/lists/*

# This locale satisfies SbSystemGetLocaleIdTest.
ENV LANG en_US.UTF-8
