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

FROM ruby:2.7-buster

ARG USER
ARG UID
ARG GID

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        bundler \
        doxygen \
        git \
        nodejs \
        python \
        # Required for GN build.
        python3 \
        python3-requests \
    && apt-get clean autoclean \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && rm -rf /var/lib/{apt,dpkg,cache,log}

COPY Gemfile /app/Gemfile

RUN bundle install --gemfile=/app/Gemfile

# === Get GN via CIPD
ARG GN_SHA256SUM="af7b2dcb3905bca56655e12131b365f1cba8e159db80d2022330c4f522fab2ef  /tmp/gn.zip"
ARG GN_HASH=r3styzkFvKVmVeEhMbNl8cuo4VnbgNICIzDE9SL6su8C

RUN curl --location --silent --output /tmp/gn.zip \
    "https://chrome-infra-packages.appspot.com/dl/gn/gn/linux-amd64/+/${GN_HASH}" \
    && echo ${GN_SHA256SUM} | sha256sum --check \
    && unzip /tmp/gn.zip -d /usr/local/bin \
    && rm /tmp/gn.zip

# We create and use a non-root user explicitly so that the generated and
# modified files maintain the same permissions as the user that launched the
# Docker container.
RUN addgroup --group --gid "${GID}" defaultgroup \
    && adduser --disabled-password --gecos '' --uid "${UID}" --gid "${GID}" ${USER:-defaultuser}

# Allow the new uses to run gn and create a descriptive out directory.
RUN chmod a+x /usr/local/bin/gn
RUN mkdir /project_out_dir \
    && chown ${USER:-defaultuser}:defaultgroup /project_out_dir

USER ${USER:-defaultuser}

CMD /cobalt/third_party/repo-publishing-toolkit-local/preview-site.sh
