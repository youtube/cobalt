#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# based on an almost identical script by: jyrki@google.com (Jyrki Alakuijala)

"""Prints out include dependencies in chrome.

Since it ignores defines, it gives just a rough estimation of file size.

Usage:
  tools/include_tracer.py chrome/browser/ui/browser.h
"""

import os
import sys

# Created by copying the command line for prerender_browsertest.cc, replacing
# spaces with newlines, and dropping everything except -F and -I switches.
# TODO(port): Add windows, linux directories.
INCLUDE_PATHS = [
  '',
  'gpu',
  'skia/config',
  'skia/ext',
  'testing/gmock/include',
  'testing/gtest/include',
  'third_party/GTM',
  'third_party/WebKit/Source',
  'third_party/WebKit/Source/JavaScriptCore',
  'third_party/WebKit/Source/JavaScriptCore/wtf',
  'third_party/WebKit/Source/ThirdParty/glu',
  'third_party/WebKit/Source/WebCore',
  'third_party/WebKit/Source/WebCore/accessibility',
  'third_party/WebKit/Source/WebCore/accessibility/chromium',
  'third_party/WebKit/Source/WebCore/bindings',
  'third_party/WebKit/Source/WebCore/bindings/generic',
  'third_party/WebKit/Source/WebCore/bindings/v8',
  'third_party/WebKit/Source/WebCore/bindings/v8/custom',
  'third_party/WebKit/Source/WebCore/bindings/v8/specialization',
  'third_party/WebKit/Source/WebCore/bridge',
  'third_party/WebKit/Source/WebCore/bridge/jni',
  'third_party/WebKit/Source/WebCore/bridge/jni/v8',
  'third_party/WebKit/Source/WebCore/css',
  'third_party/WebKit/Source/WebCore/dom',
  'third_party/WebKit/Source/WebCore/dom/default',
  'third_party/WebKit/Source/WebCore/editing',
  'third_party/WebKit/Source/WebCore/fileapi',
  'third_party/WebKit/Source/WebCore/history',
  'third_party/WebKit/Source/WebCore/html',
  'third_party/WebKit/Source/WebCore/html/canvas',
  'third_party/WebKit/Source/WebCore/html/parser',
  'third_party/WebKit/Source/WebCore/html/shadow',
  'third_party/WebKit/Source/WebCore/inspector',
  'third_party/WebKit/Source/WebCore/loader',
  'third_party/WebKit/Source/WebCore/loader/appcache',
  'third_party/WebKit/Source/WebCore/loader/archive',
  'third_party/WebKit/Source/WebCore/loader/cache',
  'third_party/WebKit/Source/WebCore/loader/icon',
  'third_party/WebKit/Source/WebCore/mathml',
  'third_party/WebKit/Source/WebCore/notifications',
  'third_party/WebKit/Source/WebCore/page',
  'third_party/WebKit/Source/WebCore/page/animation',
  'third_party/WebKit/Source/WebCore/page/chromium',
  'third_party/WebKit/Source/WebCore/platform',
  'third_party/WebKit/Source/WebCore/platform/animation',
  'third_party/WebKit/Source/WebCore/platform/audio',
  'third_party/WebKit/Source/WebCore/platform/audio/chromium',
  'third_party/WebKit/Source/WebCore/platform/audio/mac',
  'third_party/WebKit/Source/WebCore/platform/chromium',
  'third_party/WebKit/Source/WebCore/platform/cocoa',
  'third_party/WebKit/Source/WebCore/platform/graphics',
  'third_party/WebKit/Source/WebCore/platform/graphics/cg',
  'third_party/WebKit/Source/WebCore/platform/graphics/chromium',
  'third_party/WebKit/Source/WebCore/platform/graphics/cocoa',
  'third_party/WebKit/Source/WebCore/platform/graphics/filters',
  'third_party/WebKit/Source/WebCore/platform/graphics/gpu',
  'third_party/WebKit/Source/WebCore/platform/graphics/mac',
  'third_party/WebKit/Source/WebCore/platform/graphics/opentype',
  'third_party/WebKit/Source/WebCore/platform/graphics/skia',
  'third_party/WebKit/Source/WebCore/platform/graphics/transforms',
  'third_party/WebKit/Source/WebCore/platform/image-decoders',
  'third_party/WebKit/Source/WebCore/platform/image-decoders/bmp',
  'third_party/WebKit/Source/WebCore/platform/image-decoders/gif',
  'third_party/WebKit/Source/WebCore/platform/image-decoders/ico',
  'third_party/WebKit/Source/WebCore/platform/image-decoders/jpeg',
  'third_party/WebKit/Source/WebCore/platform/image-decoders/png',
  'third_party/WebKit/Source/WebCore/platform/image-decoders/skia',
  'third_party/WebKit/Source/WebCore/platform/image-decoders/webp',
  'third_party/WebKit/Source/WebCore/platform/image-decoders/xbm',
  'third_party/WebKit/Source/WebCore/platform/image-encoders/skia',
  'third_party/WebKit/Source/WebCore/platform/mac',
  'third_party/WebKit/Source/WebCore/platform/mock',
  'third_party/WebKit/Source/WebCore/platform/network',
  'third_party/WebKit/Source/WebCore/platform/network/chromium',
  'third_party/WebKit/Source/WebCore/platform/sql',
  'third_party/WebKit/Source/WebCore/platform/text',
  'third_party/WebKit/Source/WebCore/platform/text/mac',
  'third_party/WebKit/Source/WebCore/platform/text/transcoder',
  'third_party/WebKit/Source/WebCore/plugins',
  'third_party/WebKit/Source/WebCore/plugins/chromium',
  'third_party/WebKit/Source/WebCore/rendering',
  'third_party/WebKit/Source/WebCore/rendering/style',
  'third_party/WebKit/Source/WebCore/rendering/svg',
  'third_party/WebKit/Source/WebCore/storage',
  'third_party/WebKit/Source/WebCore/storage/chromium',
  'third_party/WebKit/Source/WebCore/svg',
  'third_party/WebKit/Source/WebCore/svg/animation',
  'third_party/WebKit/Source/WebCore/svg/graphics',
  'third_party/WebKit/Source/WebCore/svg/graphics/filters',
  'third_party/WebKit/Source/WebCore/svg/properties',
  'third_party/WebKit/Source/WebCore/webaudio',
  'third_party/WebKit/Source/WebCore/websockets',
  'third_party/WebKit/Source/WebCore/workers',
  'third_party/WebKit/Source/WebCore/xml',
  'third_party/WebKit/Source/WebKit/chromium/public',
  'third_party/WebKit/Source/WebKit/chromium/src',
  'third_party/WebKit/Source/WebKit/mac/WebCoreSupport',
  'third_party/WebKit/WebKitLibraries',
  'third_party/cld',
  'third_party/icu/public/common',
  'third_party/icu/public/i18n',
  'third_party/npapi',
  'third_party/npapi/bindings',
  'third_party/protobuf',
  'third_party/protobuf/src',
  'third_party/skia/gpu/include',
  'third_party/skia/include/config',
  'third_party/skia/include/core',
  'third_party/skia/include/effects',
  'third_party/skia/include/gpu',
  'third_party/skia/include/pdf',
  'third_party/skia/include/ports',
  'v8/include',
  'xcodebuild/Debug/include',
  'xcodebuild/DerivedSources/Debug/chrome',
  'xcodebuild/DerivedSources/Debug/policy',
  'xcodebuild/DerivedSources/Debug/protoc_out',
  'xcodebuild/DerivedSources/Debug/webkit',
  'xcodebuild/DerivedSources/Debug/webkit/bindings',
]


