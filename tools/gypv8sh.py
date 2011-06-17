#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script is used by chrome_tests.gypi's js2webui action to maintain the
# argument lists and to generate inlinable tests.
#
# Usage:
#   python tools/gypv8sh.py -p product_dir path/to/javascript2webui.js
#   python tools/gypv8sh.py -t # print test_harnesses
#   python tools/gypv8sh.py -i # print inputs
#   python tools/gypv8sh.py -o # print outputs
import json
import optparse
import os
import subprocess
import sys

# Please adjust the following to edit or add new javascript webui tests.
rules = [
    [
        'WebUIBrowserTestPass',
        'test/data/webui/sample_pass.js',
        'browser/ui/webui/web_ui_browsertest-inl.h',
        ],
    ]

def set_add(option, opt_str, value, parser, the_set, the_value):
  the_set.add(the_value)

# For options -t, -i, & -o, we print the "column" of the |rules|.  We keep a set
# of indices to print in |print_rule_indices| and print them in sorted order if
# non-empty.
print_rule_indices = set()
parser = optparse.OptionParser();
parser.set_usage(
    "%prog [-v][-n] --product_dir PRODUCT_DIR -or- "
    "%prog [-v][-n] (-i|-t|-o)");
parser.add_option('-v', '--verbose', action='store_true', dest='verbose')
parser.add_option('-n', '--impotent', action='store_true', dest='impotent',
                  help="don't execute; just print (as if verbose)")
parser.add_option('-p', '--product_dir', action='store', type='string',
                  dest='product_dir',
                  help='for gyp to set the <(PRODUCT_DIR) for running v8_shell')
parser.add_option('-t', '--test_fixture', action='callback', callback=set_add,
                  callback_args=tuple([print_rule_indices, 0]),
                  help='print test_fixtures')
parser.add_option('-i', '--in', action='callback', callback=set_add,
                  callback_args=tuple([print_rule_indices, 1]),
                  help='print inputs')
parser.add_option('-o', '--out', action='callback', callback=set_add,
                  callback_args=tuple([print_rule_indices, 2]),
                  help='print outputs')
(opts, args) = parser.parse_args();

if len(print_rule_indices):
  for rule in rules:
    for index in sorted(print_rule_indices):
      print rule[index],
    print
else:
  if not opts.product_dir:
    parser.error("--product_dir option is required");
  v8_shell = os.path.join(opts.product_dir, 'v8_shell')
  jsfilename = args[0]
  for rule in rules:
    arguments = [jsfilename, rule[0], rule[1], os.path.basename(rule[1])]
    arguments = "arguments=" + json.dumps(arguments)
    cmd = [v8_shell, '-e', arguments, jsfilename]
    if opts.verbose or opts.impotent:
      print cmd
    if not opts.impotent:
      sys.exit(subprocess.call(cmd, stdout=open(rule[2],'w+')))
