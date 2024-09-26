# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""PRESUBMIT for scripts.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'
USE_PYTHON3 = True


def CheckTests(input_api, output_api):
  glob = input_api.os_path.join(
      input_api.PresubmitLocalPath(), 'tests', '*_test.py')
  tests = input_api.canned_checks.GetUnitTests(input_api,
                                               output_api,
                                               input_api.glob(glob),
                                               run_on_python2=False,
                                               run_on_python3=True,
                                               skip_shebang_check=True)
  return input_api.RunTests(tests)
