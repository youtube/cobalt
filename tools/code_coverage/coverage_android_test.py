#!/usr/bin/env python
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for Android-specific coverage logic."""

import unittest
from unittest import mock

import coverage

class CoverageAndroidTest(unittest.TestCase):

  def setUp(self):
    # Store original values to restore them after each test.
    self.original_build_dir = coverage.BUILD_DIR
    self.original_get_target_os = coverage._GetTargetOS

  def tearDown(self):
    # Restore original values.
    coverage.BUILD_DIR = self.original_build_dir
    coverage._GetTargetOS = self.original_get_target_os

  def test_is_android_detection(self):
    """Tests that _IsAndroid correctly detects Android builds."""
    # Mock _GetTargetOS to simulate a non-Android target OS.
    coverage._GetTargetOS = lambda: 'linux'

    # Test case 1: BUILD_DIR contains "android". Expect True.
    coverage.BUILD_DIR = '/path/to/out/coverage_android-x86'
    self.assertTrue(coverage._IsAndroid(),
                    "Failed on BUILD_DIR check with 'android' present.")

    # Test case 2: BUILD_DIR does not contain "android". Expect False.
    coverage.BUILD_DIR = '/path/to/out/coverage_linux-x64'
    self.assertFalse(coverage._IsAndroid(),
                     "Failed on BUILD_DIR check without 'android'.")

    # Test case 3: BUILD_DIR is None. Expect False (as target_os is not android).
    coverage.BUILD_DIR = None
    self.assertFalse(coverage._IsAndroid(), "Failed when BUILD_DIR is None.")

    # Mock _GetTargetOS to simulate an Android target OS.
    coverage._GetTargetOS = lambda: 'android'

    # Test case 4: target_os is 'android', BUILD_DIR does not contain "android". Expect True.
    coverage.BUILD_DIR = '/path/to/out/coverage_linux-x64'
    self.assertTrue(coverage._IsAndroid(),
                    "Failed on target_os='android' check.")

    # Test case 5: Both conditions are met. Expect True.
    coverage.BUILD_DIR = '/path/to/out/android_build'
    self.assertTrue(coverage._IsAndroid(), "Failed when both conditions are met.")

if __name__ == '__main__':
  unittest.main()
