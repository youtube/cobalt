# Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

FROM cobalt-base

ARG HOME=/root

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        libasound2 \
        libatomic1 \
        libavcodec-dev \
        libavformat-dev \
        libgles2-mesa \
        libegl1-mesa \
        libgl1-mesa-dri \
        libx11-6 \
        libxcomposite1 \
        libxi-dev \
        libxrender1 \
        procps \
        unzip \
        xauth \
        xvfb \
    && /opt/clean-after-apt.sh

COPY ./unittest/requirements.txt /opt/requirements.txt
RUN python3 -m pip install --require-hashes --no-deps -r /opt/requirements.txt

# === Install Clang 16 toolchain coverage tools.
ARG TC_ROOT=${HOME}/starboard-toolchains/
ARG CLANG_16_VER=16-init-17653-g39da55e8-2
ARG CLANG_16_TC_HOME=${TC_ROOT}/x86_64-linux-gnu-clang-chromium-${CLANG_16_VER}
ARG CLANG_16_BASE_URL=https://commondatastorage.googleapis.com/chromium-browser-clang

RUN echo ${CLANG_16_BASE_URL}/Linux_x64/llvm-code-coverage-llvmorg-${CLANG_VERSION}.tgz

RUN cd /tmp \
    && mkdir -p ${CLANG_16_TC_HOME} \
    && curl --silent -O -J \
        ${CLANG_16_BASE_URL}/Linux_x64/llvm-code-coverage-llvmorg-${CLANG_16_VER}.tgz \
    && tar xf llvm-code-coverage-llvmorg-${CLANG_16_VER}.tgz -C ${CLANG_16_TC_HOME} \
    && echo ${CLANG_16_VER} >> ${CLANG_16_TC_HOME}/cr_build_revision \
    && rm llvm-code-coverage-llvmorg-${CLANG_16_VER}.tgz

WORKDIR /out
# Sets the locale in the environment. This is needed for NPLB unit tests.
ENV LANG en_US.UTF-8

RUN mkdir -p /app_launcher_out

ENV PYTHONPATH "/app_launcher_out"

COPY unittest/unittest_docker_launcher.py /unittest_docker_launcher.py

# Without arguments, it assumes no sharding and runs all tests in the container.
CMD python3 /unittest_docker_launcher.py
