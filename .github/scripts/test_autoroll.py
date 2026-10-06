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
"""Tests for autoroll.py and autoroll_lib.py."""

import io
import json
import os
import sys
import unittest
from unittest.mock import patch

# Add current directory to path
sys.path.append(os.path.dirname(__file__))

import autoroll  # pylint: disable=wrong-import-position
import autoroll_lib as lib  # pylint: disable=wrong-import-position


class TestAutorollLib(unittest.TestCase):
  """Tests for autoroll_lib functions."""

  def setUp(self):
    lib._PR_LABELS_CACHE.clear()  # pylint: disable=protected-access

  def tearDown(self):
    lib._PR_LABELS_CACHE.clear()  # pylint: disable=protected-access

  @patch('autoroll_lib.get_out')
  def test_get_rolled_source_items(self, mock_get_out):
    mock_get_out.return_value = (
        'Cherry pick PR #12799: content: Fail gracefully\n'
        'Refer to original PR: #12799\n\n'
        '(cherry picked from commit 1111111111111111111111111111111111111111)\n'
        'Cherry pick PR #12800: some feature\n'
        '(cherry picked from commit 2222222222222222222222222222222222222222)\n'
    )
    shas, prs = lib.get_rolled_source_items('27.lts')
    self.assertEqual(
        shas, {
            '1111111111111111111111111111111111111111',
            '2222222222222222222222222222222222222222',
        })
    self.assertEqual(prs, {12799, 12800})

  @patch('builtins.open')
  def test_load_pr_labels_from_file(self, mock_open):
    prs_data = [
        {
            'number': 123,
            'labels': [{
                'name': 'cp-27.lts'
            }, {
                'name': 'feature'
            }]
        },
        {
            'number': 456,
            'labels': []
        },
    ]
    mock_open.return_value.__enter__.return_value = io.StringIO(
        json.dumps(prs_data))

    lib.load_pr_labels_from_file('/fake/path/open_prs.json')
    self.assertEqual(lib.get_pr_labels(123), {'cp-27.lts', 'feature'})
    self.assertEqual(lib.get_pr_labels(456), set())
    # Uncached PR number should return empty set
    self.assertEqual(lib.get_pr_labels(999), set())


