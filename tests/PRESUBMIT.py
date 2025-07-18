# Copyright (c) 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

PRESUBMIT_VERSION = '2.0.0'

# This file can be removed once py2 presubmit is no longer supported. This is
# an integration test to ensure py2 presubmit still works.


def CheckPythonVersion(input_api, output_api):
  # The tests here are assuming this is not defined, so raise an error
  # if it is.
  if 'USE_PYTHON3' in globals():
    return [
        output_api.PresubmitError(
            'USE_PYTHON3 is defined; update the tests in //PRESUBMIT.py and '
            '//tests/PRESUBMIT.py.')
    ]
  if sys.version_info.major != 2:
    return [
        output_api.PresubmitError(
            'Did not use Python2 for //PRESUBMIT.py by default.')
    ]
  return []
