# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=import-error,print-statement,relative-import,protected-access

"""Unit tests for name_style_converter.py."""

import unittest

from name_style_converter import NameStyleConverter
from name_style_converter import SmartTokenizer


class SmartTokenizerTest(unittest.TestCase):
    def test_simple_cases(self):
        tokenizer = SmartTokenizer('foo')
        self.assertEqual(tokenizer.tokenize(), ['foo'])

        tokenizer = SmartTokenizer('fooBar')
        self.assertEqual(tokenizer.tokenize(), ['foo', 'Bar'])

        tokenizer = SmartTokenizer('fooBarBaz')
        self.assertEqual(tokenizer.tokenize(), ['foo', 'Bar', 'Baz'])

        tokenizer = SmartTokenizer('Baz')
        self.assertEqual(tokenizer.tokenize(), ['Baz'])

        tokenizer = SmartTokenizer('')
        self.assertEqual(tokenizer.tokenize(), [])

        tokenizer = SmartTokenizer('FOO')
        self.assertEqual(tokenizer.tokenize(), ['FOO'])

        tokenizer = SmartTokenizer('foo2')
        self.assertEqual(tokenizer.tokenize(), ['foo', '2'])

    def test_tricky_cases(self):
        tokenizer = SmartTokenizer('XMLHttpRequest')
        self.assertEqual(tokenizer.tokenize(), ['XML', 'Http', 'Request'])

        tokenizer = SmartTokenizer('HTMLElement')
        self.assertEqual(tokenizer.tokenize(), ['HTML', 'Element'])

        tokenizer = SmartTokenizer('WebGLRenderingContext')
        self.assertEqual(tokenizer.tokenize(),
                         ['WebGL', 'Rendering', 'Context'])

        tokenizer = SmartTokenizer('CanvasRenderingContext2D')
        self.assertEqual(tokenizer.tokenize(),
                         ['Canvas', 'Rendering', 'Context', '2D'])

        tokenizer = SmartTokenizer('SVGSVGElement')
        self.assertEqual(tokenizer.tokenize(), ['SVG', 'SVG', 'Element'])


class NameStyleConverterTest(unittest.TestCase):
    def test_snake_case(self):
        converter = NameStyleConverter('HTMLElement')
        self.assertEqual(converter.to_snake_case(), 'html_element')

    def test_upper_camel_case(self):
        converter = NameStyleConverter('someSuperThing')
        self.assertEqual(converter.to_upper_camel_case(), 'SomeSuperThing')

        converter = NameStyleConverter('SVGElement')
        self.assertEqual(converter.to_upper_camel_case(), 'SVGElement')

    def test_macro_case(self):
        converter = NameStyleConverter('WebGLBaz2D')
        self.assertEqual(converter.to_macro_case(), 'WEBGL_BAZ_2D')

    def test_all_cases(self):
        converter = NameStyleConverter('SVGScriptElement')
        self.assertEqual(converter.to_all_cases(), {
            'snake_case': 'svg_script_element',
            'upper_camel_case': 'SVGScriptElement',
            'macro_case': 'SVG_SCRIPT_ELEMENT',
        })
