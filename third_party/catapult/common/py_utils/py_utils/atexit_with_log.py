# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import absolute_import
import atexit
import logging


def _WrapFunction(function):
  def _WrappedFn(*args, **kwargs):
    logging.debug('Try running %s', repr(function))
    try:
      function(*args, **kwargs)
      logging.debug('Did run %s', repr(function))
    except Exception:  # pylint: disable=broad-except
      logging.exception('Exception running %s', repr(function))
  return _WrappedFn


def Register(function, *args, **kwargs):
  atexit.register(_WrapFunction(function), *args, **kwargs)
