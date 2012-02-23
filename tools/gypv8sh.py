#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used by chrome_tests.gypi's js2webui action to maintain the
argument lists and to generate inlinable tests.

Usage:
  python tools/gypv8sh.py v8_shell mock.js test_api.js js2webui.js \
         inputfile inputrelfile cxxoutfile jsoutfile
"""

try:
  import json
except ImportError:
  import simplejson as json
import optparse
import os
import subprocess
import sys
import shutil

def main ():
  parser = optparse.OptionParser()
  parser.set_usage(
      "%prog v8_shell mock.js test_api.js js2webui.js "
      "testtype inputfile inputrelfile cxxoutfile jsoutfile")
  parser.add_option('-v', '--verbose', action='store_true')
  parser.add_option('-n', '--impotent', action='store_true',
                    help="don't execute; just print (as if verbose)")
  (opts, args) = parser.parse_args()

  if len(args) != 9:
    parser.error('all arguments are required.')
  (v8_shell, mock_js, test_api, js2webui, test_type,
      inputfile, inputrelfile, cxxoutfile, jsoutfile) = args
  arguments = [js2webui, inputfile, inputrelfile, cxxoutfile, test_type]
  cmd = [v8_shell, '-e', "arguments=" + json.dumps(arguments), mock_js,
         test_api, js2webui]
  if opts.verbose or opts.impotent:
    print cmd
  if not opts.impotent:
    try:
      subprocess.check_call(cmd, stdout=open(cxxoutfile, 'w'))
      shutil.copyfile(inputfile, jsoutfile)
    except Exception, ex:
      print ex
      os.remove(cxxoutfile)
      os.remove(jsoutfile)
      sys.exit(1)

if __name__ == '__main__':
 sys.exit(main())
