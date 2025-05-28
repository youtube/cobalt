#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import os.path
import shutil
import subprocess
import sys


def RunRewritingTests():
  subprocess.run([
      "tools/clang/scripts/test_tool.py", "--extract-edits-path", "..",
      "--apply-edits", "rewrite_templated_container_fields"
  ])


def main():
  if not os.path.exists("ATL_OWNERS"):
    sys.stderr.write(
        "Please run run_all_tests.py from the root dir of Chromium")
    return -1

  if not os.path.exists("third_party/llvm-build/Release+Asserts/bin/"
                        "rewrite_templated_container_fields"):
    sys.stderr.write("Please build rewrite_templated_container_fields first")
    return -1

  RunRewritingTests()


if __name__ == "__main__":
  main()
