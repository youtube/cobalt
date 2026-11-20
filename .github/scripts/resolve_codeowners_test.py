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
"""Unit tests for resolve_codeowners.py."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).with_name('resolve_codeowners.py')

REAL_CODEOWNERS_SAMPLE = """\
/.github/ @youtube/cobalt-ci-owners
/.github/CODEOWNERS @youtube/cobalt-starboard-owners
/cobalt/build/*.py @youtube/cobalt-ci-owners
/cobalt/build/**/*.py @youtube/cobalt-ci-owners
/cobalt/build/gn.py @youtube/cobalt-build
/starboard/  # no default owners
/starboard/*.h @youtube/cobalt-starboard-owners
/starboard/sabi @youtube/cobalt-starboard-owners
*.idl @youtube/cobalt-web-api
*.nplb_filter.json @youtube/nplb-filters
"""


class ResolveCodeownersTest(unittest.TestCase):
  """Tests for CODEOWNERS resolution and reviewer paths matching."""

  def _make_repo(self, codeowners_text: str = REAL_CODEOWNERS_SAMPLE) -> Path:
    tmp = Path(tempfile.mkdtemp())
    self.addCleanup(shutil.rmtree, tmp)
    gh_dir = tmp / '.github'
    gh_dir.mkdir()
    (gh_dir / 'CODEOWNERS').write_text(codeowners_text, encoding='utf-8')
    return tmp

  def _run(self, cwd: Path, args: list[str], stdin_text: str = '') -> list[str]:
    res = subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        cwd=cwd,
        input=stdin_text,
        capture_output=True,
        text=True,
        check=True,
    )
    return res.stdout.splitlines()

  def test_repository_owners_suffix_and_aliases(self):
    repo = self._make_repo('* @youtube/default-owners\n'
                           '/cobalt/ @youtube/cobalt-ci-repository-owners\n'
                           'starboard/ @youtube/nplb-filters-owners\n')
    self.assertEqual(
        self._run(repo, ['cobalt/foo.cc', 'starboard/bar.cc']),
        ['cobalt-ci-reviewer.md', 'cobalt-starboard-reviewer.md'],
    )

  def test_leading_slash_anchoring_and_no_default_owners(self):
    repo = self._make_repo()
    # /starboard/ has no default owners, so nested .cc has no owner.
    self.assertEqual(self._run(repo, ['starboard/sub/foo.cc']), [])
    # Unanchored prefix should not match /starboard/.
    self.assertEqual(self._run(repo, ['other/starboard/foo.h']), [])

  def test_single_star_does_not_cross_slash(self):
    repo = self._make_repo()
    # /starboard/*.h matches root headers only, not nested headers.
    self.assertEqual(
        self._run(repo, ['starboard/export.h']),
        ['cobalt-starboard-reviewer.md'],
    )
    self.assertEqual(self._run(repo, ['starboard/nested/export.h']), [])

  def test_double_star_slash_matches_zero_or_more_dirs(self):
    repo = self._make_repo()
    self.assertEqual(
        self._run(
            repo,
            ['cobalt/build/foo.py', 'cobalt/build/a/foo.py'],
        ),
        ['cobalt-ci-reviewer.md'],
    )
    self.assertEqual(
        self._run(repo, ['cobalt/build/a/b/foo.py']),
        ['cobalt-ci-reviewer.md'],
    )
    # Last-match-wins override on /cobalt/build/gn.py.
    self.assertEqual(
        self._run(repo, ['cobalt/build/gn.py']),
        ['cobalt-build-reviewer.md'],
    )

  def test_unanchored_extension_and_codeowners_override(self):
    repo = self._make_repo()
    self.assertEqual(
        self._run(repo, ['cobalt/dom/window.idl']),
        ['cobalt-web-api-reviewer.md'],
    )
    self.assertEqual(
        self._run(repo, ['.github/CODEOWNERS']),
        ['cobalt-starboard-reviewer.md'],
    )
    self.assertEqual(
        self._run(repo, ['.github/workflows/pull_request.yml']),
        ['cobalt-ci-reviewer.md'],
    )

  def test_stdin_input(self):
    repo = self._make_repo()
    self.assertEqual(
        self._run(repo, [], stdin_text='cobalt/dom/window.idl\n'),
        ['cobalt-web-api-reviewer.md'],
    )

  def test_paths_frontmatter_matching(self):
    repo = self._make_repo()
    reviewers_dir = repo / '.github' / 'reviewers'
    reviewers_dir.mkdir(parents=True)
    (reviewers_dir / 'cobalt-ci-reviewer.md').write_text(
        '---\n'
        'name: cobalt-ci-reviewer\n'
        'paths:\n'
        '  - "./.github/**"\n'
        'codeowner_teams:\n'
        '  - "@youtube/cobalt-ci-owners"\n'
        '---\n'
        'Prompt body.\n',
        encoding='utf-8',
    )
    self.assertEqual(
        self._run(
            repo,
            ['--paths', '.github/workflows/pull_request.yml'],
        ),
        ['.github/reviewers/cobalt-ci-reviewer.md'],
    )
    # .github/CODEOWNERS is owned by cobalt-starboard-owners, not cobalt-ci.
    self.assertEqual(
        self._run(repo, ['--paths', '.github/CODEOWNERS']),
        [],
    )
    # Unmatched file returns nothing.
    self.assertEqual(
        self._run(repo, ['--paths', 'cobalt/dom/window.idl']),
        [],
    )


if __name__ == '__main__':
  unittest.main()
