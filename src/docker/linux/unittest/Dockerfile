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

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        libasound2 \
        libegl1-mesa \
        libgl1-mesa-dri \
        libgles2-mesa \
        libx11-6 \
        libxcomposite1 \
        libxrender1 \
        unzip \
        xauth \
        xvfb \
    && /opt/clean-after-apt.sh

WORKDIR /out
# Sets the locale in the environment. This is needed for NPLB unit tests.
ENV LANG en_US.UTF-8

RUN mkdir -p /app_launcher_out

CMD unzip -q /out/app_launcher -d /app_launcher_out && \
    xvfb-run --server-args="-screen 0 1920x1080x24 +render +extension GLX -noreset" \
    python /app_launcher_out/starboard/tools/testing/test_runner.py --run \
           -o /out --platform $PLATFORM --config $CONFIG
