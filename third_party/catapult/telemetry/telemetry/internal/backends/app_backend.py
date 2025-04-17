# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import absolute_import
import six

from py_trace_event import trace_event


class AppBackend(six.with_metaclass(trace_event.TracedMetaClass, object)):

  def __init__(self, app_type, platform_backend):
    super().__init__()
    self._app = None
    self._app_type = app_type
    self._platform_backend = platform_backend

  def SetApp(self, app):
    assert app is not None
    self._app = app

  @property
  def app(self):
    return self._app

  @property
  def app_type(self):
    return self._app_type

  @property
  def platform_backend(self):
    return self._platform_backend

  def Foreground(self):
    # TODO(catapult:#2194): Remove the unnecessary pass below when the method
    # has been implemented on all concrete subclasses.
    pass  # pylint: disable=unnecessary-pass
    raise NotImplementedError

  def Background(self):
    raise NotImplementedError

  def Close(self):
    raise NotImplementedError

  def IsAppRunning(self):
    raise NotImplementedError
