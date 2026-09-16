#!/usr/bin/env python3
"""Resolve CODEOWNERS to critic reviewer personas."""

import os
import re
import subprocess
import sys


def match(path: str, pat: str) -> bool:
  p = pat.strip("/").replace(".", r"\.").replace("**", "\x00").replace(
      "*", "[^/]*").replace("\x00", ".*")
  return bool(
      re.match(f"^{p}(/.*)?$" if pat.startswith("/") else f"(^|.*/){p}(/.*)?$",
               path))


def resolve():
  args = sys.argv[1:]
  if args and args[0] == "--pr":
    files = subprocess.check_output(
        ["gh", "pr", "diff", args[1], "--name-only"], text=True).splitlines()
  else:
    files = args or subprocess.check_output(
        ["git", "diff", "--name-only", "HEAD"], text=True).splitlines()

  rules = []
  if os.path.exists(".github/CODEOWNERS"):
    with open(".github/CODEOWNERS", "r", encoding="utf-8") as f:
      for line in f:
        clean = re.sub(r"\s+#.*$", "", line).strip()
        if clean and not clean.startswith("#"):
          parts = clean.split()
          rules.append((parts[0], parts[1:]))

  reviewers = set()
  for f in [x.strip().lstrip("/") for x in files if x.strip()]:
    for pat, owners in reversed(rules):
      if match(f, pat):
        for o in owners:
          t = o.removeprefix("@youtube/").removesuffix("-owners").removesuffix(
              "-repository-owners")
          name = "cobalt-starboard" if t == "nplb-filters" else t
          reviewers.add(f"{name}-reviewer.md")
        break

  for r in sorted(reviewers):
    print(r)


if __name__ == "__main__":
  resolve()
