# Use the official Ubuntu image as the base
FROM debian:12

# TODO(b/372303096): Move to dedicated unittest image.
RUN echo deb http://deb.debian.org/debian-debug \
    $(. /etc/os-release && echo "${VERSION_CODENAME}")-debug main \
    >> /etc/apt/sources.list

# TODO(b/372303096): Move to dedicated unittest image.
# The following list of debs were added from the pre-chrobalt unittest image:
# * libatomic1 \
# * libavcodec-dev \
# * libavformat-dev \
# * libgles2-mesa \
# * libegl1-mesa \
# * libgl1-mesa-dri-dbgsym \
# * libx11-6 \
# * libxi-dev \
# * libxrender1 \
# * procps \
# * libasound2-dbgsym \
# * libegl1-dbgsym \
# * libglapi-mesa-dbgsym \
# * libgles2-dbgsym \
# * libglx-mesa0-dbgsym \
# * libglx0-dbgsym \

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
