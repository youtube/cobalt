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

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        libxml2-dev \
        default-jdk \
        binutils-arm-linux-gnueabi \
        g++-multilib \
    && /opt/clean-after-apt.sh

RUN mkdir -p /root/.android

RUN keytool -genkey -v \
            -keystore /root/.android/debug.keystore \
            -dname "cn=Android Docker, ou=YouTube, o=Google, c=US" \
            -storepass android \
            -alias androiddebugkey \
            -keypass android \
            -keyalg RSA \
            -keysize 2048 \
            -validity 10000

# Install Android SDK and NDK
ARG TOOLS=commandlinetools-linux-6200805_latest.zip
ARG CMDLINE_URL=https://dl.google.com/android/repository/${TOOLS}
ENV ANDROID_SDK_ROOT=/root/starboard-toolchains/AndroidSdk/

RUN cd /tmp \
  && mkdir -p ${ANDROID_SDK_ROOT} \
  && curl --silent -O -J  ${CMDLINE_URL} \
  && unzip ${TOOLS} -d ${ANDROID_SDK_ROOT} \
  && rm ${TOOLS} \
  && echo yes | ${ANDROID_SDK_ROOT}/tools/bin/sdkmanager \
      --sdk_root=${ANDROID_SDK_ROOT} \
    "build-tools;30.0.0" \
    "build-tools;31.0.0" \
    "cmake;3.10.2.4988404" \
    "cmdline-tools;1.0" \
    "extras;android;m2repository" \
    "extras;google;m2repository" \
    "ndk;21.1.6352462" \
    "patcher;v4" \
    "platforms;android-30" \
    "platforms;android-31" \
    "platform-tools" \
  && echo "Android SDK installed"

CMD gn gen ${OUTDIR}/${PLATFORM}_${CONFIG} --args="target_platform=\"${PLATFORM}\" build_type=\"${CONFIG}\" target_os=\"android\" target_cpu=\"${TARGET_CPU}\"" && \
    ninja -v -j ${NINJA_PARALLEL} -C ${OUTDIR}/${PLATFORM}_${CONFIG} ${TARGET:-cobalt_install}
