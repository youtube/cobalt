# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import absolute_import
import string

from telemetry.internal.actions import page_action


# Map from DOM key values
# (https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/key) to
# Windows virtual key codes
# (https://cs.chromium.org/chromium/src/third_party/WebKit/Source/platform/WindowsKeyboardCodes.h)
# and their printed representations (if available).
_KEY_MAP = {}

def _AddSpecialKey(key, windows_virtual_key_code, text=None):
  assert key not in _KEY_MAP, 'Duplicate key: %s' % key
  _KEY_MAP[key] = (windows_virtual_key_code, text)

def _AddRegularKey(keys, windows_virtual_key_code):
  for k in keys:
    assert k not in _KEY_MAP, 'Duplicate key: %s' % k
    _KEY_MAP[k] = (windows_virtual_key_code, k)

def GetKey(key_name):
  return _KEY_MAP.get(key_name)

_AddSpecialKey('PageUp', 0x21)
_AddSpecialKey('PageDown', 0x22)
_AddSpecialKey('End', 0x23)
_AddSpecialKey('Home', 0x24)
_AddSpecialKey('ArrowLeft', 0x25)
_AddSpecialKey('ArrowUp', 0x26)
_AddSpecialKey('ArrowRight', 0x27)
_AddSpecialKey('ArrowDown', 0x28)
_AddSpecialKey('Esc', 0x1B)

_AddSpecialKey('Return', 0x0D, text='\x0D')
_AddSpecialKey('Delete', 0x2E, text='\x7F')
_AddSpecialKey('Backspace', 0x08, text='\x08')
_AddSpecialKey('Tab', 0x09, text='\x09')

# Letter keys.
for c in string.ascii_uppercase:
  _AddRegularKey([c, c.lower()], ord(c))

# Symbol keys.
_AddRegularKey(';:', 0xBA)
_AddRegularKey('=+', 0xBB)
_AddRegularKey(',<', 0xBC)
_AddRegularKey('-_', 0xBD)
_AddRegularKey('.>', 0xBE)
_AddRegularKey('/?', 0xBF)
_AddRegularKey('`~', 0xC0)
_AddRegularKey('[{', 0xDB)
_AddRegularKey('\\|', 0xDC)
_AddRegularKey(']}', 0xDD)
_AddRegularKey('\'"', 0xDE)

# Numeric keys.
_AddRegularKey('0)', 0x30)
_AddRegularKey('1!', 0x31)
_AddRegularKey('2@', 0x32)
_AddRegularKey('3#', 0x33)
_AddRegularKey('4$', 0x34)
_AddRegularKey('5%', 0x35)
_AddRegularKey('6^', 0x36)
_AddRegularKey('7&', 0x37)
_AddRegularKey('8*', 0x38)
_AddRegularKey('9(', 0x39)

# Space.
_AddRegularKey(' ', 0x20)


class KeyPressAction(page_action.PageAction):

  def __init__(self, dom_key, timeout=page_action.DEFAULT_TIMEOUT):
    super().__init__(timeout=timeout)
    char_code = 0 if len(dom_key) > 1 else ord(dom_key)
    self._dom_key = dom_key
    # Check that ascii chars are allowed.
    use_key_map = len(dom_key) > 1 or char_code < 128
    if use_key_map and dom_key not in _KEY_MAP:
      raise ValueError('No mapping for key: %s (code=%s)' % (
          dom_key, char_code))
    self._windows_virtual_key_code, self._text = _KEY_MAP.get(
        dom_key, ('', dom_key))

  def RunAction(self, tab):
    # Note that this action does not handle self.timeout properly. Since each
    # command gets the whole timeout, the PageAction can potentially
    # take three times as long as it should.
    tab.DispatchKeyEvent(
        key_event_type='rawKeyDown',
        dom_key=self._dom_key,
        windows_virtual_key_code=self._windows_virtual_key_code,
        timeout=self.timeout)
    if self._text:
      tab.DispatchKeyEvent(
          key_event_type='char',
          text=self._text,
          dom_key=self._dom_key,
          windows_virtual_key_code=ord(self._text),
          timeout=self.timeout)
    tab.DispatchKeyEvent(
        key_event_type='keyUp',
        dom_key=self._dom_key,
        windows_virtual_key_code=self._windows_virtual_key_code,
        timeout=self.timeout)

  def __str__(self):
    return "%s('%s')" % (self.__class__.__name__, self._dom_key)
