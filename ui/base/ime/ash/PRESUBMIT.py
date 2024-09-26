# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'
USE_PYTHON3 = True

def CheckTastIsRequested(input_api, output_api):
  """Checks that the user did add the tast trybot to the description
  """
  keyword = 'CQ_INCLUDE_TRYBOTS=luci.chrome.try:chromeos-betty-pi-arc-chrome'
  if not(keyword in input_api.change.DescriptionText()):
    return [output_api.PresubmitPromptWarning(
        'Changes in this directory are high risk for breaking ChromeOS inputs,'
        + ' please include the tast trybot by adding: '
        + keyword
        + ' to your CL description')]

  return []
