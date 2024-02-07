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
"""Provides an interface to produce valid, readable Markdown documents."""

import contextlib
import logging
import textwrap

_CONTEXT_NONE = 'none'
_CONTEXT_LINK = 'link'
_CONTEXT_LIST = 'list'
_CONTEXT_ITEM = 'item'
_CONTEXT_HEADING = 'heading'

_LIST_TYPE_ORDERED = 'ordered'
_LIST_TYPE_UNORDERED = 'unordered'

# Ready to start a new parapgrah
_STATE_BASE = 'base'

# On an empty line with a paragraph on the previous line.
_STATE_EMPTY = 'empty'

# Inside a partial text block.
_STATE_INLINE = 'inline'


def _strip(string_or_none):
  if string_or_none:
    return string_or_none.strip()
  return ''


def _stripwrap(body, width):
  return textwrap.fill(
      _strip(body), width, break_long_words=False, replace_whitespace=False)


def _get_list_prefix(list_type, depth):
  list_marker = '1.' if list_type == _LIST_TYPE_ORDERED else '*'
  return f'{"    " * depth}{list_marker:4.4}'


class _Context(object):
  """Stores info about current context in the document."""

  def __init__(self, context_type, url=None, list_type=None, pending=False):
    self.context_type = context_type
    self.url = url
    self.list_type = list_type
    self.pending = pending

  def test_and_clear_pending_list(self):
    if self.context_type == _CONTEXT_ITEM and self.pending:
      self.pending = False
      return True
    return False


