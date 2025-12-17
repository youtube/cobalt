#!/usr/bin/env python
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for the _GetComponentMappings function in code coverage tools."""

import json
import os
import subprocess
import unittest
from unittest import mock

import coverage


class GetComponentMappingsTest(unittest.TestCase):
  """Test case for the _GetComponentMappings function."""

  @mock.patch('subprocess.check_output')
  @mock.patch('urllib.request.urlopen')
  def test_get_component_mappings_with_gcloud_auth(self, mock_urlopen,
                                                   mock_check_output):
    """Test that _GetComponentMappings works with gcloud auth."""
    # Mock the gcloud command to return a fake token.
    mock_check_output.return_value = b'fake-token\n'

    # Mock the urlopen call to return a fake component map.
    fake_component_map = {'dir-to-component': {'a/b': 'Component>Test'}}
    mock_urlopen.return_value.read.return_value = json.dumps(
        fake_component_map).encode('utf-8')

    # Call the function and assert that it returns the correct component map.
    component_map = coverage._GetComponentMappings(None)
    self.assertEqual(component_map, fake_component_map)

  @mock.patch('subprocess.check_output')
  @mock.patch('urllib.request.urlopen')
  def test_get_component_mappings_without_gcloud_auth(self, mock_urlopen,
                                                      mock_check_output):
    """Test that _GetComponentMappings works without gcloud auth."""
    # Mock the gcloud command to raise an exception.
    mock_check_output.side_effect = subprocess.CalledProcessError(1, 'gcloud')

    # Mock the urlopen call to return a fake component map.
    fake_component_map = {'dir-to-component': {'c/d': 'Component>Test2'}}
    mock_urlopen.return_value.read.return_value = json.dumps(
        fake_component_map).encode('utf-8')

    # Call the function and assert that it returns the correct component map.
    component_map = coverage._GetComponentMappings(None)
    self.assertEqual(component_map, fake_component_map)


if __name__ == '__main__':
  unittest.main()
