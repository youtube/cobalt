# Copyright 2012 Google Inc.
#
# http://www.logilab.fr/ -- mailto:contact@logilab.fr
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
"""Checkers for various standard library functions."""

import re
import six
import sys

import astroid
from astroid.bases import Instance

from pylint.interfaces import IAstroidChecker
from pylint.checkers import BaseChecker
from pylint.checkers import utils


if sys.version_info >= (3, 0):
    OPEN_MODULE = '_io'
else:
    OPEN_MODULE = '__builtin__'


def _check_mode_str(mode):
    # check type
    if not isinstance(mode, six.string_types):
        return False
    # check syntax
    modes = set(mode)
    _mode = "rwatb+U"
    creating = False
    if six.PY3:
        _mode += "x"
        creating = "x" in modes
    if modes - set(_mode) or len(mode) > len(modes):
        return False
    # check logic
    reading = "r" in modes
    writing = "w" in modes
    appending = "a" in modes
    updating = "+" in modes
    text = "t" in modes
    binary = "b" in modes
    if "U" in modes:
        if writing or appending or creating and six.PY3:
            return False
        reading = True
        if not six.PY3:
            binary = True
    if text and binary:
        return False
    total = reading + writing + appending + (creating if six.PY3 else 0)
    if total > 1:
        return False
    if not (reading or writing or appending or creating and six.PY3):
        return False
    # other 2.x constraints
    if not six.PY3:
        if "U" in mode:
            mode = mode.replace("U", "")
            if "r" not in mode:
                mode = "r" + mode
        return mode[0] in ("r", "w", "a", "U")
    return True


class StdlibChecker(BaseChecker):
    __implements__ = (IAstroidChecker,)
    name = 'stdlib'

    msgs = {
        'W1501': ('"%s" is not a valid mode for open.',
                  'bad-open-mode',
                  'Python supports: r, w, a[, x] modes with b, +, '
                  'and U (only with r) options. '
                  'See http://docs.python.org/2/library/functions.html#open'),
        'W1502': ('Using datetime.time in a boolean context.',
                  'boolean-datetime',
                  'Using datetetime.time in a boolean context can hide '
                  'subtle bugs when the time they represent matches '
                  'midnight UTC. This behaviour was fixed in Python 3.5. '
                  'See http://bugs.python.org/issue13936 for reference.',
                  {'maxversion': (3, 5)}),
        'W1503': ('Redundant use of %s with constant '
                  'value %r',
                  'redundant-unittest-assert',
                  'The first argument of assertTrue and assertFalse is'
                  'a condition. If a constant is passed as parameter, that'
                  'condition will be always true. In this case a warning '
                  'should be emitted.')
    }

    @utils.check_messages('bad-open-mode', 'redundant-unittest-assert')
    def visit_callfunc(self, node):
        """Visit a CallFunc node."""
        if hasattr(node, 'func'):
            infer = utils.safe_infer(node.func)
            if infer:
                if infer.root().name == OPEN_MODULE:
                    if getattr(node.func, 'name', None) in ('open', 'file'):
                        self._check_open_mode(node)
                if infer.root().name == 'unittest.case':
                    self._check_redundant_assert(node, infer)

    @utils.check_messages('boolean-datetime')
    def visit_unaryop(self, node):
        if node.op == 'not':
            self._check_datetime(node.operand)

    @utils.check_messages('boolean-datetime')
    def visit_if(self, node):
        self._check_datetime(node.test)

    @utils.check_messages('boolean-datetime')
    def visit_ifexp(self, node):
        self._check_datetime(node.test)

    @utils.check_messages('boolean-datetime')
    def visit_boolop(self, node):
        for value in node.values:
            self._check_datetime(value)

    def _check_redundant_assert(self, node, infer):
        if (isinstance(infer, astroid.BoundMethod) and
                node.args and isinstance(node.args[0], astroid.Const) and
                infer.name in ['assertTrue', 'assertFalse']):
            self.add_message('redundant-unittest-assert',
                             args=(infer.name, node.args[0].value, ),
                             node=node)

    def _check_datetime(self, node):
        """ Check that a datetime was infered.
        If so, emit boolean-datetime warning.
        """
        try:
            infered = next(node.infer())
        except astroid.InferenceError:
            return
        if (isinstance(infered, Instance) and
                infered.qname() == 'datetime.time'):
            self.add_message('boolean-datetime', node=node)


    def _check_open_mode(self, node):
        """Check that the mode argument of an open or file call is valid."""
        try:
            mode_arg = utils.get_argument_from_call(node, position=1,
                                                    keyword='mode')
        except utils.NoSuchArgumentError:
            return
        if mode_arg:
            mode_arg = utils.safe_infer(mode_arg)
            if (isinstance(mode_arg, astroid.Const)
                    and not _check_mode_str(mode_arg.value)):
                self.add_message('bad-open-mode', node=node,
                                 args=mode_arg.value)


def register(linter):
    """required method to auto register this checker """
    linter.register_checker(StdlibChecker(linter))
