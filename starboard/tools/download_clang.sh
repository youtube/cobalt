#!/bin/sh

# Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

set -e

CLANG_VERSION="${1:-365097-f7e52fbd-8}"
TOOLCHAIN_ROOT="${HOME}/starboard-toolchains/"
TOOLCHAIN_HOME="${TOOLCHAIN_ROOT}/x86_64-linux-gnu-clang-chromium-${CLANG_VERSION}"

if [ -d "${TOOLCHAIN_HOME}" ]; then
  echo "Clang is already downloaded, exiting."
  exit 0
fi

BASE_URL="https://commondatastorage.googleapis.com/chromium-browser-clang"

cd /tmp
mkdir -p ${TOOLCHAIN_HOME}

# Download and extract clang.
curl --silent -O -J ${BASE_URL}/Linux_x64/clang-${CLANG_VERSION}.tgz
tar xf clang-${CLANG_VERSION}.tgz -C ${TOOLCHAIN_HOME}
rm clang-${CLANG_VERSION}.tgz

# Download and extract llvm coverage tools.
curl --silent -O -J ${BASE_URL}/Linux_x64/llvm-code-coverage-${CLANG_VERSION}.tgz
tar xf llvm-code-coverage-${CLANG_VERSION}.tgz -C ${TOOLCHAIN_HOME}
rm llvm-code-coverage-${CLANG_VERSION}.tgz

echo ${CLANG_VERSION} >> ${TOOLCHAIN_HOME}/cr_build_revision

echo "Downloaded clang."
