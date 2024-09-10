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

FROM linux-x64x11-bionic

ARG DEBIAN_FRONTEND=noninteractive

# Note: NASM 2.16 is obtained via PPA for ubuntu distributions older than 20.04
RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        clang-3.9 software-properties-common \
    && add-apt-repository -y ppa:git-core/ppa \
    && add-apt-repository -y ppa:savoury1/backports \
    && apt update -qqy \
    && apt install -qqy --no-install-recommends git nasm \
        python3.8 python3.8-dev python3.8-venv \
    && /opt/clean-after-apt.sh

# TODO(b/338650174): Upgrade the base linux distro to avoid backporting python
# 3.8 manually. Additionally, this involves upgrading clang or backporting
# the current version (3.9) to the new distro.
RUN python3.8 -m venv /python38_env
ENV VIRTUAL_ENV /python38_env
ENV PATH /python38_env/bin:$PATH

COPY clang-3-9/requirements.txt /opt/requirements.txt
RUN python3.8 -m pip install --require-hashes --no-deps -r /opt/requirements.txt

CMD gn gen ${OUTDIR}/${PLATFORM}_${CONFIG} --args="target_platform=\"${PLATFORM}\" build_type=\"${CONFIG}\" using_old_compiler=true build_with_separate_cobalt_toolchain=true" && \
    ninja -v -j ${NINJA_PARALLEL} -C ${OUTDIR}/${PLATFORM}_${CONFIG} ${TARGET:-cobalt_install}
