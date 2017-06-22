# Copyright 2017 Google Inc. All Rights Reserved.
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

"""Extracts the API versions for all available builds.

  Example usage:
    cd cobalt/src/
    extract_starboard_versions.py
"""

from __future__ import print_function

import fnmatch
import os
import re
import sys


def AutoDecodeString(file_data):
  for encoding in {'UTF-8', 'utf_16', 'windows-1253', 'iso-8859-7', 'macgreek'}:
    try:
      return file_data.decode(encoding)
    except:
      continue
  raise IOError('Could not read file')

def AnyExistsLowerCase(list_a, list_b):
  list_aa = [a.lower() for a in list_a]
  list_bb = [b.lower() for b in list_b]
  return not set(list_aa).isdisjoint(list_bb)

def SearchInFileReturnMatchingLine(file_path, search_term):
  try:
    with open(file_path, 'rb+') as fd:
      file_text_lines = AutoDecodeString(fd.read()).splitlines()
      for file_line in file_text_lines:
        if search_term in file_line:
          return file_line
  except IOError:
    print('  error while reading file ', file_path)

# "C:\dir1\dir2" -> ['C:', 'dir1', 'dir2']
def PathAsList(path, debug=False):
  parts = []
  while True:
    newpath, tail = os.path.split(path)
    if debug:
      print (repr(path), (newpath, tail))
    if newpath == path:
      assert not tail
      if path:
        parts.append(path)
      break
    parts.append(tail)
    path = newpath
  parts.reverse()
  parts = [p.replace('\\', '') for p in parts]
  return parts

# Reverse of PathAsList()
def ListAsPath(path_list):
  return os.path.sep.join(path_list)

# From the current working directory find the root directory where
# tail_path is resolvable to a directory path.
# Example:
#  FindPathByTraversingUp(os.getcwd(), 'src/cobalt/build')
def FindPathByTraversingUp(current_path, tail_path):
  relative_path_list = PathAsList(tail_path)
  original_path = os.path.abspath(current_path)
  original_path_list = PathAsList(original_path)

  current_path_list = original_path_list

  while (len(current_path_list) > 0):
    path_now = ListAsPath(current_path_list + relative_path_list)
    if os.path.isdir(path_now):
      return os.path.abspath(path_now)
    else:
      current_path_list.pop()
  return None

# Also returns file path with matches
def SearchInFileReturnMatchingLine2(file_path, search_term):
  try:
    with open(file_path, 'rb+') as fd:
      file_text_lines = AutoDecodeString(fd.read()).splitlines()
      for file_line in file_text_lines:
        if search_term in file_line:
          return [file_path, file_line]
  except IOError:
    print ('  error while reading file ', file_path)


def MatchFilesRecursive(start_point, filename_wildcard):
  filename_wildcard_list = [x.strip() for x in filename_wildcard.split(',')]
  output_list = []
  for root, dirs, files in os.walk(start_point):
    for f in files:
      full_path = os.path.join(root, f)
      if '.git' in full_path and 'objects' in full_path:
        continue
      for wildcard in filename_wildcard_list:
        if fnmatch.fnmatch(full_path, wildcard):
          output_list.append(os.path.abspath(full_path))
          break
  return output_list

def FindExperimentalApiVersion():
  files = MatchFilesRecursive('.', '*.h')
  matching_lines = \
      [SearchInFileReturnMatchingLine(f, "#define SB_EXPERIMENTAL_API_VERSION")
      for f in files]
  matching_lines = filter(lambda line: line != None, matching_lines)
  if len(matching_lines) == 0:
    print ('Error - could not determine starboard experimental version.')
    sys.exit(1)
  if len(matching_lines) > 1:
    print ('There were more than one experimental api versions?!')

  elements = matching_lines[0].split(' ')
  exp_api_version = int(elements[2])
  return exp_api_version

# exp_api_version is type int
def PrintApiVersions(exp_api_version):
  files = MatchFilesRecursive('.', '*configuration_public.h*')

  matching_lines = [SearchInFileReturnMatchingLine2(f, '#define SB_API_VERSION')
                    for f in files]
  matching_lines = filter(lambda line: line != None, matching_lines)
  for line in matching_lines:
    version = line[1].split(" ")[2]
    if 'SB_EXPERIMENTAL_API_VERSION' in version:
      version = exp_api_version
    build_id = str(line[0])
    build_id = re.sub("^.*starboard", '', build_id)
    build_id = re.sub('configuration_public\.h', '', build_id)
    build_id = build_id[1:-1]
    build_id = build_id.replace(os.path.sep, '-')
    print (build_id + ': ' + str(version))

def ChangeDirectoryToStarboardRootOrDie():
  start_path = os.path.abspath(__file__)
  starboard_path = FindPathByTraversingUp(start_path, 'src/starboard')

  if not starboard_path:
    print('Could not find starboard path from location' + start_path)
    sys.exit(1)
  os.chdir(starboard_path)

if __name__ == '__main__':
  ChangeDirectoryToStarboardRootOrDie()
  experimental_api_version = FindExperimentalApiVersion()
  print('\nExperimental API Version: ' + str(experimental_api_version) + '\n')
  PrintApiVersions(experimental_api_version)
  sys.exit(0)
