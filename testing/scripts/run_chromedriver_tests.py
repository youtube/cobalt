#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs chrome driver tests.

This script attempts to emulate the contract of gtest-style tests
invoked via recipes.

If optional argument --isolated-script-test-output=[FILENAME] is passed
to the script, json is written to that file in the format detailed in
//docs/testing/json-test-results-format.md.

If optional argument --isolated-script-test-filter=[TEST_NAMES] is passed to
the script, it should be a  double-colon-separated ("::") list of test names,
to run just that subset of tests. This list is forwarded to the chrome driver
test runner.  """

import json
import sys

import common


class ChromeDriverAdapter(common.BaseIsolatedScriptArgsAdapter):

  # Overriding parent implementation.
  # pylint: disable=no-self-use
  def generate_test_output_args(self, output):
    return ['--isolated-script-test-output', output]
  # pylint: enable=no-self-use

  def generate_test_launcher_retry_limit_args(self, retry_limit):
    if any('--retry-limit' in arg for arg in self.rest_args):
      self.parser.error("can't have the test call filter with the "
                        '--isolated-script-test-launcher-retry-limit argument '
                        'to the wrapper script')
    return ['--retry-limit=%d' % retry_limit]

  def generate_test_repeat_args(self, repeat_count):
    if any('--repeat' in arg for arg in self.rest_args):
      self.parser.error(
          "can't have the test call filter with the "
          '--isolated-script-test-repeat argument to the wrapper script')
    return ['--repeat=%d' % repeat_count]

  def generate_test_filter_args(self, test_filter_str):
    if any('--filter' in arg for arg in self.rest_args):
      self.parser.error(
          "can't have the test call filter with the "
          '--isolated-script-test-filter argument to the wrapper script')

    return ['--filter', test_filter_str.replace('::', ':')]


def main():
  adapter = ChromeDriverAdapter()
  return adapter.run_test()


# This is not really a "script test" so does not need to manually add
# any additional compile targets.
def main_compile_targets(args):
  json.dump([], args.output)


if __name__ == '__main__':
  # Conform minimally to the protocol defined by ScriptTest.
  if 'compile_targets' in sys.argv:
    funcs = {
        'run': None,
        'compile_targets': main_compile_targets,
    }
    sys.exit(common.run_script(sys.argv[1:], funcs))
  sys.exit(main())
