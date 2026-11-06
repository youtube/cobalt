#!/usr/bin/env python3
"""Resolve CODEOWNERS to critic reviewer personas."""

import fnmatch
import os
import subprocess
import sys


def cmd(args):
  return subprocess.check_output(args, text=True)


def match(path: str, pat: str) -> bool:
  p = pat.strip("/")
  if path == p or path.startswith(f"{p}/"):
    return True
  return fnmatch.fnmatch(path, p) or fnmatch.fnmatch(path, f"*/{p}")


def resolve():
  if len(sys.argv) > 1:
    files = sys.argv[1:]
  elif not sys.stdin.isatty():
    files = sys.stdin.read().splitlines()
  else:
    files = cmd(["git", "diff", "--name-only", "HEAD"]).splitlines()

  rules = []
  if os.path.exists(".github/CODEOWNERS"):
    with open(".github/CODEOWNERS", "r", encoding="utf-8") as f:
      for line in f:
        clean = line.split("#")[0].strip()
        if clean:
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
