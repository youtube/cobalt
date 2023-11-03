#!/bin/bash
#
# Copyright 2021 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -ex

readonly LIB_DIR="$(cd "$(dirname "$0")" && pwd)"
readonly SRC_DIR="$(cd "$(dirname "$0")/../../source/libvpx" && pwd)"

(cd "${SRC_DIR}" && ./configure \
  --target=x86_64-linux-gcc \
  --disable-examples \
  --disable-tools \
  --disable-docs \
  --disable-unit-tests \
  --enable-vp9-highbitdepth \
  --disable-vp8 \
  --disable-vp9-encoder \
  --enable-postproc \
  --enable-multithread \
  --enable-runtime-cpu-detect \
  --enable-shared \
  --enable-vp9-temporal-denoising \
  --disable-webm-io \
)

make -C "${SRC_DIR}" -j verbose=yes

cp -v "${SRC_DIR}/libvpx.a" "${LIB_DIR}"
cp -v "${SRC_DIR}/libvpx.so.6" "${LIB_DIR}"