class MarkdownWriter(object):
  """Stateful wrapper for a file-like object to generate Markdown."""

  def __init__(self, out, width=80, heading_level=0):
    assert heading_level >= 0
    self.out = out
    self.width = width
    self.base_heading_level = heading_level
    self.heading_stack = []
    self.context_stack = [_Context(_CONTEXT_NONE)]
    self.state = _STATE_BASE
    self.current_line = ''

  def heading(self, levels=1):
    logging.debug('MdW: heading(%d)', levels)
    assert levels >= 1
    self.context_stack.append(_Context(_CONTEXT_HEADING))
    self.heading_stack.append(levels)
    self._return_to_base()
    self.text(('#' * self._get_heading_level()) + ' ', wrap=False)
    assert self._get_heading_level() >= 1

  def end_heading(self):
    logging.debug('MdW: end_heading()')
    assert self._get_context_type() == _CONTEXT_HEADING
    self.context_stack.pop()
    self._return_to_empty()

  def pop_heading_level(self):
    logging.debug('MdW: pop_heading_level()')
    self.heading_stack.pop()
    assert self._get_heading_level() >= 0

  def code_block(self, contents, language=None):
    logging.debug('MdW: code_block(%s, %s)', contents.replace('\n', '\\n'),
                  language)
    self._return_to_base()
    if not language:
      language = ''
    self.text(f'```{language}\n{contents}\n```\n\n', wrap=False)
    self._set_state(_STATE_BASE, 'code block')

  def ordered_list(self):
    logging.debug('MdW: ordered_list()')
    self._return_to_base()
    self.context_stack.append(
        _Context(_CONTEXT_LIST, list_type=_LIST_TYPE_ORDERED))

  def unordered_list(self):
    logging.debug('MdW: unordered_list()')
    self._return_to_base()
    self.context_stack.append(
        _Context(_CONTEXT_LIST, list_type=_LIST_TYPE_UNORDERED))

  def end_list(self):
    logging.debug('MdW: end_list()')
    assert self._get_context_type() == _CONTEXT_LIST
    self.context_stack.pop()
    self._return_to_base()

  def item(self):
    logging.debug('MdW: item()')
    assert self._get_context_type() == _CONTEXT_LIST
    self.context_stack.append(_Context(_CONTEXT_ITEM, pending=True))

  def end_item(self):
    logging.debug('MdW: end_item()')
    assert self._get_context_type() == _CONTEXT_ITEM
    self._return_to_empty()
    self.context_stack.pop()

  def link(self, url):
    logging.debug('MdW: link(%s)', url)
    assert url
    self.context_stack.append(_Context(_CONTEXT_LINK, url=url))
    self.text('[', wrap=True)

  def end_link(self):
    logging.debug('MdW: end_link()')
    context = self._get_context()
    assert context.context_type == _CONTEXT_LINK
    self.text(f']({context.url})', wrap=False)
    self.context_stack.pop()

  def paragraph(self):
    logging.debug('MdW: paragraph()')
    self._return_to_base()

  def end_paragraph(self):
    logging.debug('MdW: end_paragraph()')
    self._return_to_empty()

  def code(self, contents, wrap=None):
    self._style_text(contents.replace('`', '``'), '`', wrap)

  def bold(self, contents, wrap=None):
    self._style_text(contents, '**', wrap)

  def highlight(self, contents, wrap=None):
    self._style_text(contents, '==', wrap)

  def italics(self, contents, wrap=None):
    self._style_text(contents, '*', wrap)

  def strikeout(self, contents, wrap=None):
    self._style_text(contents, '~~', wrap)

  def text(self, contents, wrap=None):
    logging.debug('MdW: text(%s, %s)', contents.replace('\n', '\\n'), str(wrap))
    if not contents or not contents.strip():
      return

    wrap = self._default_wrap(wrap)
    if wrap:
      contents = contents.strip()
      if self.current_line and self.current_line[-1] != ' ':
        contents = ' ' + contents
      if not contents:
        return
      current_line_length = len(self.current_line)
      contents = textwrap.fill(
          self.current_line + contents,
          self._get_wrap_width(),
          break_long_words=False)
      contents = contents[current_line_length:]

    lines = contents.split('\n')
    if not lines:
      return

    if self.current_line or self.state in [_STATE_BASE, _STATE_EMPTY]:
      if self._get_context().test_and_clear_pending_list():
        self._return_to_empty()
        self.out.write(self._get_list_prefix())
        self._set_state(_STATE_INLINE, 'list prefix')
      elif self.state in [_STATE_BASE, _STATE_EMPTY]:
        indent = self._get_indent()
        if indent:
          self.out.write(indent)
          self._set_state(_STATE_INLINE, 'indentation')
      self.out.write(lines[0])
      self.current_line += lines[0]
      self._set_state(_STATE_INLINE, 'first line')
      del lines[0]
      if lines:
        self.out.write('\n')
        self.current_line = ''
        self._set_state(_STATE_EMPTY, 'more lines')

    if lines:
      indent = self._get_indent()
      conjunction = '\n' + indent
      self.out.write(indent + conjunction.join(lines))
      self.current_line = lines[-1]

    if not self.current_line:
      self._set_state(_STATE_EMPTY, 'no current line')
    else:
      self._set_state(_STATE_INLINE, 'current line')

  @contextlib.contextmanager
  def auto_heading(self, levels=1):
    self.heading(levels=levels)
    yield
    self.end_heading()

  @contextlib.contextmanager
  def auto_scoped_heading(self, text, levels=1):
    with self.auto_heading(levels):
      self.text(text)
    yield
    self.pop_heading_level()

  @contextlib.contextmanager
  def auto_paragraph(self):
    self.paragraph()
    yield
    self.end_paragraph()

  @contextlib.contextmanager
  def auto_link(self, url):
    self.link(url)
    yield
    self.end_link()

  @contextlib.contextmanager
  def auto_unordered_list(self):
    self.unordered_list()
    yield
    self.end_list()

  @contextlib.contextmanager
  def auto_ordered_list(self):
    self.ordered_list()
    yield
    self.end_list()

  @contextlib.contextmanager
  def auto_item(self):
    self.item()
    yield
    self.end_item()

  def _get_list_context(self):
    for context in reversed(self.context_stack):
      if context.context_type == _CONTEXT_LIST:
        return context
    return None

  def _get_list_type(self):
    context = self._get_list_context()
    if not context:
      return None
    return context.list_type

  def _get_list_depth(self):
    depth = 0
    for context in self.context_stack:
      if context.context_type == _CONTEXT_LIST:
        depth += 1
    if not depth:
      return 0
    return depth

  def _get_context_type(self):
    context = self._get_context()
    if not context:
      return None
    return context.context_type

  def _get_context(self):
    if not self.context_stack:
      return None
    return self.context_stack[-1]

  def _style_text(self, contents, style, wrap):
    if not contents or not contents.strip():
      return
    self.text(f'{style}{contents}{style}', wrap)

  def _return_to_empty(self):
    if self.state in [_STATE_BASE, _STATE_EMPTY]:
      return

    if self.state == _STATE_INLINE:
      self.out.write('\n')
      self.current_line = ''
      self._set_state(_STATE_EMPTY)
      return

    assert False

  def _return_to_base(self):
    self._return_to_empty()

    if self.state == _STATE_BASE:
      return

    if self.state == _STATE_EMPTY:
      self.out.write('\n')
      self._set_state(_STATE_BASE)
      return

    assert False

  def _set_state(self, state, label=''):
    if self.state != state:
      logging.debug('MdW: STATE%s: %s -> %s',
                    '(' + label + ')' if label else '', self.state, state)
    self.state = state

  def _get_indent(self):
    return (' ' * 4) * self._get_list_depth()

  def _get_heading_level(self):
    return self.base_heading_level + sum(self.heading_stack)

  def _get_list_prefix(self):
    return _get_list_prefix(self._get_list_type(), self._get_list_depth() - 1)

  def _get_wrap_width(self):
    return self.width - self._get_list_depth() * 4

  def _default_wrap(self, wrap):
    if wrap is not None:
      return wrap
    if self._get_context_type() in [_CONTEXT_LINK, _CONTEXT_HEADING]:
      return False
    return True
