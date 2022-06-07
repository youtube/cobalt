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

# === Install portable sccache binary
ARG SCCACHE=sccache-v0.3.0-x86_64-unknown-linux-musl.tar.gz
ARG SCCACHE_SHA256=e6cd8485f93d683a49c83796b9986f090901765aa4feb40d191b03ea770311d8
RUN cd /tmp \
    && curl -L -O https://github.com/mozilla/sccache/releases/download/v0.3.0/${SCCACHE} \
    && echo "${SCCACHE_SHA256} *${SCCACHE}" | sha256sum -c - \
    && tar xvzf ${SCCACHE} -C /usr/local/bin --strip-components=1 \
    && chmod +x /usr/local/bin/sccache \
    && rm -rf ${SCCACHE} \
    && sccache --version

# TODO: b/379174067 - Move variables out of container.
ENV IS_CI=1
