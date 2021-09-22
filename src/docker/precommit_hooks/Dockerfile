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

FROM gcr.io/cloud-marketplace-containers/google/debian10:latest

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        git python2 python3 python3-pip \
        python3-setuptools python3-wheel \
        libncurses5 curl unzip \
    && apt-get clean autoclean \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && rm -rf /var/lib/{apt,dpkg,cache,log}

RUN pip3 install "pre-commit<3" "cpplint<2" "yapf<1" "pylint<3"

# === Get GN via CIPD
ARG GN_SHA256SUM="1291d4cf9729b6615c621139be4e9c8bb49b5cc80330e7a9e3e83c583d683f71  /usr/local/bin/gn"
ARG GN_HASH=vC0rxqiqGTD3ls9KJHrgJoWP2OBiPk_QEO_xbDItKYoC

RUN curl --location --silent --output /tmp/gn.zip \
    "https://chrome-infra-packages.appspot.com/dl/gn/gn/linux-amd64/+/${GN_HASH}" \
    && unzip /tmp/gn.zip -d /usr/local/bin \
    && rm /tmp/gn.zip \
    && echo ${GN_SHA256SUM} | sha256sum --check

WORKDIR /code

ENV PRE_COMMIT_COLOR=always
ENV SKIP=test-download-from-gcs-helper,check-bug-in-commit-message,run-py2-tests,check-if-starboard-interface-changed

CMD pre-commit install -t pre-push -t pre-commit && \
    (pre-commit run --from-ref ${FROM_REF} --to-ref ${TO_REF} --hook-stage commit; \
     ret=$?; \
     pre-commit run --from-ref ${FROM_REF} --to-ref ${TO_REF} --hook-stage push; \
     exit $(( $ret || $? )))
