#!/usr/bin/env vpython3
# coding=utf-8
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for gerrit_client.py."""

import logging
import os
import sys
import unittest

if sys.version_info.major == 2:
  from StringIO import StringIO
  import mock
  BUILTIN_OPEN = '__builtin__.open'
else:
  from io import StringIO
  from unittest import mock
  BUILTIN_OPEN = 'builtins.open'

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import gerrit_client
import gerrit_util


class TestGerritClient(unittest.TestCase):
  @mock.patch('gerrit_util.GetGerritBranch', return_value='')
  def test_branch_info(self, util_mock):
    gerrit_client.main([
        'branchinfo', '--host', 'https://example.org/foo', '--project',
        'projectname', '--branch', 'branchname'
    ])
    util_mock.assert_called_once_with('example.org', 'projectname',
                                      'branchname')

  @mock.patch('gerrit_util.CreateGerritBranch', return_value='')
  def test_branch(self, util_mock):
    gerrit_client.main([
        'branch', '--host', 'https://example.org/foo', '--project',
        'projectname', '--branch', 'branchname', '--commit', 'commitname'
    ])
    util_mock.assert_called_once_with('example.org', 'projectname',
                                      'branchname', 'commitname')

  @mock.patch('gerrit_util.QueryChanges', return_value='')
  def test_changes(self, util_mock):
    gerrit_client.main([
        'changes', '--host', 'https://example.org/foo', '-p', 'foo=bar', '-p',
        'baz=qux', '--limit', '10', '--start', '20', '-o', 'op1', '-o', 'op2'
    ])
    util_mock.assert_called_once_with(
        'example.org', [('foo', 'bar'), ('baz', 'qux')],
        limit=10,
        start=20,
        o_params=['op1', 'op2'])

  @mock.patch('gerrit_util.GetRelatedChanges', return_value='')
  def test_relatedchanges(self, util_mock):
    gerrit_client.main([
        'relatedchanges', '--host', 'https://example.org/foo', '--change',
        'foo-change-id', '--revision', 'foo-revision-id'
    ])
    util_mock.assert_called_once_with('example.org',
                                      change='foo-change-id',
                                      revision='foo-revision-id')

  @mock.patch('gerrit_util.CreateChange', return_value={})
  def test_createchange(self, util_mock):
    gerrit_client.main([
        'createchange', '--host', 'https://example.org/foo', '--project',
        'project', '--branch', 'main', '--subject', 'subject', '-p',
        'work_in_progress=true'
    ])
    util_mock.assert_called_once_with('example.org',
                                      'project',
                                      branch='main',
                                      subject='subject',
                                      params=[('work_in_progress', 'true')])

  @mock.patch(BUILTIN_OPEN, mock.mock_open())
  @mock.patch('gerrit_util.ChangeEdit', return_value='')
  def test_changeedit(self, util_mock):
    open().read.return_value = 'test_data'
    gerrit_client.main([
        'changeedit', '--host', 'https://example.org/foo', '--change', '1',
        '--path', 'path/to/file', '--file', '/my/foo'
    ])
    util_mock.assert_called_once_with('example.org', 1, 'path/to/file',
                                      'test_data')

  @mock.patch('gerrit_util.PublishChangeEdit', return_value='')
  def test_publishchangeedit(self, util_mock):
    gerrit_client.main([
        'publishchangeedit', '--host', 'https://example.org/foo', '--change',
        '1', '--notify', 'yes'
    ])
    util_mock.assert_called_once_with('example.org', 1, 'yes')

  @mock.patch('gerrit_util.AbandonChange', return_value='')
  def test_abandon(self, util_mock):
    gerrit_client.main([
        'abandon', '--host', 'https://example.org/foo', '-c', '1', '-m', 'bar'
    ])
    util_mock.assert_called_once_with('example.org', 1, 'bar')

  @mock.patch('gerrit_util.SetReview', return_value='')
  def test_setlabel(self, util_mock):
    gerrit_client.main([
        'setlabel',
        '--host',
        'https://example.org/foo',
        '-c',
        '1',
        '-l',
        'some-label',
        '-2',
    ])
    util_mock.assert_called_once_with('example.org',
                                      1,
                                      labels={'some-label': '-2'})


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  unittest.main()
