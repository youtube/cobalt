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
        libatomic1 \
        libavcodec-dev \
        libavformat-dev \
        libavresample-dev \
        libavutil-dev \
        libegl1-mesa-dev \
        libgl1-mesa-glx \
        libglapi-mesa \
        libgles2-mesa \
        libglu1-mesa \
        libwayland-egl1-mesa \
        mesa-common-dev \
        mesa-utils   \
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

RUN python3 -m pip install "selenium==3.141.0" "Brotli==1.0.9"

WORKDIR /out
# Sets the locale in the environment. This is needed for NPLB unit tests.
ENV LANG en_US.UTF-8

RUN mkdir -p /app_launcher_out

ENV PYTHONPATH "/app_launcher_out"

COPY unittest/unittest_docker_launcher.py /unittest_docker_launcher.py

# Without arguments, it assumes no sharding and runs all tests in the container.
CMD python3 /unittest_docker_launcher.py