def Walk(seen, filename, parent, indent):
  """Returns the size of |filename| plus the size of all files included by
  |filename| and prints the include tree of |filename| to stdout. Every file
  is visited at most once.
  """
  total_bytes = 0

  # .proto(devel) filename translation
  if filename.endswith('.pb.h'):
    basename = filename[:-5]
    if os.path.exists(basename + '.proto'):
      filename = basename + '.proto'
    else:
      print 'could not find ', filename

  # Show and count files only once.
  if filename in seen:
    return total_bytes
  seen.add(filename)

  # Display the paths.
  print ' ' * indent + filename

  # Skip system includes.
  if filename[0] == '<':
    return total_bytes

  # Find file in all include paths.
  resolved_filename = filename
  for root in INCLUDE_PATHS + [os.path.dirname(parent)]:
    if os.path.exists(os.path.join(root, filename)):
      resolved_filename = os.path.join(root, filename)
      break

  # Recurse.
  if os.path.exists(resolved_filename):
    lines = open(resolved_filename).readlines()
  else:
    print ' ' * (indent + 2) + "-- not found"
    lines = []
  for line in lines:
    line = line.strip()
    if line.startswith('#include "'):
      total_bytes += Walk(
          seen, line.split('"')[1], resolved_filename, indent + 2)
    elif line.startswith('#include '):
      include = '<' + line.split('<')[1].split('>')[0] + '>'
      total_bytes += Walk(
          seen, include, resolved_filename, indent + 2)
    elif line.startswith('import '):
      total_bytes += Walk(
          seen, line.split('"')[1], resolved_filename, indent + 2)
  return total_bytes + len("".join(lines))


def main():
  bytes = Walk(set(), sys.argv[1], '', 0)
  print
  print float(bytes) / (1 << 20), "megabytes of chrome source"


if __name__ == '__main__':
  sys.exit(main())
