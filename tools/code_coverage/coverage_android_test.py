#!/usr/bin/env python
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for Android-specific coverage logic."""

import os
import shutil
import tempfile
import unittest
from unittest import mock

import coverage

class CoverageAndroidTest(unittest.TestCase):

  def setUp(self):
    # Store original values to restore them after each test.
    self.original_build_dir = coverage.BUILD_DIR
    self.original_get_target_os = coverage._GetTargetOS
    self.original_build_args = coverage._BUILD_ARGS
    self.test_dir = tempfile.mkdtemp()

  def tearDown(self):
    # Restore original values.
    coverage.BUILD_DIR = self.original_build_dir
    coverage._GetTargetOS = self.original_get_target_os
    coverage._BUILD_ARGS = self.original_build_args
    shutil.rmtree(self.test_dir)

  def test_is_android_detection(self):
    """Tests that _IsAndroid correctly detects Android builds (original logic)."""
    # Mock _GetTargetOS to simulate a non-Android target OS.
    coverage._GetTargetOS = lambda: 'linux'

    # Test case 1: target_os is 'linux'. Expect False.
    self.assertFalse(coverage._IsAndroid(),
                    "Failed on target_os='linux' check.")

    # Mock _GetTargetOS to simulate an Android target OS.
    coverage._GetTargetOS = lambda: 'android'

    # Test case 2: target_os is 'android'. Expect True.
    self.assertTrue(coverage._IsAndroid(),
                    "Failed on target_os='android' check.")

  def test_get_build_args_with_imports(self):
    """Tests that _GetBuildArgs correctly parses args.gn with imports."""
    # Create a mock build directory and args.gn files.
    build_dir = os.path.join(self.test_dir, 'out', 'coverage')
    os.makedirs(build_dir)
    
    cobalt_build_args_dir = os.path.join(self.test_dir, 'cobalt', 'build', 'args')
    os.makedirs(cobalt_build_args_dir)

    with open(os.path.join(build_dir, 'args.gn'), 'w') as f:
      f.write('import("//cobalt/build/args/testing.gni")\n')
      f.write('is_debug = true\n')

    with open(os.path.join(cobalt_build_args_dir, 'testing.gni'), 'w') as f:
      f.write('target_os = "android"\n')

    # Set the global BUILD_DIR and SRC_ROOT_PATH for the coverage script.
    coverage.BUILD_DIR = build_dir
    coverage.SRC_ROOT_PATH = self.test_dir
    coverage._BUILD_ARGS = None  # Reset cache

    # Call the function and verify the results.
    build_args = coverage._GetBuildArgs()
    self.assertEqual(build_args.get('target_os'), 'android')
    self.assertEqual(build_args.get('is_debug'), 'true')


if __name__ == '__main__':
  unittest.main()
