# Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
FROM ${FROM_IMAGE:-cobalt-build-android}

ENV PATH="/root/starboard-toolchains/AndroidSdk/cmdline-tools/1.0/bin/:${PATH}"

RUN yes | sdkmanager --licenses

RUN sdkmanager --update && \
    sdkmanager "system-images;android-30;google_apis;x86"

RUN avdmanager create avd \
    -n testAVD \
    -k "system-images;android-30;google_apis;x86" \
    --device "pixel"

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        libpulse0 \
    && /opt/clean-after-apt.sh

CMD /root/starboard-toolchains/AndroidSdk/emulator/emulator \
    -avd testAVD
