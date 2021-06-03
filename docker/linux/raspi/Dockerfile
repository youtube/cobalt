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

FROM cobalt-build-base

ARG raspi_home=/root/raspi-home
ARG raspi_tools=cobalt_raspi_tools.tar.bz2

# Required by the gyp build system.
ENV RASPI_HOME=${raspi_home}

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        g++-multilib \
        bzip2 \
    && apt-get clean autoclean \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && rm -rf /var/lib/{apt,dpkg,cache,log} \
    && echo "Done"

# Get the combined toolchains package.
RUN cd /tmp \
    && curl --silent -O -J \
    "https://storage.googleapis.com/cobalt-static-storage/${raspi_tools}" \
    && mkdir -p ${raspi_home} \
    && cd ${raspi_home} \
    && tar xjvf /tmp/${raspi_tools} \
    && rm /tmp/${raspi_tools}

CMD /code/cobalt/build/gyp_cobalt -v -C ${CONFIG} ${PLATFORM} \
    && ninja -v -j ${NINJA_PARALLEL} -C ${OUTDIR}/${PLATFORM}_${CONFIG} ${TARGET:-cobalt_deploy}
