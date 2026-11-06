#!/usr/bin/env python3
"""Unit tests for resolve_codeowners.py."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).with_name("resolve_codeowners.py")


class ResolveCodeownersTest(unittest.TestCase):

  def test_resolve_codeowners(self):
    tmp = tempfile.mkdtemp()
    self.addCleanup(shutil.rmtree, tmp)
    gh_dir = Path(tmp) / ".github"
    gh_dir.mkdir()
    (gh_dir / "CODEOWNERS").write_text(
        "* @youtube/default-owners\n"
        "/cobalt/ @youtube/cobalt-ci\n"
        "starboard/ @youtube/nplb-filters-owners\n",
        encoding="utf-8",
    )
    res = subprocess.run(
        [sys.executable,
         str(SCRIPT), "cobalt/foo.cc", "starboard/bar.cc"],
        cwd=tmp,
        capture_output=True,
        text=True,
        check=True,
    )
    self.assertEqual(
        res.stdout.splitlines(),
        ["cobalt-ci-reviewer.md", "cobalt-starboard-reviewer.md"],
    )


if __name__ == "__main__":
  unittest.main()
