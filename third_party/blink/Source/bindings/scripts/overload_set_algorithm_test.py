# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=import-error,print-statement,relative-import,protected-access

"""Unit tests for overload_set_algorithm.py."""

import unittest

from overload_set_algorithm import effective_overload_set
from overload_set_algorithm import MethodContextAdapter


class MethodContextAdapterTest(unittest.TestCase):
    def test_simple(self):
        adapter = MethodContextAdapter()
        self.assertEqual(adapter.arguments({'arguments': 'foo'}), 'foo')
        self.assertEqual(adapter.type({'idl_type_object': 'bar'}), 'bar')
        self.assertEqual(adapter.is_optional({'is_optional': 'baz'}), 'baz')
        self.assertEqual(adapter.is_variadic({'is_variadic': 'qux'}), 'qux')


class EffectiveOverloadSetTest(unittest.TestCase):
    def test_example_in_comments(self):
        operation_list = [
            {'arguments': [{'idl_type_object': 'long',  # f1(optional long x)
                            'is_optional': True,
                            'is_variadic': False}]},
            {'arguments': [{'idl_type_object': 'DOMString',  # f2(DOMString s)
                            'is_optional': False,
                            'is_variadic': False}]},
        ]

        overload_set = [
            ({'arguments': [{'idl_type_object': 'long',  # f1(long)
                             'is_optional': True,
                             'is_variadic': False}]},
             ('long',),
             (True,)),
            ({'arguments': [{'idl_type_object': 'long',  # f1()
                             'is_optional': True,
                             'is_variadic': False}]},
             (),
             ()),
            ({'arguments': [{'idl_type_object': 'DOMString',  # f2(DOMString)
                             'is_optional': False,
                             'is_variadic': False}]},
             ('DOMString',),
             (False,))]

        self.assertEqual(
            effective_overload_set(operation_list, MethodContextAdapter()),
            overload_set)
