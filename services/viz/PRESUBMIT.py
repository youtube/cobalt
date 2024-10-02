# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for services/viz."""

USE_PYTHON3 = True


def CheckChangeOnUpload(input_api, output_api):
  import sys
  original_sys_path = sys.path
  sys.path = sys.path + [input_api.os_path.join(
      input_api.change.RepositoryRoot(),
      'components', 'viz')]

  import presubmit_checks as ps
  allowlist=(r'^services[\\/]viz[\\/].*\.(cc|h)$',)
  return ps.RunAllChecks(input_api, output_api, allowlist)
