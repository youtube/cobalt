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
    && apt install -qqy --no-install-recommends clang-3.9 \
    && /opt/clean-after-apt.sh

CMD /code/cobalt/build/gyp_cobalt -v -C ${CONFIG} ${PLATFORM} && \
    ccache -z && \
    ninja -v -j ${NINJA_PARALLEL} -C ${OUTDIR}/${PLATFORM}_${CONFIG} ${TARGET:-cobalt_deploy} && \
    ccache -s
