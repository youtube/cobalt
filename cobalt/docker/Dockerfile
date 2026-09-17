# Copyright 2025 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# --- Stage 1: Build Git >= 2.43 from release source ---
FROM marketplace.gcr.io/google/debian12@sha256:be15f84c44c4ee68b4f187128f0278df1b0f424c04fb5f08b098967d5b896388 AS git-builder

# Git >= 2.43 is required for --end-of-options in gclient sync (b/562551706)
ARG GIT_VERSION=2.46.2
ARG GIT_SHA256=65c5689fd44f1d09de7fd8c44de7fef074ddd69dda8b8503d44afb91495ecbce

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    curl \
    libcurl4-openssl-dev \
    libexpat1-dev \
    libssl-dev \
    zlib1g-dev \
  && curl -fsSL "https://www.kernel.org/pub/software/scm/git/git-${GIT_VERSION}.tar.gz" -o git.tar.gz \
  && echo "${GIT_SHA256}  git.tar.gz" | sha256sum --check \
  && tar -zxf git.tar.gz \
  && cd "git-${GIT_VERSION}" \
  && make -j$(nproc) prefix=/usr/local DESTDIR=/opt/git NO_TCLTK=1 NO_GETTEXT=1 all install

# --- Stage 2: Shared Base Container ---
FROM marketplace.gcr.io/google/debian12@sha256:be15f84c44c4ee68b4f187128f0278df1b0f424c04fb5f08b098967d5b896388 AS base

# Copy compiled Git binaries
COPY --from=git-builder /opt/git/usr/local /usr/local

# Install common base runtime utilities & UTF-8 locale in a single layer
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    python3-dev \
    xz-utils \
    zstd \
    jq \
    pkgconf \
    libglib2.0-0 \
    libnss3 \
    libasound2 \
    libcurl4 \
    libexpat1 \
    locales \
  && sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen \
  && locale-gen \
  && rm -rf /var/lib/apt/lists/*

# This locale satisfies SbSystemGetLocaleIdTest and standard UTF-8 environments
ENV LANG=en_US.UTF-8

# Install Google Cloud SDK
ARG GCLOUD=google-cloud-cli-linux-x86_64.tar.gz
RUN cd /tmp \
  && curl -fsSL -O "https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/${GCLOUD}" \
  && tar xf "${GCLOUD}" -C /usr/local --strip-components=1 google-cloud-sdk/bin google-cloud-sdk/lib \
  && rm "${GCLOUD}" \
  && gcloud --version

# --- Stage 3: Linux Build Image ---
FROM base AS linux

# Install dependencies required for Linux builds and V8 snapshot binary
RUN apt-get update && apt-get install -y --no-install-recommends \
    gperf \
    libdbus-1-3 \
    libxcomposite1 \
    libxdamage1 \
    libxext6 \
    libxfixes3 \
    libxrandr2 \
    libxtst6 \
    libgbm1 \
    libxkbcommon0 \
    libpango-1.0-0 \
    gcc-multilib \
    bzip2 \
    fuse-overlayfs \
  && rm -rf /var/lib/apt/lists/*

# --- Stage 4: RDK Build Image ---
FROM linux AS rdk

# Install dependencies required for RDK host builds
RUN apt-get update && apt-get install -y --no-install-recommends \
    g++-multilib \
    libglib2.0-dev \
    libtool \
    libxml2 \
    pkg-config \
  && rm -rf /var/lib/apt/lists/*

ARG rdk_home=/root/rdk-home
ARG rdk_toolchain=20250521_rdk-glibc-x86_64-arm-toolchain-2.0.sh

# Required by the gyp build system.
ENV RDK_HOME=${rdk_home}

# Get the toolchain package.
RUN cd /tmp \
  && curl --silent -O -J \
  "https://storage.googleapis.com/cobalt-static-storage-public/${rdk_toolchain}" \
  && sh ${rdk_toolchain} -y -d ${rdk_home} \
  && rm /tmp/${rdk_toolchain}

# --- Stage 5: Unit Test Image ---
FROM base AS unittest

# Install any necessary dependencies for unit tests and virtual display
RUN apt-get update && apt-get install -y --no-install-recommends \
    dbus-x11 \
    ffmpeg \
    libatomic1 \
    libdbus-1-3 \
    libegl1 \
    libegl1-mesa \
    libegl-mesa0 \
    libgbm-dev \
    libglapi-mesa \
    libgles2-mesa \
    libxcomposite-dev \
    openbox \
    python3 \
    x11-xserver-utils \
    xcompmgr \
    xcvt \
    xserver-xorg-core \
    xserver-xorg-video-dummy \
    xvfb \
  && rm -rf /var/lib/apt/lists/*

# --- Stage 6: Web Test Image ---
FROM base AS webtest

# Install any necessary dependencies for headless web tests
RUN apt-get update && apt-get install -y --no-install-recommends \
    libatk-bridge2.0-0 \
    libatk1.0-0 \
    libgles2-mesa-dev \
    libpangocairo-1.0-0 \
    libxdamage1 \
    libxkbcommon-x11-0 \
    xvfb \
    x11-utils \
    fuse-overlayfs \
  && rm -rf /var/lib/apt/lists/*