class TestAutorollMain(unittest.TestCase):
  """Tests for autoroll main logic."""

  # pylint: disable=too-many-positional-arguments,unused-argument

  def setUp(self):
    lib._PR_LABELS_CACHE.clear()  # pylint: disable=protected-access

  def tearDown(self):
    lib._PR_LABELS_CACHE.clear()  # pylint: disable=protected-access

  @patch('autoroll.cherry_pick')
  @patch('autoroll_lib.get_start_sha')
  @patch('autoroll_lib.get_commits')
  @patch('autoroll_lib.get_rolled_source_items')
  @patch('autoroll_lib.get_pr_labels')
  @patch('autoroll_lib.get_cherry_pick_metadata')
  def test_only_migrates_prs_with_target_cherry_pick_label(
      self, mock_metadata, mock_get_pr_labels, mock_rolled_items,
      mock_get_commits, mock_start_sha, mock_cherry_pick):
    mock_start_sha.side_effect = ['sha0', 'sha0']
    mock_rolled_items.return_value = (set(), set())
    mock_get_commits.return_value = [
        ('sha1', 'Title 1', '101'),  # has cp-27.lts
        ('sha2', 'Title 2', '102'),  # no cherry pick label
        ('sha3', 'Direct commit without PR', None),  # not a PR
        ('sha4', 'Title 4', '104'),  # has cp-27.lts
    ]

    def labels_for_pr(pr_num):
      if pr_num in ('101', 101, '104', 104):
        return {'cp-27.lts'}
      return {'some-other-label'}

    mock_get_pr_labels.side_effect = labels_for_pr
    mock_metadata.side_effect = [
        ('date1', 'author1', 'msg1'),
        ('date4', 'author4', 'msg4'),
    ]
    mock_cherry_pick.return_value = (lib.CommitStatus.SUCCESS, None)

    test_args = [
        'autoroll.py',
        '--source-branch',
        'main',
        '--target-branch',
        '27.lts',
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        '10',
        '--existing-pr-sha',
        '',
        '--mode',
        'label',
        '--prs-json',
        '/dummy/path.json',
    ]

    with patch('autoroll_lib.load_pr_labels_from_file'):
      captured_out = io.StringIO()
      with patch('sys.argv', test_args), patch('sys.stdout', captured_out):
        autoroll.main()

    # Only PR 101 and PR 104 should be cherry-picked
    self.assertEqual(mock_cherry_pick.call_count, 2)
    cherry_picked_shas = [
        call.args[0] for call in mock_cherry_pick.call_args_list
    ]
    self.assertEqual(cherry_picked_shas, ['sha1', 'sha4'])

    output = captured_out.getvalue().strip()
    self.assertEqual(output, '- #101\n- #104')

  @patch('autoroll.cherry_pick')
  @patch('autoroll_lib.get_start_sha')
  @patch('autoroll_lib.get_commits')
  @patch('autoroll_lib.get_rolled_source_items')
  @patch('autoroll_lib.load_pr_labels_from_file')
  @patch('autoroll_lib.get_cherry_pick_metadata')
  def test_full_mode_rolls_all_commits_regardless_of_labels(
      self, mock_metadata, mock_load_labels, mock_rolled_items,
      mock_get_commits, mock_start_sha, mock_cherry_pick):
    mock_start_sha.side_effect = ['sha0', 'sha0']
    mock_rolled_items.return_value = (set(), set())
    mock_get_commits.return_value = [
        ('sha1', 'Title 1', '101'),  # unlabeled
        ('sha2', 'Direct commit without PR', None),  # no PR
        ('sha3', 'Title 3', '103'),  # unlabeled
    ]
    mock_metadata.side_effect = [
        ('date1', 'author1', 'msg1'),
        ('date2', 'author2', 'msg2'),
        ('date3', 'author3', 'msg3'),
    ]
    mock_cherry_pick.return_value = (lib.CommitStatus.SUCCESS, None)

    test_args = [
        'autoroll.py',
        '--source-branch',
        'main',
        '--target-branch',
        'staging',
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        '10',
        '--existing-pr-sha',
        '',
        '--mode',
        'full',
    ]

    captured_out = io.StringIO()
    with patch('sys.argv', test_args), patch('sys.stdout', captured_out):
      autoroll.main()

    # All 3 commits should be cherry-picked in full mode
    self.assertEqual(mock_cherry_pick.call_count, 3)
    cherry_picked_shas = [
        call.args[0] for call in mock_cherry_pick.call_args_list
    ]
    self.assertEqual(cherry_picked_shas, ['sha1', 'sha2', 'sha3'])
    mock_load_labels.assert_not_called()

    output = captured_out.getvalue().strip()
    self.assertEqual(output, '- #101\n- sha2\n- #103')

  def test_label_mode_requires_prs_json(self):
    test_args = [
        'autoroll.py',
        '--source-branch',
        'main',
        '--target-branch',
        '27.lts',
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        '10',
        '--existing-pr-sha',
        '',
        '--mode',
        'label',
    ]

    with patch('autoroll_lib.get_start_sha', return_value='sha0'):
      with patch('autoroll_lib.get_commits', return_value=[]):
        with patch(
            'autoroll_lib.get_rolled_source_items',
            return_value=(set(), set())):
          with patch('sys.argv', test_args):
            with self.assertRaises(SystemExit) as cm:
              autoroll.main()
            self.assertEqual(cm.exception.code, 1)

  @patch('autoroll.cherry_pick')
  @patch('autoroll_lib.get_start_sha')
  @patch('autoroll_lib.get_commits')
  @patch('autoroll_lib.get_rolled_source_items')
  @patch('autoroll_lib.get_pr_labels')
  @patch('autoroll_lib.get_cherry_pick_metadata')
  def test_preserves_already_rolled_prs_and_picks_new(
      self, mock_metadata, mock_get_pr_labels, mock_rolled_items,
      mock_get_commits, mock_start_sha, mock_cherry_pick):
    mock_start_sha.side_effect = ['sha0', 'sha1']
    # sha1 is already in HEAD (cherry-picked previously)
    mock_rolled_items.return_value = ({'sha1'}, {101})
    mock_get_commits.return_value = [
        ('sha1', 'Title 1', '101'),  # already rolled
        ('sha2', 'Title 2', '102'),  # no cp-27.lts
        ('sha3', 'Title 3', '103'),  # has cp-27.lts, new
    ]

    def labels_for_pr(pr_num):
      if pr_num in ('101', 101, '103', 103):
        return {'cp-27.lts'}
      return set()

    mock_get_pr_labels.side_effect = labels_for_pr
    mock_metadata.return_value = ('date3', 'author3', 'msg3')
    mock_cherry_pick.return_value = (lib.CommitStatus.SUCCESS, None)

    test_args = [
        'autoroll.py',
        '--source-branch',
        'main',
        '--target-branch',
        '27.lts',
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        '10',
        '--existing-pr-sha',
        '',
        '--mode',
        'label',
        '--prs-json',
        '/dummy/path.json',
    ]

    with patch('autoroll_lib.load_pr_labels_from_file'):
      captured_out = io.StringIO()
      with patch('sys.argv', test_args), patch('sys.stdout', captured_out):
        autoroll.main()

    # Only sha3 should be cherry picked (sha1 was already rolled)
    self.assertEqual(mock_cherry_pick.call_count, 1)
    self.assertEqual(mock_cherry_pick.call_args[0][0], 'sha3')

    # Output includes both already-rolled and newly-rolled PRs
    output = captured_out.getvalue().strip()
    self.assertEqual(output, '- #101\n- #103')

  @patch('autoroll.cherry_pick')
  @patch('autoroll_lib.get_start_sha')
  @patch('autoroll_lib.get_commits')
  @patch('autoroll_lib.get_rolled_source_items')
  @patch('autoroll_lib.get_pr_labels')
  @patch('autoroll_lib.get_cherry_pick_metadata')
  def test_skips_commit_if_pr_number_already_rolled(
      self, mock_metadata, mock_get_pr_labels, mock_rolled_items,
      mock_get_commits, mock_start_sha, mock_cherry_pick):
    mock_start_sha.side_effect = ['sha0', 'sha0']
    # PR 101 was already rolled, but under a different SHA in target
    mock_rolled_items.return_value = ({'different_sha'}, {101})
    mock_get_commits.return_value = [
        ('new_sha_for_101', 'Title 1', '101'),  # PR 101 already rolled
        ('sha2', 'Title 2', '102'),  # new PR
    ]

    mock_get_pr_labels.return_value = {'cp-27.lts'}
    mock_metadata.return_value = ('date2', 'author2', 'msg2')
    mock_cherry_pick.return_value = (lib.CommitStatus.SUCCESS, None)

    test_args = [
        'autoroll.py',
        '--source-branch',
        'main',
        '--target-branch',
        '27.lts',
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        '10',
        '--existing-pr-sha',
        '',
        '--mode',
        'label',
        '--prs-json',
        '/dummy/path.json',
    ]

    with patch('autoroll_lib.load_pr_labels_from_file'):
      captured_out = io.StringIO()
      with patch('sys.argv', test_args), patch('sys.stdout', captured_out):
        autoroll.main()

    # Only sha2 should be cherry-picked
    self.assertEqual(mock_cherry_pick.call_count, 1)
    self.assertEqual(mock_cherry_pick.call_args[0][0], 'sha2')
    output = captured_out.getvalue().strip()
    self.assertEqual(output, '- #101\n- #102')

  @patch('autoroll_lib.get_cherry_pick_metadata')
  @patch('autoroll.cherry_pick')
  @patch('autoroll_lib.get_start_sha')
  @patch('autoroll_lib.get_commits')
  @patch('autoroll_lib.get_rolled_source_items')
  @patch('autoroll_lib.get_pr_labels')
  def test_respects_max_commits(self, mock_get_pr_labels, mock_rolled_items,
                                mock_get_commits, mock_start_sha,
                                mock_cherry_pick, mock_metadata):
    mock_metadata.return_value = ('date', 'author', 'msg')
    mock_start_sha.side_effect = ['sha0', 'sha0']
    mock_rolled_items.return_value = (set(), set())
    mock_get_commits.return_value = [
        ('sha1', 'Title 1', '101'),
        ('sha2', 'Title 2', '102'),
        ('sha3', 'Title 3', '103'),
    ]
    mock_get_pr_labels.return_value = {'cp-27.lts'}
    mock_cherry_pick.return_value = (lib.CommitStatus.SUCCESS, None)

    test_args = [
        'autoroll.py',
        '--source-branch',
        'main',
        '--target-branch',
        '27.lts',
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        '2',
        '--existing-pr-sha',
        '',
        '--mode',
        'label',
        '--prs-json',
        '/dummy/path.json',
    ]

    with patch('autoroll_lib.load_pr_labels_from_file'):
      captured_out = io.StringIO()
      with patch('sys.argv', test_args), patch('sys.stdout', captured_out):
        autoroll.main()

    # Only 2 commits should be cherry-picked
    self.assertEqual(mock_cherry_pick.call_count, 2)
    output = captured_out.getvalue().strip()
    self.assertEqual(output, '- #101\n- #102')


if __name__ == '__main__':
  unittest.main()
