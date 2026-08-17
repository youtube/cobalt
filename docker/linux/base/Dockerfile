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
FROM ${BASE_OS:-marketplace.gcr.io/google/debian11}:${BASE_OS_TAG:-latest}

COPY base/clean-after-apt.sh /opt/clean-after-apt.sh

ENV PYTHONUNBUFFERED 1

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

ARG GCLOUD_VERSION=465.0.0
ENV PATH="/opt/google-cloud-sdk/bin:${PATH}"
RUN cd /tmp \
    && curl -sSLf --connect-timeout 5 -O "https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/google-cloud-cli-${GCLOUD_VERSION}-linux-x86_64.tar.gz" \
    && tar xf "google-cloud-cli-${GCLOUD_VERSION}-linux-x86_64.tar.gz" -C /opt \
    && rm "google-cloud-cli-${GCLOUD_VERSION}-linux-x86_64.tar.gz" \
    && gcloud --version

CMD ["/usr/bin/python3","--version"]
