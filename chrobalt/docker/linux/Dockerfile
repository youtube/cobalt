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
    && rm -rf /var/lib/apt/lists/*

ADD files/sccache /usr/local/bin

# === Install pinned host Clang toolchain for all Linux-hosted builds
ARG CLANG_VER

ARG TC_ROOT=${HOME}/starboard-toolchains/
ARG TC_HOME=${TC_ROOT}/x86_64-linux-gnu-clang-chromium-${CLANG_VER}
ARG BASE_URL=https://commondatastorage.googleapis.com/chromium-browser-clang

RUN cd /tmp \
    && mkdir -p ${TC_HOME} \
    && curl --silent -O -J \
        ${BASE_URL}/Linux_x64/clang-${CLANG_VER}.tgz \
    && tar xf clang-${CLANG_VER}.tgz -C ${TC_HOME} \
    && echo ${CLANG_VER} >> ${TC_HOME}/cr_build_revision \
    && rm clang-${CLANG_VER}.tgz

ENV SCCACHE_GCS_BUCKET=githubactions-chrome-sccache
ENV SCCACHE_GCS_RW_MODE=READ_WRITE
ENV SCCACHE_GCS_KEY_PATH=/root/key.json
ENV SCCACHE_GCS_SERVICE_ACCOUNT=github-actions-bucket-access@cobalt-demo-330821.iam.gserviceaccount.com
