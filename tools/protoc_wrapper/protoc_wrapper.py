#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A simple wrapper for protoc to add includes in generated cpp headers."""

import optparse
import subprocess
import sys

PROTOC_INCLUDE_POINT = '// @@protoc_insertion_point(includes)\n'

def ModifyHeader(header_file, extra_header):
  """Adds |extra_header| to |header_file|. Returns 0 on success.

  |extra_header| is the name of the header file to include.
  |header_file| is a generated protobuf cpp header.
  """
  include_point_found = False
  header_contents = []
  with open(header_file) as f:
    for line in f:
      header_contents.append(line)
      if line == PROTOC_INCLUDE_POINT:
        extra_header_msg = '#include "%s"\n' % extra_header
        header_contents.append(extra_header_msg)
        include_point_found = True;
  if not include_point_found:
    return 1

  with open(header_file, 'wb') as f:
    f.write(''.join(header_contents))
  return 0


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('--include', dest='extra_header',
                    help='The extra header to include. This must be specified '
                         'along with --protobuf.')
  parser.add_option('--protobuf', dest='generated_header',
                    help='The c++ protobuf header to add the extra header to. '
                         'This must be specified along with --include.')
  (options, args) = parser.parse_args(sys.argv)
  if len(args) < 2:
    return 1

  # Run what is hopefully protoc.
  ret = subprocess.call(args[1:])
  if ret != 0:
    return ret

  # protoc succeeded, check to see if the generated cpp header needs editing.
  if not options.extra_header or not options.generated_header:
    return 0
  return ModifyHeader(options.generated_header, options.extra_header)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
