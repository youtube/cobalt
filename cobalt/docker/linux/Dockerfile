# Use the official Ubuntu image as the base
FROM debian:12

# Install any necessary dependencies
# NOTE: From libxcomposite down, these are minimal requirements to run a
# V8 snapshot binary, on X11/GTK builds
#     gcc-multilib  for cross-builds of v8/32bit arm
RUN apt-get update && apt-get install -y \
    curl python3-dev git \
    xz-utils \
    pkgconf \
    libglib2.0-0 \
    libnss3 \
    gperf \
    libdbus-1-3 \
    libatk1.0-0 \
    libatk-bridge2.0-0 \
    libcairo2-dev \
    libxcomposite1 \
    libxdamage1 \
    libxext6 \
    libxfixes3 \
    libxrandr2 \
    libxtst6 \
    libgbm1 \
    libasound2 \
    libxkbcommon0 \
    libpango-1.0-0 \
    gcc-multilib \
    bzip2 \
    python3-certifi \
    xauth \
    xvfb \
    jq \
    && rm -rf /var/lib/apt/lists/*

ARG GCLOUD=google-cloud-cli-linux-x86_64.tar.gz
RUN cd /tmp \
    && curl -L -O "https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/${GCLOUD}" \
    && tar xf "${GCLOUD}" -C /usr/local --strip-components=1 google-cloud-sdk/bin google-cloud-sdk/lib \
    && rm "${GCLOUD}" \
    && gcloud --version
