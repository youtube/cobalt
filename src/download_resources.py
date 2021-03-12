#!/usr/bin/env python
# Copyright 2020 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Downloads tools to assist in developing Cobalt."""

import logging
import os
import platform

import tools.download_from_gcs as download_from_gcs

try:
  import download_resources_internal
except ImportError:
  logging.warning('Skipping internal tools.')
  download_resources_internal = None


def DownloadClangFormat(force=False):
  clang_format_sha_path = os.path.join('buildtools', '{}', 'clang-format')

  system = platform.system()
  if system == 'Linux':
    clang_format_sha_path = clang_format_sha_path.format('linux64')
  elif system == 'Darwin':
    clang_format_sha_path = clang_format_sha_path.format('mac')
  elif system == 'Windows':
    clang_format_sha_path = clang_format_sha_path.format('win') + '.exe'
  else:
    logging.error('Unknown system: %s', system)
    return

  download_from_gcs.MaybeDownloadFileFromGcs('chromium-clang-format',
                                             clang_format_sha_path + '.sha1',
                                             clang_format_sha_path, force)


if __name__ == '__main__':
  logging_format = '[%(levelname)s:%(filename)s:%(lineno)s] %(message)s'
  logging.basicConfig(
      level=logging.INFO, format=logging_format, datefmt='%H:%M:%S')

  DownloadClangFormat()

  if download_resources_internal:
    download_resources_internal.main()
