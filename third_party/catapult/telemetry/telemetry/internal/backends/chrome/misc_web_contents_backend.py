# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import absolute_import
from telemetry.core import exceptions
from telemetry.internal.backends.chrome import oobe
from telemetry.internal.backends.chrome_inspector import inspector_backend_list


class MiscWebContentsBackend(inspector_backend_list.InspectorBackendList):
  """A dynamic sequence of web contents not related to tabs and extensions.

  Provides acccess to chrome://oobe/login page.
  """

  @property
  def oobe_exists(self):
    """Lightweight property to determine if the oobe webui is visible."""
    try:
      return bool(len(self))
    except exceptions.Error:
      return False

  def GetOobe(self):
    if not self:
      return None
    return self[0]

  def ShouldIncludeContext(self, context):
    return context.get('url').startswith('chrome://oobe')

  def CreateWrapper(self, inspector_backend_instance):
    return oobe.Oobe(inspector_backend_instance)
