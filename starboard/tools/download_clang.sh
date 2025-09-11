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

readonly CLANG_VERSION="${1:-17-init-8029-g27f27d15-3}"
readonly CLANG_DIR="${HOME}/starboard-toolchains/x86_64-linux-gnu-clang-chromium-${CLANG_VERSION}"
readonly BASE_URL="https://commondatastorage.googleapis.com/chromium-browser-clang/Linux_x64"

if [ -d "${CLANG_DIR}" ]; then
  echo "already have Clang ${CLANG_VERSION}; not downloading"
  exit 0
fi

mkdir -p "${CLANG_DIR}"
curl -s "${BASE_URL}/clang-llvmorg-${CLANG_VERSION}.tgz" | tar xzC "${CLANG_DIR}"
curl -s "${BASE_URL}/llvm-code-coverage-llvmorg-${CLANG_VERSION}.tgz" | tar xzC "${CLANG_DIR}"
echo "${CLANG_VERSION}" > "${CLANG_DIR}/cr_build_revision"
echo "downloaded to ${CLANG_DIR}"
