#!/usr/bin/env vpython3
"""Tests for split_cl."""

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import split_cl


class SplitClTest(unittest.TestCase):
  _description = """Convert use of X to Y in $directory

<add some background about this conversion for the reviewers>

"""

  _footers = 'Bug: 12345'

  def testAddUploadedByGitClSplitToDescription(self):
    added_line = 'This CL was uploaded by git cl split.'

    # Description without footers
    self.assertEqual(
        split_cl.AddUploadedByGitClSplitToDescription(self._description),
        self._description + added_line)
    # Description with footers
    self.assertEqual(
        split_cl.AddUploadedByGitClSplitToDescription(self._description +
                                                      self._footers),
        self._description + added_line + '\n\n' + self._footers)


if __name__ == '__main__':
  unittest.main()
