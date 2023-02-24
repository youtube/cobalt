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

ARG FROM_IMAGE
FROM ${FROM_IMAGE:-cobalt-base}

ARG HOME=/root

# === Install common build tools, required by all platforms
RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        binutils \
        bison \
        ninja-build \
        pkgconf \
        unzip \
        yasm \
    && /opt/clean-after-apt.sh

# === Get Nodejs pinned LTS version via NVM
ARG NVM_SHA256SUM="f068e17dacb88f73302790cc076956c7a0d459ce9b01df842ff3e75744f9e2fe /tmp/install.sh"
ARG NVM_URL="https://cobalt.googlesource.com/third_party/nvm/+/refs/tags/v0.35.3/install.sh?format=TEXT"
ENV NVM_DIR=${HOME}/.nvm
ENV NODE_VERSION 12.17.0

RUN curl --silent -o- ${NVM_URL} \
  | base64 -d > /tmp/install.sh \
   && echo ${NVM_SHA256SUM} | sha256sum --check \
   && . /tmp/install.sh

# === Pinned Node version to v16, as this is latest one that will work on
# legacy Debian systems
ARG LTS_VERSION=v16
RUN . $NVM_DIR/nvm.sh \
   && nvm install --lts ${LTS_VERSION} \
   && nvm alias default ${LTS_VERSION}/* \
   && nvm use default

ENV NODE_PATH $NVM_DIR/v$NODE_VERSION/lib/node_modules
ENV PATH $NVM_DIR/versions/node/v$NODE_VERSION/bin:$PATH

# === Get GN via CIPD
ARG GN_SHA256SUM="af7b2dcb3905bca56655e12131b365f1cba8e159db80d2022330c4f522fab2ef  /tmp/gn.zip"
ARG GN_HASH=r3styzkFvKVmVeEhMbNl8cuo4VnbgNICIzDE9SL6su8C

RUN curl --location --silent --output /tmp/gn.zip \
    "https://chrome-infra-packages.appspot.com/dl/gn/gn/linux-amd64/+/${GN_HASH}" \
    && echo ${GN_SHA256SUM} | sha256sum --check \
    && unzip /tmp/gn.zip -d /usr/local/bin \
    && rm /tmp/gn.zip

# === Configure common env vars
ENV OUTDIR=out \
    NINJA_STATUS="[%e sec | %f/%t %u remaining | %c/sec | j%r] " \
    NINJA_PARALLEL=4 \
    IS_CI=0 \
    CCACHE_DIR=${HOME}/ccache \
    CCACHE_MAXSIZE=30G

# === Set up ccache
RUN cd /tmp && mkdir ${HOME}/ccache

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

WORKDIR /code
CMD ["/usr/bin/python","--version"]
