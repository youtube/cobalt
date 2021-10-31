# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from webkitpy.common.memoized import memoized
from webkitpy.common.webkit_finder import WebKitFinder


@memoized
def absolute_chromium_wpt_dir(host):
    finder = WebKitFinder(host.filesystem)
    return finder.path_from_webkit_base('LayoutTests', 'external', 'wpt')


@memoized
def absolute_chromium_dir(host):
    finder = WebKitFinder(host.filesystem)
    return finder.chromium_base()
