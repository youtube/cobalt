#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for code_coverage_installer.py."""

import pathlib
import sys
import unittest
from unittest.mock import patch, MagicMock

from pyfakefs import fake_filesystem_unittest

import code_coverage_installer


class CodeCoverageInstallerTest(fake_filesystem_unittest.TestCase):

  def setUp(self):
    self.setUpPyfakefs()
    self.src_root = pathlib.Path('/src')
    self.fs.create_dir('/src')

  def test_verify_chromium_checkout_success(self):
    self.fs.create_file('/src/DEPS')

    with patch('sys.stdout') as _:
      result = code_coverage_installer.verify_chromium_checkout(self.src_root)
    self.assertTrue(result)

  def test_verify_chromium_checkout_missing_deps(self):
    with patch('sys.stdout') as _, patch('sys.stderr') as _:
      result = code_coverage_installer.verify_chromium_checkout(self.src_root)
    self.assertFalse(result)

  def test_verify_llvm_tools_success(self):
    self.fs.create_file(
        '/src/third_party/llvm-build/Release+Asserts/bin/llvm-cov')
    self.fs.create_file(
        '/src/third_party/llvm-build/Release+Asserts/bin/llvm-profdata')

    with patch('sys.stdout') as _:
      result = code_coverage_installer.verify_llvm_tools(self.src_root)
    self.assertTrue(result)

  def test_verify_llvm_tools_missing(self):
    with patch('sys.stdout') as _, patch('sys.stderr') as _:
      result = code_coverage_installer.verify_llvm_tools(self.src_root)
    self.assertFalse(result)

  def test_verify_recipes_success(self):
    self.fs.create_dir('/infra_dir/build/recipes/recipe_modules/code_coverage')
    infra_dir = pathlib.Path('/infra_dir')
    with patch('sys.stdout') as _:
      result = code_coverage_installer.verify_recipes(infra_dir)
    self.assertTrue(result)

  def test_verify_recipes_missing(self):
    infra_dir = pathlib.Path('/infra_dir')
    with patch('sys.stdout') as _:
      result = code_coverage_installer.verify_recipes(infra_dir)
    self.assertFalse(result)

  def test_verify_service_success(self):
    self.fs.create_dir('/infra_dir/infra/appengine/findit')
    infra_dir = pathlib.Path('/infra_dir')
    with patch('sys.stdout') as _:
      result = code_coverage_installer.verify_service(infra_dir)
    self.assertTrue(result)

  def test_verify_service_missing(self):
    infra_dir = pathlib.Path('/infra_dir')
    with patch('sys.stdout') as _:
      result = code_coverage_installer.verify_service(infra_dir)
    self.assertFalse(result)

  @patch('code_coverage_installer.run_command')
  def test_setup_infra_gclient_exists_sync_success(self, mock_run):
    infra_dir = pathlib.Path('/infra_dir')
    self.fs.create_file('/infra_dir/.gclient')
    mock_run.return_value = 0

    with patch('sys.stdout') as _:
      result = code_coverage_installer.setup_infra(infra_dir)
    self.assertTrue(result)
    mock_run.assert_called_once_with(['gclient', 'sync'], cwd=infra_dir)

  @patch('code_coverage_installer.run_command')
  def test_setup_infra_gclient_exists_sync_failure(self, mock_run):
    infra_dir = pathlib.Path('/infra_dir')
    self.fs.create_file('/infra_dir/.gclient')
    mock_run.return_value = 1

    with patch('sys.stdout') as _, patch('sys.stderr') as _:
      result = code_coverage_installer.setup_infra(infra_dir)
    self.assertFalse(result)

  @patch('code_coverage_installer.run_command')
  def test_setup_infra_no_gclient_fetch_success(self, mock_run):
    infra_dir = pathlib.Path('/infra_dir')
    mock_run.return_value = 0

    with patch('sys.stdout') as _:
      result = code_coverage_installer.setup_infra(infra_dir)
    self.assertTrue(result)
    mock_run.assert_called_once_with(['fetch', 'infra_superproject'],
                                     cwd=infra_dir)

  @patch('code_coverage_installer.run_command')
  def test_setup_infra_no_gclient_fetch_failure(self, mock_run):
    infra_dir = pathlib.Path('/infra_dir')
    mock_run.return_value = 1

    with patch('sys.stdout') as _, patch('sys.stderr') as _:
      result = code_coverage_installer.setup_infra(infra_dir)
    self.assertFalse(result)

  def test_fs_permission_error_handled(self):
    # Mocking check_exists or path methods to throw PermissionError / OSError
    with patch('pathlib.Path.exists',
               side_effect=PermissionError('Access Denied')):
      with patch('sys.stdout') as _, patch('sys.stderr') as _:
        result = code_coverage_installer.verify_chromium_checkout(self.src_root)
    self.assertFalse(result)

  @patch('sys.argv',
         ['code_coverage_installer.py', '--infra-dir', '/infra_dir'])
  @patch('sys.exit', side_effect=SystemExit)
  @patch('code_coverage_installer.verify_chromium_checkout')
  def test_main_checkout_failed(self, mock_checkout, mock_exit):
    mock_checkout.return_value = False
    with patch('sys.stdout') as _:
      with self.assertRaises(SystemExit):
        code_coverage_installer.main()
    mock_exit.assert_called_once_with(1)

  @patch('sys.argv',
         ['code_coverage_installer.py', '--infra-dir', '/infra_dir'])
  @patch('sys.exit', side_effect=SystemExit)
  @patch('code_coverage_installer.verify_chromium_checkout')
  @patch('code_coverage_installer.verify_llvm_tools')
  def test_main_llvm_failed(self, mock_llvm, mock_checkout, mock_exit):
    mock_checkout.return_value = True
    mock_llvm.return_value = False
    with patch('sys.stdout') as _:
      with self.assertRaises(SystemExit):
        code_coverage_installer.main()
    mock_exit.assert_called_once_with(1)

  @patch('sys.argv',
         ['code_coverage_installer.py', '--infra-dir', '/infra_dir'])
  @patch('sys.exit', side_effect=SystemExit)
  @patch('code_coverage_installer.verify_chromium_checkout')
  @patch('code_coverage_installer.verify_llvm_tools')
  @patch('code_coverage_installer.verify_recipes')
  @patch('code_coverage_installer.verify_service')
  @patch('code_coverage_installer.setup_infra')
  def test_main_already_fully_set_up(self, mock_setup, mock_service,
                                     mock_recipes, mock_llvm, mock_checkout,
                                     mock_exit):
    mock_checkout.return_value = True
    mock_llvm.return_value = True
    mock_recipes.return_value = True
    mock_service.return_value = True

    with patch('sys.stdout') as _:
      code_coverage_installer.main()

    mock_setup.assert_not_called()
    mock_exit.assert_not_called()

  @patch('sys.argv',
         ['code_coverage_installer.py', '--infra-dir', '/infra_dir'])
  @patch('sys.exit', side_effect=SystemExit)
  @patch('code_coverage_installer.verify_chromium_checkout')
  @patch('code_coverage_installer.verify_llvm_tools')
  @patch('code_coverage_installer.verify_recipes')
  @patch('code_coverage_installer.verify_service')
  @patch('code_coverage_installer.setup_infra')
  def test_main_setup_needed_and_succeeds(self, mock_setup, mock_service,
                                          mock_recipes, mock_llvm,
                                          mock_checkout, mock_exit):
    mock_checkout.return_value = True
    mock_llvm.return_value = True
    # First call False (check), second call True (re-verify)
    mock_recipes.side_effect = [False, True]
    mock_service.side_effect = [False, True]
    mock_setup.return_value = True

    with patch('sys.stdout') as _:
      code_coverage_installer.main()

    mock_setup.assert_called_once_with(pathlib.Path('/infra_dir'))
    mock_exit.assert_not_called()

  @patch('sys.argv',
         ['code_coverage_installer.py', '--infra-dir', '/infra_dir'])
  @patch('sys.exit', side_effect=SystemExit)
  @patch('code_coverage_installer.verify_chromium_checkout')
  @patch('code_coverage_installer.verify_llvm_tools')
  @patch('code_coverage_installer.verify_recipes')
  @patch('code_coverage_installer.verify_service')
  @patch('code_coverage_installer.setup_infra')
  def test_main_setup_needed_but_setup_fails(self, mock_setup, mock_service,
                                             mock_recipes, mock_llvm,
                                             mock_checkout, mock_exit):
    mock_checkout.return_value = True
    mock_llvm.return_value = True
    mock_recipes.return_value = False
    mock_service.return_value = False
    mock_setup.return_value = False

    with patch('sys.stdout') as _:
      with self.assertRaises(SystemExit):
        code_coverage_installer.main()

    mock_setup.assert_called_once_with(pathlib.Path('/infra_dir'))
    mock_exit.assert_called_once_with(1)

  @patch('sys.argv',
         ['code_coverage_installer.py', '--infra-dir', '/infra_dir'])
  @patch('sys.exit', side_effect=SystemExit)
  @patch('code_coverage_installer.verify_chromium_checkout')
  @patch('code_coverage_installer.verify_llvm_tools')
  @patch('code_coverage_installer.verify_recipes')
  @patch('code_coverage_installer.verify_service')
  @patch('code_coverage_installer.setup_infra')
  def test_main_setup_succeeds_but_re_verification_fails(
      self, mock_setup, mock_service, mock_recipes, mock_llvm, mock_checkout,
      mock_exit):
    mock_checkout.return_value = True
    mock_llvm.return_value = True
    # First call False, second call False
    mock_recipes.side_effect = [False, False]
    mock_service.side_effect = [False, True]
    mock_setup.return_value = True

    with patch('sys.stdout') as _, patch('sys.stderr') as _:
      with self.assertRaises(SystemExit):
        code_coverage_installer.main()

    mock_setup.assert_called_once_with(pathlib.Path('/infra_dir'))
    mock_exit.assert_called_once_with(1)


if __name__ == '__main__':
  unittest.main()
