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

ARG BASE_OS
ARG BASE_OS_TAG
FROM ${BASE_OS:-marketplace.gcr.io/google/debian10}:${BASE_OS_TAG:-latest}

COPY base/clean-after-apt.sh /opt/clean-after-apt.sh

ENV PYTHONUNBUFFERED 1

# Fix the sources list to be able to pull updates from Buster (which is now archived).
RUN sed -i 's|deb.debian.org/debian|archive.debian.org/debian|g' /etc/apt/sources.list
RUN sed -i 's|security.debian.org/debian-security|archive.debian.org/debian-security|g' /etc/apt/sources.list

# === Install common dependencies
RUN apt update -qqy \
    && apt -qqy upgrade \
    && apt install -qqy --no-install-recommends \
        python3-dev \
        python3-pip \
        python3-requests \
        python3-setuptools \
        python3-six \
        git ccache \
        curl xz-utils \
    && /opt/clean-after-apt.sh

CMD ["/usr/bin/python","--version"]
