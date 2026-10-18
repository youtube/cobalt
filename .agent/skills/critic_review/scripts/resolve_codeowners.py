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
  files = []
  fmt_markdown = "--format" in args and "markdown" in args

  if "--pr" in args:
    idx = args.index("--pr")
    if idx + 1 < len(args):
      pr_num = args[idx + 1]
      try:
        res = subprocess.check_output([
            "gh", "pr", "view",
            str(pr_num), "--json", "files", "--jq", ".files[].path"
        ],
                                      text=True)
        files = [f.strip() for f in res.splitlines() if f.strip()]
      except (subprocess.CalledProcessError, OSError):
        files = []
  elif "--diff" in args:
    try:
      res = subprocess.check_output(["git", "diff", "--name-only", "HEAD"],
                                    text=True)
      files = [f.strip() for f in res.strip().splitlines() if f.strip()]
      if not files:
        status_out = subprocess.check_output(["git", "status", "--porcelain"],
                                             text=True)
        files = [
            line[3:].strip()
            for line in status_out.splitlines()
            if line.strip()
        ]
    except (subprocess.CalledProcessError, OSError):
      files = []
  else:
    files = [a for a in args if not a.startswith("--") and a != "markdown"]
    if not files:
      try:
        res = subprocess.check_output(["git", "diff", "--name-only", "HEAD"],
                                      text=True)
        files = [f.strip() for f in res.splitlines() if f.strip()]
      except (subprocess.CalledProcessError, OSError):
        files = []

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
    if fmt_markdown:
      print(f"- `{r}`")
    else:
      print(r)


if __name__ == "__main__":
  resolve()
