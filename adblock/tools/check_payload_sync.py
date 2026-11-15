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

"""Verifies that both adblock.js copies are byte-identical.

The canonical payload lives in the APK patcher
(youtube-tv-cobalt-adblock/src/payload/adblock.js). The same bytes are embedded
twice in the Cobalt patch: as cobalt/adblock/adblock.js and inside the generated
cobalt/adblock/adblock_js.h. This script compares all three.

Usage:
  check_payload_sync.py [--patch PATH] [--payload PATH]

Exit status is 0 when everything matches, 1 on drift, 2 on usage or I/O errors.
"""

import argparse
import sys
from pathlib import Path

_DELIMITER = "YTADBLOCK_JS"
_PATCH_FILE = "0001-cobalt-adblock-27lts.patch"
_PAYLOAD = Path("youtube-tv-cobalt-adblock/src/payload/adblock.js")


def extract_added(patch_text, path):
  marker = "diff --git a/{0} b/{0}".format(path)
  start = patch_text.find(marker)
  if start == -1:
    raise SystemExit("patch does not create {0}".format(path))
  end = patch_text.find("\ndiff --git ", start + 1)
  section = patch_text[start:] if end == -1 else patch_text[start:end]

  added = []
  in_hunk = False
  for line in section.splitlines():
    if line.startswith("@@"):
      in_hunk = True
      continue
    if not in_hunk:
      continue
    if line.startswith("+"):
      added.append(line[1:])
    elif line.startswith("\\"):
      continue
  if not added:
    raise SystemExit("no added lines found for {0}".format(path))
  text = "\n".join(added)
  if "\\ No newline at end of file" not in section:
    text += "\n"
  return text


def first_difference(expected, actual):
  expected_lines = expected.splitlines()
  actual_lines = actual.splitlines()
  for index in range(max(len(expected_lines), len(actual_lines))):
    left = expected_lines[index] if index < len(expected_lines) else "<missing>"
    right = actual_lines[index] if index < len(actual_lines) else "<missing>"
    if left != right:
      return "line {0}: patch has {1!r}, payload has {2!r}".format(
          index + 1, left, right)
  return "trailing-byte difference only"


def check_header(header, script):
  opening = 'R"{0}(\n'.format(_DELIMITER)
  closing = '\n){0}"'.format(_DELIMITER)
  if opening not in header or closing not in header:
    return "adblock_js.h does not contain the expected raw string"
  start = header.index(opening) + len(opening)
  end = header.index(closing)
  if header[start:end] != script.rstrip("\n"):
    return "adblock_js.h is stale; re-run cobalt/adblock/generate_adblock_js.py"
  return None


def main():
  repo = Path(__file__).resolve().parent.parent
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      "--patch", type=Path, default=repo / "patches" / _PATCH_FILE)
  parser.add_argument(
      "--payload", type=Path, default=repo.parent / _PAYLOAD)
  args = parser.parse_args()

  try:
    patch_text = args.patch.read_text(encoding="utf-8")
    payload = args.payload.read_text(encoding="utf-8")
  except OSError as error:
    print("error: {0}".format(error), file=sys.stderr)
    return 2

  embedded = extract_added(patch_text, "cobalt/adblock/adblock.js")
  if embedded != payload:
    print("adblock.js copies differ: {0}".format(
        first_difference(embedded, payload)), file=sys.stderr)
    print(
        "update the patch from {0}".format(args.payload), file=sys.stderr)
    return 1

  header = extract_added(patch_text, "cobalt/adblock/adblock_js.h")
  problem = check_header(header, payload)
  if problem:
    print("error: {0}".format(problem), file=sys.stderr)
    return 1

  print("adblock.js is identical in the patch, the embedded header, and {0}"
        .format(args.payload))
  return 0


if __name__ == "__main__":
  sys.exit(main())
