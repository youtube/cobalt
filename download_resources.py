#!/usr/bin/env python2
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

import os
import platform
import subprocess
import sys


def _UseShell():
  return platform.system() == 'Windows'


def DownloadClangFormat():
  platform_arg = '--platform={}'
  clang_format_sha_path = os.path.join('buildtools', '{}', 'clang-format')

  system = platform.system()
  if system == 'Linux':
    platform_arg = platform_arg.format('linux*')
    clang_format_sha_path = clang_format_sha_path.format('linux64') + '.sha1'
  elif system == 'Darwin':
    platform_arg = platform_arg.format('darwin')
    clang_format_sha_path = clang_format_sha_path.format('mac') + '.sha1'
  elif system == 'Windows':
    platform_arg = platform_arg.format('win32')
    clang_format_sha_path = clang_format_sha_path.format('win') + '.exe.sha1'
  else:
    return

  clang_format_cmd = [
      'download_from_google_storage', '--no_resume', '--no_auth', '--bucket',
      'chromium-clang-format', platform_arg, '-s', clang_format_sha_path
  ]
  subprocess.call(clang_format_cmd, shell=_UseShell())


def DownloadConformanceTests():
  conformance_test_cmd = [
      'download_from_google_storage', '--no_resume', '--no_auth', '--bucket',
      'cobalt-static-storage', '--num_threads', '8', '-d',
      'cobalt/demos/content/mse-eme-conformance-tests/media'
  ]
  subprocess.call(conformance_test_cmd, shell=_UseShell())


def DownloadGerritCommitMsgHook(force=False):
  git_commit_msg_hook_path = os.path.join('.git', 'hooks', 'commit-msg')
  commit_msg_cmd = [
      'curl', '-Lo', git_commit_msg_hook_path,
      'https://gerrit-review.googlesource.com/tools/hooks/commit-msg'
  ]

  if force or not os.path.exists(git_commit_msg_hook_path):
    subprocess.call(commit_msg_cmd)
    try:
      subprocess.call(['chmod', '+x', git_commit_msg_hook_path])
    except WindowsError:
      print('Downloading in CMD, commit-msg already executable.')


if __name__ == "__main__":
  DownloadClangFormat()
  DownloadConformanceTests()
  DownloadGerritCommitMsgHook()
