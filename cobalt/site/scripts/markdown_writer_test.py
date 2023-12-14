#!/usr/bin/python
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
"""Tests the markdown_writer module."""

import contextlib
from io import StringIO
import textwrap
import unittest

from cobalt.site.scripts import markdown_writer

_LOREM_IPSUM_1 = ('Lorem ipsum dolor sit amet, consectetur adipiscing elit, '
                  'sed do eiusmod tempor incididunt ut labore et dolore magna '
                  'aliqua.')
_LOREM_IPSUM_2 = ('Ut enim ad minim veniam, quis nostrud exercitation '
                  'ullamco laboris nisi ut aliquip ex ea commodo consequat. '
                  'Duis aute irure dolor in reprehenderit in voluptate velit '
                  'esse cillum dolore eu fugiat nulla pariatur.')
_LOREM_IPSUM_3 = ('Excepteur sint occaecat cupidatat non proident, sunt in '
                  'culpa qui officia deserunt mollit anim id est laborum.')

_LOREM_IPSUM = f'{_LOREM_IPSUM_1} {_LOREM_IPSUM_2} {_LOREM_IPSUM_3}'


def _indent(text, size):
  return _indent_behind(text, ' ' * size)


def _indent_behind(text, prefix):
  if not text:
    return text

  indentation = '\n' + (' ' * len(prefix))
  result = prefix + indentation.join(text.split('\n'))
  return result


def _wrap_behind(text, prefix, width):
  if not text:
    return text
  return _indent_behind(textwrap.fill(text, width - len(prefix)), prefix)


class MarkdownWriterTest(unittest.TestCase):
  """Tests the markdown_writer module."""

  def testBasic(self):
    with contextlib.closing(StringIO()) as test_io:
      out = markdown_writer.MarkdownWriter(test_io)
      out.text(_LOREM_IPSUM_1)
      out.text(_LOREM_IPSUM_2)
      out.text(_LOREM_IPSUM_3)
      out.text(_LOREM_IPSUM_1)
      out.text(_LOREM_IPSUM_2)
      out.text(_LOREM_IPSUM_3)
      actual = test_io.getvalue()

    expected = textwrap.fill(_LOREM_IPSUM + ' ' + _LOREM_IPSUM, 80)
    self.assertEqual(expected, actual)

  def testParagraph(self):
    with contextlib.closing(StringIO()) as test_io:
      out = markdown_writer.MarkdownWriter(test_io)
      out.text(_LOREM_IPSUM_1)
      out.text(_LOREM_IPSUM_2)
      out.paragraph()
      out.text(_LOREM_IPSUM_3)
      out.text(_LOREM_IPSUM_1)
      out.paragraph()
      out.text(_LOREM_IPSUM_2)
      out.text(_LOREM_IPSUM_3)
      actual = test_io.getvalue()

    expected = (
        textwrap.fill(_LOREM_IPSUM_1 + ' ' + _LOREM_IPSUM_2, 80) + '\n\n' +
        textwrap.fill(_LOREM_IPSUM_3 + ' ' + _LOREM_IPSUM_1, 80) + '\n\n' +
        textwrap.fill(_LOREM_IPSUM_2 + ' ' + _LOREM_IPSUM_3, 80))
    self.assertEqual(expected, actual)

  def testListDepth1(self):
    with contextlib.closing(StringIO()) as test_io:
      out = markdown_writer.MarkdownWriter(test_io)
      out.paragraph()
      out.paragraph()
      out.paragraph()
      with out.auto_ordered_list():
        with out.auto_item():
          out.text(_LOREM_IPSUM_1)
          out.text(_LOREM_IPSUM_2)
          out.paragraph()
          out.paragraph()
        with out.auto_item():
          out.paragraph()
          out.text(_LOREM_IPSUM_3)
          out.text(_LOREM_IPSUM_1)
        with out.auto_item():
          out.text(_LOREM_IPSUM_2)
          out.paragraph()
          out.text(_LOREM_IPSUM_3)
          out.paragraph()
      out.text(_LOREM_IPSUM_1)
      actual = test_io.getvalue()
    list_prefix = '1.  '
    expected = (
        _wrap_behind(_LOREM_IPSUM_1 + ' ' + _LOREM_IPSUM_2, list_prefix, 80) +
        '\n\n' +
        _wrap_behind(_LOREM_IPSUM_3 + ' ' + _LOREM_IPSUM_1, list_prefix, 80) +
        '\n' + _wrap_behind(_LOREM_IPSUM_2, list_prefix, 80) + '\n\n' +
        _wrap_behind(_LOREM_IPSUM_3, ' ' * len(list_prefix), 80) + '\n\n' +
        textwrap.fill(_LOREM_IPSUM_1, 80))
    self.assertEqual(expected, actual)

  def testListDepth2(self):
    with contextlib.closing(StringIO()) as test_io:
      out = markdown_writer.MarkdownWriter(test_io)
      with out.auto_ordered_list():
        with out.auto_item():
          out.text(_LOREM_IPSUM_1)
          with out.auto_unordered_list():
            with out.auto_item():
              out.text(_LOREM_IPSUM_2)
            with out.auto_item():
              out.text(_LOREM_IPSUM_3)
              out.text(_LOREM_IPSUM_1)
        with out.auto_item():
          out.text(_LOREM_IPSUM_2)
          with out.auto_unordered_list():
            with out.auto_item():
              out.text(_LOREM_IPSUM_3)
              out.text(_LOREM_IPSUM_1)
            with out.auto_item():
              out.text(_LOREM_IPSUM_2)
      out.text(_LOREM_IPSUM_3)
      actual = test_io.getvalue()
    list_prefix_1 = '1.  '
    list_prefix_2 = '    *   '
    expected = (
        _wrap_behind(_LOREM_IPSUM_1, list_prefix_1, 80) + '\n\n' +
        _wrap_behind(_LOREM_IPSUM_2, list_prefix_2, 80) + '\n' +
        _wrap_behind(_LOREM_IPSUM_3 + ' ' + _LOREM_IPSUM_1, list_prefix_2, 80) +
        '\n\n' + _wrap_behind(_LOREM_IPSUM_2, list_prefix_1, 80) + '\n\n' +
        _wrap_behind(_LOREM_IPSUM_3 + ' ' + _LOREM_IPSUM_1, list_prefix_2, 80) +
        '\n' + _wrap_behind(_LOREM_IPSUM_2, list_prefix_2, 80) + '\n\n' +
        textwrap.fill(_LOREM_IPSUM_3, 80))
    self.assertEqual(expected, actual)


if __name__ == '__main__':
  unittest.main()
