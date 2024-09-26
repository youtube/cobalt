# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit checks for content/test
See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import sys

PRESUBMIT_VERSION = '2.0.0'

USE_PYTHON3 = True


def CheckChange(input_api, output_api):
    old_sys_path = sys.path[:]
    results = []
    try:
        sys.path.append(input_api.change.RepositoryRoot())
        # pylint: disable=no-name-in-module,import-outside-toplevel
        from build.ios import presubmit_support
        results += presubmit_support.CheckBundleData(
            input_api, output_api, 'content_unittests_bundle_data', '.')
    finally:
        sys.path = old_sys_path
    return results
