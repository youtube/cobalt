#!/usr/bin/env python3
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

"""Checks that adblock_js.h embeds the current adblock.js."""

import sys
from pathlib import Path

_DELIMITER = "YTADBLOCK_JS"


def main():
  if len(sys.argv) > 2:
    raise SystemExit("usage: verify_embedded_payload.py [cobalt/adblock]")
  adblock_dir = Path(sys.argv[1] if len(sys.argv) == 2 else "cobalt/adblock")
  script = (adblock_dir / "adblock.js").read_text(encoding="utf-8")
  header = (adblock_dir / "adblock_js.h").read_text(encoding="utf-8")

  opening = 'R"{0}(\n'.format(_DELIMITER)
  closing = '\n){0}"'.format(_DELIMITER)
  if opening not in header or closing not in header:
    raise SystemExit("adblock_js.h does not contain the expected raw string")

  start = header.index(opening) + len(opening)
  end = header.index(closing)
  if header[start:end] != script.rstrip("\n"):
    raise SystemExit(
        "adblock_js.h is stale; run python3 cobalt/adblock/generate_adblock_js.py"
    )
  print("adblock_js.h embeds adblock.js")


if __name__ == "__main__":
  main()
