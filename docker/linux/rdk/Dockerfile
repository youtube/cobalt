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

FROM cobalt-build-base

ARG rdk_home=/root/rdk-home
ARG rdk_toolchain=20250521_rdk-glibc-x86_64-arm-toolchain-2.0.sh

# Required by the gyp build system.
ENV RDK_HOME=${rdk_home}

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        g++-multilib \
        bzip2 \
        libxml2 \
        binutils-aarch64-linux-gnu \
        binutils-arm-linux-gnueabi \
        libglib2.0-dev \
        pkg-config \
        libtool \
    && /opt/clean-after-apt.sh

# Get the toolchain package.
RUN cd /tmp \
    && curl --silent -O -J \
    "https://storage.googleapis.com/cobalt-static-storage-public/${rdk_toolchain}" \
    && sh ${rdk_toolchain} -y -d ${rdk_home} \
    && rm /tmp/${rdk_toolchain}

# TODO(b/402053134): for now we only build evergreen targets, once modular builds are working this needs to updated to build modular by default
CMD gn gen ${OUTDIR}/${PLATFORM}${SB_API_VERSION:+-sbversion-$SB_API_VERSION}_${CONFIG:-debug} --args="target_platform=\"${PLATFORM}\" build_type=\"${CONFIG:-debug}\" target_cpu=\"arm\" target_os=\"linux\" is_clang=false using_old_compiler=true rdk_build_with_yocto=false rdk_starboard_root=\"//starboard/contrib/rdk/src\" rdk_enable_securityagent=false ${SB_API_VERSION:+sb_api_version=$SB_API_VERSION}" && \
    ninja -v -j ${NINJA_PARALLEL} -C out/${PLATFORM}_${CONFIG:-debug} loader_app_install loader_app_bin native_target/crashpad_handler elf_loader_sandbox_install elf_loader_sandbox_bin
