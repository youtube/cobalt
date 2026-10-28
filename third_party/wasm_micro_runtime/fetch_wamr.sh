#!/bin/bash
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

# Fetches WAMR (WebAssembly Micro Runtime) from upstream into src/, which
# BUILD.gn builds from and git ignores. Run it once before building
# //third_party/wasm_micro_runtime:wamr.

set -euo pipefail

readonly REPO="https://github.com/bytecodealliance/wasm-micro-runtime.git"
# Upstream commit, closest release WAMR-2.4.4. Same as google3's
# //third_party/wasm_micro_runtime.
readonly COMMIT="8c18e3f68b16c4bcaf05996b2636f6ed2b4cf629"

cd "$(dirname "$0")"

if [[ -d src/.git ]]; then
  if [[ "$(git -C src rev-parse HEAD)" == "${COMMIT}" ]]; then
    echo "WAMR is already at ${COMMIT}."
    exit 0
  fi
  git -C src fetch --depth 1 origin "${COMMIT}"
else
  rm -rf src
  git init -q src
  git -C src remote add origin "${REPO}"
  git -C src fetch --depth 1 origin "${COMMIT}"
fi
git -C src checkout -q --detach FETCH_HEAD
echo "WAMR fetched at ${COMMIT}."
