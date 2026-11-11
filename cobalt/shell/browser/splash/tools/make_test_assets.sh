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

# Makes the assets to seed the splash cache with for a demo or benchmark, in
# the given directory:
#   splash_selector.wasm   the test selector (testdata/test_selector, Rust)
#   splash_1080.webm       the built-in splash video, colors inverted
#   splash_2160.webm       ... hue-shifted by 240 degrees
#   music.webm             ... hue-shifted by 120 degrees
# The videos are visibly different from the built-in one, so that one can tell
# on screen which the selector chose. Needs ffmpeg with libvpx.

set -euo pipefail

OUT="${1:?usage: $0 OUT_DIR}"
cd "$(dirname "$0")"
SRC_ROOT="$(git rev-parse --show-toplevel)"
SPLASH="${SRC_ROOT}/cobalt/shell/embedded_resources/loader_embedded_resources/splash.webm"
mkdir -p "${OUT}"

# The selector's bytes are embedded in test_selector_wasm.h.
python3 - "${OUT}/splash_selector.wasm" <<'EOF'
import re, sys
header = open("../testdata/test_selector_wasm.h").read()
data = bytes(int(x, 16) for x in re.findall(r"0x([0-9a-f]{2}),", header))
open(sys.argv[1], "wb").write(data)
print(f"splash_selector.wasm: {len(data)} bytes")
EOF

encode() {
  ffmpeg -loglevel error -y -i "${SPLASH}" -vf "$1" -c:v libvpx-vp9 -b:v 0 \
      -crf 30 -an "${OUT}/$2"
  echo "$2: $(stat -c %s "${OUT}/$2") bytes"
}
encode "negate" splash_1080.webm
encode "hue=h=240" splash_2160.webm
encode "hue=h=120" music.webm
