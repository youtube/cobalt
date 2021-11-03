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

RUN apt update -qqy \
    # Install the |add-apt-repository| command for convenience.
    && apt install -qqy software-properties-common \
    # Add zesty archive repository, and remove universe repository.
    && echo "deb http://old-releases.ubuntu.com/ubuntu zesty main" \
      | tee /etc/apt/sources.list.d/zesty.list \
    && apt-add-repository -r universe \
    # Install gcc 6.3 from zesty repo.
    && apt update -qqy \
    && apt install -qqy gcc-6 g++-6 \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-6 1 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-6 1 \
    # Remove zesty repo, and restore universe repo.
    # Also remove unnecessary software-properties-common.
    && rm /etc/apt/sources.list.d/zesty.list \
    && apt-add-repository universe \
    && apt purge -qqy software-properties-common \
    && apt autoremove -qqy --purge \
    && /opt/clean-after-apt.sh

CMD /code/cobalt/build/gyp_cobalt -v -C ${CONFIG} ${PLATFORM} && \
    ccache -z && \
    ninja -v -j ${NINJA_PARALLEL} -C ${OUTDIR}/${PLATFORM}_${CONFIG} ${TARGET:-cobalt_deploy} && \
    ccache -s
