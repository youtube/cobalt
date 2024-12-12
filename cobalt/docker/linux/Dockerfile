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
    libatomic1 \
    libavcodec-dev \
    libavformat-dev \
    libgles2-mesa \
    libegl1-mesa \
    libgl1-mesa-dri-dbgsym \
    libx11-6 \
    libxi-dev \
    libxrender1 \
    procps \
    libasound2-dbgsym \
    libegl1-dbgsym \
    libglapi-mesa-dbgsym \
    libgles2-dbgsym \
    libglx-mesa0-dbgsym \
    libglx0-dbgsym \
    && rm -rf /var/lib/apt/lists/*

ADD files/sccache /usr/local/bin

# TODO: b/379174067 - Move variables out of container.
ENV SCCACHE_GCS_BUCKET=githubactions-chrome-sccache
ENV SCCACHE_GCS_RW_MODE=READ_WRITE
ENV SCCACHE_GCS_KEY_PATH=/root/key.json
ENV SCCACHE_GCS_SERVICE_ACCOUNT=github-actions-bucket-access@cobalt-demo-330821.iam.gserviceaccount.com
ENV IS_CI=1
