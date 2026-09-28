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

ARG BASE_IMAGE_SHA=sha256:be15f84c44c4ee68b4f187128f0278df1b0f424c04fb5f08b098967d5b896388

FROM marketplace.gcr.io/google/debian12@${BASE_IMAGE_SHA}

# Install Git from release (Git >= 2.43 is required for --end-of-options in gclient sync, b/562551706)
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
    && make -j$(nproc) prefix=/usr/local NO_TCLTK=1 NO_GETTEXT=1 all install \
    && cd .. \
    && rm -rf "git-${GIT_VERSION}" git.tar.gz \
    && apt-get purge -y build-essential libcurl4-openssl-dev libexpat1-dev libssl-dev \
    && apt-get install -y --no-install-recommends libcurl4 libexpat1 \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/*

# Install any necessary dependencies
# NOTE: From libxcomposite down, these are minimal requirements to run a
# V8 snapshot binary, on X11/GTK builds
#     gcc-multilib  for cross-builds of v8/32bit arm
RUN apt-get update && apt-get install -y \
    curl \
    python3-dev \
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
    jq \
    fuse-overlayfs \
    && rm -rf /var/lib/apt/lists/*

ARG GCLOUD=google-cloud-cli-linux-x86_64.tar.gz
RUN cd /tmp \
    && curl -L -O "https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/${GCLOUD}" \
    && tar xf "${GCLOUD}" -C /usr/local --strip-components=1 google-cloud-sdk/bin google-cloud-sdk/lib \
    && rm "${GCLOUD}" \
    && gcloud --version

ARG rdk_home=/root/rdk-home
ARG rdk_toolchain=20250521_rdk-glibc-x86_64-arm-toolchain-2.0.sh

# Required by the gyp build system.
ENV RDK_HOME=${rdk_home}

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        g++-multilib \
        bzip2 \
        libxml2 \
        libglib2.0-dev \
        pkg-config \
        libtool \
    && rm -rf /var/lib/apt/lists/*

# Get the toolchain package.
RUN cd /tmp \
    && curl --silent -O -J \
    "https://storage.googleapis.com/cobalt-static-storage-public/${rdk_toolchain}" \
    && sh ${rdk_toolchain} -y -d ${rdk_home} \
    && rm /tmp/${rdk_toolchain}
