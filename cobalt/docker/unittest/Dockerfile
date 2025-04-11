ARG BASE_IMAGE_SHA=sha256:4abf773f2a570e6873259c4e3ba16de6c6268fb571fd46ec80be7c67822823b3

FROM debian:12@${BASE_IMAGE_SHA}

# Install any necessary dependencies
RUN apt-get update && apt-get install -y \
    jq \
    libasound2 \
    libdbus-1-3 \
    libegl1 \
    libegl-mesa0 \
    libgbm-dev \
    libglapi-mesa \
    libgles2-mesa \
    libglib2.0-0 \
    libnss3 \
    libxcomposite-dev \
    xvfb \
    && rm -rf /var/lib/apt/lists/*

# This locale satisfies SbSystemGetLocaleIdTest.
ENV LANG en_US.UTF-8
