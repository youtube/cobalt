#
# Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
#

"""Unit tests for FlattenedInterface class."""

import os
import platform
import unittest

import _env  # pylint: disable=unused-import
import flatten_idls


def _TestDataPath():
  return os.path.join(os.path.dirname(__file__), 'testdata')


def _TestDataPathGenerator(files):
  test_data_path = _TestDataPath()
  for f in files:
    yield os.path.join(test_data_path, f)


@unittest.skip('Test has bitrotted.')
class FlattenedInterfacesTest(unittest.TestCase):

  def testFlattenIdl(self):
    test_idls = ['TestInterface.idl']
    result = flatten_idls._FlattenInterfaces(_TestDataPathGenerator(test_idls))
    self.assertEqual(1, len(result))
    self.assertEqual('TestInterface', result[0].name)
    self.assertItemsEqual(['peach', 'banana'], result[0].attributes)
    self.assertItemsEqual(['Tomato'], result[0].operations)
    self.assertItemsEqual(['PORK_CHOP'], result[0].constants)
    self.assertItemsEqual(['TestInterface'], result[0].constructors)

  def testNoInterfaceObject(self):
    test_idls = ['NoInterfaceObject.idl']
    result = flatten_idls._FlattenInterfaces(_TestDataPathGenerator(test_idls))
    self.assertEqual(1, len(result))
    self.assertEqual('NoInterfaceObject', result[0].name)

  def testNamedConstructor(self):
    test_idls = ['NamedConstructorInterface.idl']
    result = flatten_idls._FlattenInterfaces(_TestDataPathGenerator(test_idls))
    self.assertEqual(1, len(result))
    self.assertEqual('NamedConstructorInterface', result[0].name)
    self.assertItemsEqual(
        ['Apple', 'NamedConstructorInterface'], result[0].constructors)

  def testNamedConstructorWithNoInterfaceObject(self):
    test_idls = ['NamedConstructorInterfaceWithNoInterfaceObject.idl']
    result = flatten_idls._FlattenInterfaces(_TestDataPathGenerator(test_idls))
    self.assertEqual(1, len(result))
    self.assertEqual('NamedConstructorInterfaceWithNoInterfaceObject',
                     result[0].name)
    self.assertItemsEqual(['Orange'], result[0].constructors)

  def testPartialInterface(self):
    test_idls = ['TestInterface.idl', 'TestInterface_partial.idl']
    result = flatten_idls._FlattenInterfaces(_TestDataPathGenerator(test_idls))
    self.assertEqual(1, len(result))
    self.assertEqual('TestInterface', result[0].name)
    self.assertItemsEqual(['peach', 'banana', 'blueberry'],
                          result[0].attributes)

  def testImplementedInterface(self):
    test_idls = ['TestInterface.idl', 'ImplementedInterface.idl']
    result = flatten_idls._FlattenInterfaces(_TestDataPathGenerator(test_idls))
    self.assertEqual(1, len(result))
    self.assertEqual('TestInterface', result[0].name)
    self.assertItemsEqual(['Tomato', 'Onion'], result[0].operations)


class FlattenedInterfaceDifferenceTest(unittest.TestCase):

  def testAssertsOnDifferentInterfaces(self):
    lhs = flatten_idls.FlattenedInterface(name='Toyota',
                                          operations=[],
                                          attributes=[],
                                          constants=[],
                                          constructors=[])
    rhs = flatten_idls.FlattenedInterface(name='Honda',
                                          operations=[],
                                          attributes=[],
                                          constants=[],
                                          constructors=[])
    with self.assertRaises(AssertionError):
      flatten_idls.FlattenedInterface.Difference(lhs, rhs)

  def testDifference(self):
    lhs = flatten_idls.FlattenedInterface(
        name='Bunny',
        operations=['Hop', 'Skip'],
        attributes=['wheels'],
        constants=['NUM_EARS', 'NUM_FEET', 'NUM_EYES'],
        constructors=[])
    rhs = flatten_idls.FlattenedInterface(
        name='Bunny',
        operations=['Hop', 'Jump'],
        attributes=['wheels'],
        constants=['NUM_FEET', 'NUM_EYES', 'NUM_TEETH'],
        constructors=[])
    diff = flatten_idls.FlattenedInterface.Difference(lhs, rhs)
    self.assertEqual('Bunny', diff.name)
    self.assertFalse(diff.attributes)
    self.assertItemsEqual(['Skip'], diff.operations)
    self.assertItemsEqual(['NUM_EARS'], diff.constants)
    self.assertFalse(diff.constructors)


if __name__ == '__main__':
  if platform.system() == 'Linux':
    unittest.main()
