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

ARG BASE_IMAGE=base
FROM ${BASE_IMAGE}

# Install dependencies required for Linux and RDK host builds
RUN apt-get update && apt-get install -y --no-install-recommends \
    bzip2 \
    fuse-overlayfs \
    g++-multilib \
    gcc-multilib \
    gperf \
    libdbus-1-3 \
    libgbm1 \
    libglib2.0-dev \
    libtool \
    libxcomposite1 \
    libxdamage1 \
    libxext6 \
    libxfixes3 \
    libxkbcommon0 \
    libxml2 \
    libpango-1.0-0 \
    libxrandr2 \
    libxtst6 \
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
