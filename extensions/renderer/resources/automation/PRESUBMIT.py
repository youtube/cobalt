# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for files in extensions/renderer/resources/automation.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

USE_PYTHON3 = True


def _CheckWebDevStyle(input_api, output_api):
  results = []

  try:
    import sys
    old_sys_path = sys.path[:]
    cwd = input_api.PresubmitLocalPath()
    sys.path += [input_api.os_path.join(cwd, '../..', '..', '..', 'tools')]
    from web_dev_style import presubmit_support
    results += presubmit_support.CheckStyle(input_api, output_api)
  finally:
    sys.path = old_sys_path

  return results


def _CheckChangeOnUploadOrCommit(input_api, output_api):
  results = _CheckWebDevStyle(input_api, output_api)
  results += input_api.canned_checks.CheckPatchFormatted(input_api, output_api,
                                                         check_js=True)
  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CheckChangeOnUploadOrCommit(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CheckChangeOnUploadOrCommit(input_api, output_api)
