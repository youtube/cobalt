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

ARG gn_hash=vC0rxqiqGTD3ls9KJHrgJoWP2OBiPk_QEO_xbDItKYoC

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
ENV NVM_DIR /root/.nvm
ENV NODE_VERSION 12.17.0

RUN curl --silent -o- ${NVM_URL} \
  | base64 -d > /tmp/install.sh \
   && echo ${NVM_SHA256SUM} | sha256sum --check \
   && . /tmp/install.sh

RUN . $NVM_DIR/nvm.sh \
   && nvm install --lts \
   && nvm alias default lts/* \
   && nvm use default

ENV NODE_PATH $NVM_DIR/v$NODE_VERSION/lib/node_modules
ENV PATH $NVM_DIR/versions/node/v$NODE_VERSION/bin:$PATH

# === Get GN via CIPD
RUN curl --location --silent --output /tmp/gn.zip \
    "https://chrome-infra-packages.appspot.com/dl/gn/gn/linux-amd64/+/${gn_hash}" \
    && unzip /tmp/gn.zip -d /usr/local/bin \
    && rm /tmp/gn.zip

# === Configure common env vars
ENV OUTDIR=out \
    NINJA_STATUS="[%f/%t %c/sec] " \
    NINJA_PARALLEL=4 \
    IS_CI=0 \
    CCACHE_DIR=/root/ccache \
    CCACHE_MAXSIZE=30G

# == Set up  ccache
RUN cd /tmp && mkdir /root/ccache

WORKDIR /code
CMD ["/usr/bin/python","--version"]
