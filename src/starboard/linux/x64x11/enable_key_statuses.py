#
# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
#
"""Utility to apply GetKeys() related changes to wv_cdm.

Such changes are required for x64x11-wv to support key statuses.
"""

import os
import subprocess


def GetWideVinePath():
  """Function to return the path to wv cdm."""
  paths = ['starboard', 'linux', 'x64x11', 'wv']

  current_path = os.path.dirname(os.path.realpath(__file__))
  for path in reversed(paths):
    assert os.path.basename(current_path) == path
    current_path = os.path.dirname(current_path)
  return os.path.join(current_path, 'third_party', 'cdm')


def GetLastGitCommit():
  """Function to return the hash of the last commit."""
  return subprocess.check_output(['git', 'log', '--oneline', '-1'])[:7]


def GetCurrentGitBranch():
  """Function to return the current git branch."""
  branches = subprocess.check_output(['git', 'branch']).split('\n')
  current_branches = [branch for branch in branches if branch[:2] == '* ']
  if len(current_branches) == 1:
    return current_branches[0][2:].strip()
  return ''


def PatchWideVine():
  """Function to patch wv cdm with the `wv_cdm_get_keys.diff` if not applied."""
  os.chdir(GetWideVinePath())

  with open('cdm/include/content_decryption_module.h') as f:
    if f.read().find('GetKeys') != -1:
      # We have already patched WideVine
      print 'WideVine has already been been patched with GetKeys() support.'
      return

  if (GetCurrentGitBranch() != 'master' and
      GetCurrentGitBranch() != '(HEAD detached at 5ae19b0)' and
      GetLastGitCommit() != '5ae19b0'):
    raise Exception('WideVine is not at master or `5ae19b0`'
                    ', not proceed to avoid overwriting changes.')

  if (subprocess.call(['git', 'diff', '--exit-code']) == 1 or
      subprocess.call(['git', 'diff', '--cached', '--exit-code']) == 1):
    raise Exception('WideVine repository is not clean.')

  subprocess.check_call(
      ['git', 'checkout', '--quiet', '-b', 'wv2.2', 'tags/v2.2'])
  assert GetLastGitCommit() == '5ae19b0'

  subprocess.check_call(
      ['git', 'apply', '../../starboard/linux/x64x11/wv_cdm_get_keys.diff'])
  subprocess.check_call(
      ['git', 'commit', '-a', '--quiet', '-m', '"Add GetKeys() support"'])

  with open('cdm/include/content_decryption_module.h') as f:
    assert f.read().find('GetKeys') != -1

  print('WideVine has been patched with GetKeys() support, '
        'remember to re-run gyp_driver!')


PatchWideVine()
