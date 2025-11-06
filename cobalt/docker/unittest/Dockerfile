ARG BASE_IMAGE_SHA=sha256:be15f84c44c4ee68b4f187128f0278df1b0f424c04fb5f08b098967d5b896388

FROM marketplace.gcr.io/google/debian12@${BASE_IMAGE_SHA}

# Install any necessary dependencies
RUN apt-get update && apt-get install -y \
    ffmpeg \
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
    xz-utils \
    zstd \
    && rm -rf /var/lib/apt/lists/*

# This locale satisfies SbSystemGetLocaleIdTest.
ENV LANG=en_US.UTF-8
