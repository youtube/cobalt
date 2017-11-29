#!/usr/bin/python

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

import os
import re
import sys

import _env  # pylint: disable=unused-import
from starboard.tools import paths
from starboard.tools import platform


# Sometimes files have weird encodings. This function will use a variety of
# hand selected encoders that work on the starboard codebase.
def AutoDecodeString(file_data):
  for encoding in {'UTF-8', 'utf_16', 'windows-1253', 'iso-8859-7', 'macgreek'}:
    try:
      return file_data.decode(encoding)
    except ValueError:
      continue
  raise IOError('Could not read file')


# Given a search_term, this will open the file_path and return the first
# line that contains the search term. This will ignore C-style comments.
def SearchInFileReturnFirstMatchingLine(file_path, search_term):
  try:
    lines = OpenFileAndDecodeLinesAndRemoveComments(file_path)
    for line in lines:
      if search_term in line:
        return line
    return None
  except IOError:
    print ('  error while reading file ', file_path)


# Opens a header or cc file and decodes it to utf8 using a variety
# of decoders. All lines will have their comments stripped out.
# This will return the lines of the given file.
def OpenFileAndDecodeLinesAndRemoveComments(file_path):
  with open(file_path, 'rb+') as fd:
    lines = AutoDecodeString(fd.read()).splitlines()
    # remove c-style comments.
    lines = [re.sub('//.*', '', line) for line in lines]
    return lines


def FindIncludeFiles(file_path):
  """Given a file_path, return all include files that it contains."""
  try:
    output_list = []
    lines = OpenFileAndDecodeLinesAndRemoveComments(file_path)
    for line in lines:
      # Remove c-style comments.
      if '#include' in line:
        line = re.sub('#include ', '', line).replace('"', '')
        output_list.append(line)
    return output_list
  except IOError:
    print ('  error while reading file ', file_path)


# Searches from the search_location for a configuration.h file that
# contains the definition of the SB_EXPERIMENTAL_API_VERSION and then
# returns that as type int.
def ExtractExperimentalApiVersion(config_file_path):
  """Searches for and extracts the current experimental API version."""
  needle = '#define SB_EXPERIMENTAL_API_VERSION'
  line = SearchInFileReturnFirstMatchingLine(
      config_file_path,
      '#define SB_EXPERIMENTAL_API_VERSION')
  if not line:
    raise ValueError('Could not find ' + needle + ' in ' + config_file_path)

  elements = line.split(' ')
  exp_api_version = int(elements[2])
  return exp_api_version


# Given platform path, this function will try and find the version. Returns
# either the version if found, or None.
# If the version string is returned, note that it could be
# 'SB_EXPERIMENTAL_VERSION' or a number string.
def FindVersion(platform_path):
  api_version_str = '#define SB_API_VERSION'
  result = SearchInFileReturnFirstMatchingLine(platform_path, api_version_str)
  if not result:
    return None
  version_str = result.replace(api_version_str, '')
  return version_str.strip()


# Given the path to the platform_include_file, this will find the include
# files with "configuration_public.h" in the name and return those.
def FindConfigIncludefile(platform_path_config_file):
  include_files = FindIncludeFiles(platform_path_config_file)
  include_files = [x for x in include_files if 'configuration_public.h' in x]
  return include_files


def GeneratePlatformPathMap():
  """Return a map of platform-name -> full-path-to-platform-config-header."""
  def GenPath(p):
    full_path = os.path.abspath(os.path.join(p.path, 'configuration_public.h'))
    if not os.path.exists(full_path):
      raise IOError('Could not find path ' + full_path)
    return full_path

  return {p.name: GenPath(p) for p in platform.GetAllInfos()}


# Given the root starboard directory, and the full path to the platform,
# this function will search for the API_VERSION of the platform. It will
# first see if the version is defined within the include file, if it is
# not then the include paths for shared platform configurations are
# searched in the recursive step.
def FindVersionRecursive(starboard_dir, platform_path):
  version_str = FindVersion(platform_path)
  if version_str:
    return version_str
  else:
    config_include_paths = FindConfigIncludefile(platform_path)
    if not config_include_paths:
      return '<UNKNOWN>'
    elif len(config_include_paths) > 1:
      return '<AMBIGUIOUS>'
    else:
      include_path = config_include_paths[0]
      include_path = re.sub(r'^starboard/', '', include_path)
      full_include_path = os.path.join(starboard_dir, include_path)
      return FindVersionRecursive(starboard_dir, full_include_path)


def Main():
  """Prints the API versions of all known ports."""
  print('\n***** Listing the API versions of all known ports. *****\n')

  port_dict = GeneratePlatformPathMap()

  experimental_api_version = ExtractExperimentalApiVersion(
      os.path.join(paths.STARBOARD_ROOT, 'configuration.h'))

  path_map = {}

  print('Experimental API Version: ' + str(experimental_api_version) + '\n')

  for platform_name, platform_path in port_dict.iteritems():
    version_str = FindVersionRecursive(paths.STARBOARD_ROOT, platform_path)
    if 'SB_EXPERIMENTAL_API_VERSION' in version_str:
      version_str = str(experimental_api_version)
    path_map[platform_name] = version_str

  for platform_name, api_version in sorted(path_map.iteritems()):
    print(platform_name + ': ' + api_version)

  return 0


if __name__ == '__main__':
  # All functionality stored in Main() to avoid py-lint from warning about
  # about shadowing global variables in local functions.
  sys.exit(Main())
