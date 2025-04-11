ARG BASE_IMAGE_SHA=sha256:be15f84c44c4ee68b4f187128f0278df1b0f424c04fb5f08b098967d5b896388

FROM marketplace.gcr.io/google/debian12@${BASE_IMAGE_SHA}

# Install any necessary dependencies
RUN apt-get update && apt-get install -y \
    jq \
    libasound2 \
    libgles2-mesa-dev \
    libglapi-mesa \
    libegl-mesa0 \
    libdbus-1-3 \
    libgbm-dev \
    libnss3 \
    libxcomposite-dev \
    xvfb \
    && rm -rf /var/lib/apt/lists/*

# This locale satisfies SbSystemGetLocaleIdTest.
ENV LANG en_US.UTF-8
