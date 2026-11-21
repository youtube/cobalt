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

  def _run(self, codeowners, args, reviewers=None):
    tmp = Path(tempfile.mkdtemp())
    self.addCleanup(shutil.rmtree, tmp)
    gh_dir = tmp / ".github"
    gh_dir.mkdir()
    (gh_dir / "CODEOWNERS").write_text(codeowners, encoding="utf-8")
    if reviewers:
      rdir = gh_dir / "reviewers"
      rdir.mkdir()
      for name, content in reviewers.items():
        (rdir / name).write_text(content, encoding="utf-8")
    res = subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        cwd=tmp,
        capture_output=True,
        text=True,
        check=True,
    )
    return res.stdout.splitlines()

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

  def test_repository_owners_suffix(self):
    self.assertEqual(
        self._run(
            "/cobalt/ @youtube/cobalt-ci-repository-owners\n",
            ["cobalt/foo.cc"],
        ),
        ["cobalt-ci-reviewer.md"],
    )

  def test_anchoring_and_single_star(self):
    codeowners = ("* @youtube/default-owners\n"
                  "/starboard/  # no default owners\n"
                  "/starboard/*.h @youtube/cobalt-starboard-owners\n"
                  "*.idl @youtube/cobalt-web-api\n")
    self.assertEqual(self._run(codeowners, ["starboard/sub/foo.cc"]), [])
    self.assertEqual(
        self._run(codeowners, ["other/starboard/foo.h"]),
        ["default-reviewer.md"],
    )
    self.assertEqual(
        self._run(codeowners, ["starboard/export.h"]),
        ["cobalt-starboard-reviewer.md"],
    )
    self.assertEqual(self._run(codeowners, ["starboard/nested/export.h"]), [])
    self.assertEqual(
        self._run(codeowners, ["cobalt/dom/window.idl"]),
        ["cobalt-web-api-reviewer.md"],
    )

  def test_double_star_globbing(self):
    codeowners = ("/cobalt/build/**/*.py @youtube/cobalt-ci-owners\n"
                  "/cobalt/build/gn.py @youtube/cobalt-build\n")
    self.assertEqual(
        self._run(
            codeowners,
            [
                "cobalt/build/foo.py",
                "cobalt/build/a/foo.py",
                "cobalt/build/a/b/foo.py",
            ],
        ),
        ["cobalt-ci-reviewer.md"],
    )
    self.assertEqual(
        self._run(codeowners, ["cobalt/build/gn.py"]),
        ["cobalt-build-reviewer.md"],
    )

  def test_paths_mode(self):
    codeowners = ("/.github/ @youtube/cobalt-ci-owners\n"
                  "/.github/CODEOWNERS @youtube/cobalt-starboard-owners\n")
    reviewers = {
        "cobalt-ci-reviewer.md": ("---\n"
                                  "name: cobalt-ci-reviewer\n"
                                  "paths:\n"
                                  "  - './.github/**'\n"
                                  "codeowner_teams:\n"
                                  "  - '@youtube/cobalt-ci-owners'\n"
                                  "---\n"
                                  "Prompt body.\n"),
    }
    self.assertEqual(
        self._run(
            codeowners,
            ["--paths", ".github/workflows/pull_request.yml"],
            reviewers=reviewers,
        ),
        [".github/reviewers/cobalt-ci-reviewer.md"],
    )
    self.assertEqual(
        self._run(codeowners, ["--paths", ".github/CODEOWNERS"], reviewers),
        [],
    )
    self.assertEqual(
        self._run(codeowners, ["--paths", "cobalt/dom/window.idl"], reviewers),
        [],
    )


if __name__ == "__main__":
  unittest.main()
