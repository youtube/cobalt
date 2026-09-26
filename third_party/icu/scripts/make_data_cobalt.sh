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
OUT_FILE="${2:-${ICU_ROOT}/cobalt/icudtl.dat}"
FILTER_FILE="${ICU_DATA_FILTER_FILE:-${ICU_ROOT}/filters/cobalt.json}"

echo "==> Building Cobalt ICU data bundle"
echo "    Filter:    ${FILTER_FILE}"
echo "    Build Dir: ${BUILD_DIR}"
echo "    Out File:  ${OUT_FILE}"

python3 "${SCRIPT_DIR}/generate_cobalt_icudata.py" \
    --filter-file "${FILTER_FILE}" \
    --build-dir "${BUILD_DIR}" \
    --out-file "${OUT_FILE}"

if [[ -z "$1" ]]; then
  rm -rf "${BUILD_DIR}"
fi

echo "==> Done! Cobalt ICU data successfully generated at ${OUT_FILE}"
ls -lh "${OUT_FILE}"

