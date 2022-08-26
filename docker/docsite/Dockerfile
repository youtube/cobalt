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

ARG FROM_IMAGE
FROM ${FROM_IMAGE:-cobalt-build-base}

ARG USER
ARG UID
ARG GID

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        build-essential \
        bundler \
        doxygen \
        git \
        python-pygments \
        ruby2.5-dev \
        ruby-execjs \
    && apt-get clean autoclean \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && rm -rf /var/lib/{apt,dpkg,cache,log}

COPY Gemfile /app/Gemfile

RUN bundle install --gemfile=/app/Gemfile

# We create and use a non-root user explicitly so that the generated and
# modified files maintain the same permissions as the user that launched the
# Docker container.
RUN addgroup --group --gid "${GID}" defaultgroup \
    && adduser --disabled-password --gecos '' --uid "${UID}" --gid "${GID}" defaultuser

# The new user must be able to execute gn.
RUN chmod a+x /usr/local/bin/gn

# Create an out directory with a descriptive name for gn.
RUN mkdir /project_out_dir \
    && chown ${USER:-defaultuser}:defaultgroup /project_out_dir

USER ${USER:-defaultuser}

CMD /code/third_party/repo-publishing-toolkit-local/preview-site.sh run
