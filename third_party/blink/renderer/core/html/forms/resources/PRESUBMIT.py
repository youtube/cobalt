# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


USE_PYTHON3 = True


def _CheckChangeOnUploadOrCommit(input_api, output_api):
    return input_api.canned_checks.CheckPatchFormatted(
        input_api, output_api, check_js=True)


def CheckChangeOnUpload(input_api, output_api):
    return _CheckChangeOnUploadOrCommit(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CheckChangeOnUploadOrCommit(input_api, output_api)
