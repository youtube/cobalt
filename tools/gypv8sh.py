#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used by chrome_tests.gypi's js2webui action to maintain the
argument lists and to generate inlinable tests.

Usage:
  python tools/gypv8sh.py v8_shell js2webui.js inputfile outputfile
"""

try:
  import json
except ImportError:
  import simplejson as json
import optparse
import os
import subprocess
import sys

def main ():
  parser = optparse.OptionParser()
  parser.set_usage("%prog v8_shell js2webui.js inputfile outputfile")
  parser.add_option('-v', '--verbose', action='store_true')
  parser.add_option('-n', '--impotent', action='store_true',
                    help="don't execute; just print (as if verbose)")
  (opts, args) = parser.parse_args()

  if len(args) != 4:
    parser.error('all arguments are required.')
  v8_shell, js2webui, inputfile, outputfile = args
  arguments = [js2webui, inputfile, os.path.basename(inputfile), outputfile]
  cmd = [v8_shell, '-e', "arguments=" + json.dumps(arguments), js2webui]
  if opts.verbose or opts.impotent:
    print cmd
  if not opts.impotent:
    try:
      subprocess.check_call(cmd, stdout=open(outputfile,'w'))
    except Exception, ex:
      print ex
      os.remove(outputfile)
      sys.exit(1)

if __name__ == '__main__':
 sys.exit(main())
