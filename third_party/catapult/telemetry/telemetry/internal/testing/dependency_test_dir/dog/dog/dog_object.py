# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import absolute_import
import os
import sys

sys.path.append(os.path.join(
    os.path.dirname(__file__), '..', '..', 'other_animals', 'cat'))

from cat import cat_object  # pylint: disable=import-error,wrong-import-position

class Dog():
  def CreateEnemy(self):
    return cat_object.Cat()
