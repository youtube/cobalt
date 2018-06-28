#!/usr/bin/env python
# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

import unittest

import _env  # pylint: disable=unused-import
from cobalt.bindings.path_generator import PathBuilder


@unittest.skip('Test has bitrotted.')
class PathBuilderTest(unittest.TestCase):

  def setUp(self):
    self.interfaces_info = {
        'TestInterface': {
            'full_path': '/some/path/root/this/is/a/test_interface.idl'
        }
    }
    self.path_builder = PathBuilder('pre', self.interfaces_info,
                                    '/some/path/root', '/path/to/generated')

  def testNamespaceComponents(self):
    self.assertListEqual(
        self.path_builder.NamespaceComponents('TestInterface'),
        ['root', 'this', 'is', 'a'])

  def testBindingsClass(self):
    self.assertEqual(
        self.path_builder.BindingsClass('TestInterface'), 'PreTestInterface')

  def testFullBindingsClass(self):
    self.assertEqual(
        self.path_builder.FullBindingsClassName('TestInterface'),
        'root::this::is::a::PreTestInterface')

  def testFullClass(self):
    self.assertEqual(
        self.path_builder.FullClassName('TestInterface'),
        'root::this::is::a::TestInterface')

  def testImplementationHeaderIncludePath(self):
    self.assertEqual(
        self.path_builder.ImplementationHeaderPath('TestInterface'),
        'root/this/is/a/test_interface.h')

  def testBindingsHeaderIncludePath(self):
    self.assertEqual(
        self.path_builder.BindingsHeaderIncludePath('TestInterface'),
        'root/this/is/a/pre_test_interface.h')

  def testBindingsHeaderFullPath(self):
    self.assertEqual(
        self.path_builder.BindingsHeaderFullPath('TestInterface'),
        '/path/to/generated/root/this/is/a/pre_test_interface.h')

  def testBindingsImplementationPath(self):
    self.assertEqual(
        self.path_builder.BindingsImplementationPath('TestInterface'),
        '/path/to/generated/root/this/is/a/pre_test_interface.cc')

  def testDictionaryHeaderIncludePath(self):
    self.assertEqual(
        self.path_builder.DictionaryHeaderIncludePath('TestInterface'),
        'root/this/is/a/test_interface.h')

  def testDictionaryHeaderFullPath(self):
    self.assertEqual(
        self.path_builder.DictionaryHeaderFullPath('TestInterface'),
        '/path/to/generated/root/this/is/a/test_interface.h')

  def testDictionaryConversionHeaderIncludePath(self):
    self.assertEqual(
        self.path_builder.BindingsHeaderIncludePath('TestInterface'),
        'root/this/is/a/pre_test_interface.h')

  def testDictionaryConversionHeaderFullPath(self):
    self.assertEqual(
        self.path_builder.BindingsHeaderFullPath('TestInterface'),
        '/path/to/generated/root/this/is/a/pre_test_interface.h')


if __name__ == '__main__':
  unittest.main()
