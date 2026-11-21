# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to find candidate directories for Java inline FQN cleanup."""

import os
import random
import re
import sys

SCAN_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__),
                 "../../../../../../../app_rating/src"))
if not os.path.exists(SCAN_DIR):
    SCAN_DIR = os.getcwd()

SKIP_DIRS = {
    ".git", ".cipd", "out", "build", "testing", "tools", "infra",
    "third_party", "clank"
}
FQN_REGEX = re.compile(
    r"\b(org\.chromium|android|com\.google|androidx|java|javax)"
    r"(?:\.[a-zA-Z0-9_]+)+\b")
URL_REGEX = re.compile(r"https?://\S+")


def should_skip_line(line):
    line = line.strip()
    if not line:
        return True
    if line.startswith("import ") or line.startswith("package "):
        return True
    if line.startswith("//") or line.startswith("/*") or line.startswith("*"):
        return True
    return False


def get_file_fqns(file_path):
    fqns = set()
    try:
        with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
            for idx, line in enumerate(f, start=1):
                # Skip license headers in first 15 lines
                if idx <= 15:
                    continue
                if should_skip_line(line):
                    continue
                # Remove URLs to avoid matching domain names in comments
                cleaned_line = URL_REGEX.sub("", line)
                for match in FQN_REGEX.finditer(cleaned_line):
                    fqn = match.group(0)
                    if len(fqn.split(".")) >= 3:
                        fqns.add(fqn)
    except Exception:
        pass
    return fqns


def analyze_directory(root, java_files):
    candidate_files = []
    dir_fqns = set()

    for java_file in java_files:
        file_path = os.path.join(root, java_file)
        fqns = get_file_fqns(file_path)
        if fqns:
            candidate_files.append(java_file)
            dir_fqns.update(fqns)

    if len(candidate_files) >= 1:
        rel_dir = os.path.relpath(root, SCAN_DIR)
        print("Candidate Batch Found:")
        print(f"Directory: {rel_dir}")
        print(f"File Count: {len(candidate_files)}")
        print("Files:")
        for f in candidate_files[:15]:
            print(f"  - {f}")
        if len(candidate_files) > 15:
            print(f"  - ... and {len(candidate_files) - 15} more")
        print("Unique FQNs/Imports to Clean:")
        sorted_fqns = sorted(list(dir_fqns))
        for fqn in sorted_fqns[:5]:
            print(f"  - {fqn}")
        if len(sorted_fqns) > 5:
            print(f"  - ... and {len(sorted_fqns) - 5} more")
        return True
    return False


def main():
    print(f"Scanning first-party Java files in {SCAN_DIR}...", file=sys.stderr)
    for root, dirs, files in os.walk(SCAN_DIR):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        random.shuffle(dirs)

        if "javatests" in root or "junit" in root or "test" in root:
            continue

        java_files = [f for f in files if f.endswith(".java")]
        if not java_files:
            continue

        if analyze_directory(root, java_files):
            sys.exit(0)

    print("No suitable batches found.")


if __name__ == "__main__":
    main()
