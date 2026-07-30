#!/bin/bash
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ICU_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${1:-$(mktemp -d /tmp/icu_cobalt_build_XXXXXX)}"
NUM_CORES="${NUM_CORES:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"

echo "==> Building Cobalt ICU data bundle"
echo "    ICU Root:  ${ICU_ROOT}"
echo "    Filter:    ${ICU_ROOT}/filters/cobalt.json"
echo "    Build Dir: ${BUILD_DIR}"
echo "    Cores:     ${NUM_CORES}"

# Ensure build directory exists and resolve to an absolute path
mkdir -p "${BUILD_DIR}"
BUILD_DIR="$(cd "${BUILD_DIR}" && pwd)"
cd "${BUILD_DIR}"

# Step 1: Configure and build host tools
echo "==> Step 1/3: Configuring and building host ICU tools..."
"${ICU_ROOT}/source/runConfigureICU" Linux/gcc \
    --disable-tests --disable-samples --disable-layoutex --enable-rpath \
    --prefix="${BUILD_DIR}" > configure_tools.log 2>&1
make -j"${NUM_CORES}" > make_tools.log 2>&1

# Step 2: Configure data build with Cobalt filter and compile
echo "==> Step 2/3: Building filtered data for Cobalt..."
make -C data clean > /dev/null 2>&1 || true

ICU_DATA_FILTER_FILE="${ICU_ROOT}/filters/cobalt.json" \
"${ICU_ROOT}/source/runConfigureICU" Linux/gcc \
    --disable-tests --disable-samples --disable-layoutex --enable-rpath \
    --prefix="${BUILD_DIR}" > configure_data.log 2>&1

make -j"${NUM_CORES}" -C data > make_data.log 2>&1

# Step 3: Copy generated data file to third_party/icu/cobalt/icudtl.dat
echo "==> Step 3/3: Copying icudtl.dat to ${ICU_ROOT}/cobalt/icudtl.dat..."
"${SCRIPT_DIR}/copy_data.sh" cobalt

# Clean up if a temporary directory was used
if [[ -z "$1" ]]; then
  echo "==> Cleaning up temporary build directory..."
  rm -rf "${BUILD_DIR}"
fi

echo "==> Done! Cobalt ICU data successfully generated at ${ICU_ROOT}/cobalt/icudtl.dat"
ls -lh "${ICU_ROOT}/cobalt/icudtl.dat"
