ARG BASE_IMAGE_SHA=sha256:be15f84c44c4ee68b4f187128f0278df1b0f424c04fb5f08b098967d5b896388

FROM marketplace.gcr.io/google/debian12@${BASE_IMAGE_SHA}

# Install any necessary dependencies
RUN apt-get update && apt-get install -y \
    curl \
    python3-dev \
    git \
    xz-utils \
    zstd \
    jq \
    libasound2 \
    libatk-bridge2.0-0 \
    libatk1.0-0 \
    libgles2-mesa-dev \
    libglib2.0-0 \
    libnss3 \
    libpangocairo-1.0-0 \
    libxdamage1 \
    libxkbcommon-x11-0 \
    xvfb \
    x11-utils \
    && rm -rf /var/lib/apt/lists/*

# This locale satisfies SbSystemGetLocaleIdTest.
ENV LANG en_US.UTF-8
