# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'


def CheckChange(input_api, output_api):
  results = []
  try:
    import sys
    old_sys_path = sys.path[:]
    cwd = input_api.PresubmitLocalPath()
    sys.path += [input_api.os_path.join(cwd, '..', '..', '..', '..', 'tools')]
    import web_dev_style.presubmit_support
    results += web_dev_style.presubmit_support.CheckStyleESLint(
        input_api, output_api)
  finally:
    sys.path = old_sys_path
  results += input_api.canned_checks.CheckPatchFormatted(input_api, output_api,
                                                         check_js=True)
  return results

def CheckTestFilename(input_api, output_api):
  results = []

  def IsNameInvalid(affected_file):
    return affected_file.LocalPath().endswith('_tests.ts')

  invalid_test_files = input_api.AffectedFiles(include_deletes=False,
                                               file_filter=IsNameInvalid)
  for f in invalid_test_files:
    results += [
        output_api.PresubmitError(
            f'Disallowed \'_tests\' suffix found in \'{f}\'. WebUI test files '
            'must end with "_test" suffix instead.')
    ]

  return results
