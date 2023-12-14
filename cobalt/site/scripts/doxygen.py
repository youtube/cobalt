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
"""Python wrapper for Doxygen."""

import errno
import logging
import os
import re
import subprocess

import environment

_MODULE_OVERVIEW_PATTERN = r'Module Overview: '
_MODULE_OVERVIEW_RE = re.compile(_MODULE_OVERVIEW_PATTERN)


def _subprocess(command, working_directory):
  """Executes command in working_directory."""
  if not os.path.isdir(working_directory):
    raise RuntimeError(
        f'Running {command}: directory {working_directory} not found')

  try:
    # shell=True for Windows to be able to find git in the PATH.
    subprocess.check_output(
        ' '.join(command),
        shell=True,
        cwd=working_directory,
        stderr=subprocess.STDOUT)
    return True
  except subprocess.CalledProcessError as e:
    logging.warning('%s: \"%s\" failed. Return Code: %d', working_directory,
                    e.cmd, e.returncode)
    logging.debug('>>>\n%s\n<<<', e.output)
    return False


def _mkdirs(directory_path):
  """Makes the given path and all ancestors necessary."""
  try:
    os.makedirs(directory_path)
  except OSError as e:
    if e.errno == errno.EEXIST and os.path.isdir(directory_path):
      pass
    else:
      raise


def _join_config(lines):
  return ' \\\n  '.join(lines)


def doxygen(project_number, input_files, predefined_macros,
            output_directory_path):
  """Runs doxygen with the given semantic parameters."""
  doxyfile_template_path = os.path.join(environment.SCRIPTS_DIR,
                                        'doxyfile.template')
  doxyfile_template = environment.read_file(doxyfile_template_path)
  os.makedirs(output_directory_path)
  predefined_macros = predefined_macros[:]
  predefined_macros.append(f'SB_API_VERSION={project_number}')
  doxyfile_contents = doxyfile_template.format(
      **{
          'project_number': 1,
          'output_directory': output_directory_path,
          'input_files': _join_config(input_files),
          'predefined_macros': _join_config(predefined_macros),
      })

  doxyfile_path = os.path.join(output_directory_path, 'Doxyfile')
  environment.write_file(doxyfile_path, doxyfile_contents)
  _subprocess(['doxygen', doxyfile_path], output_directory_path)


def _split(line):
  """Splits a line into a (indentation, lstripped content) tuple."""
  stripped = line.lstrip()
  return line[:(len(line) - len(stripped))], stripped


def _doxygenate_line(line):
  """Adds an extra slash to a comment line."""
  indent, stripped = _split(line)
  return indent + '/' + stripped


def _doxygenate_lines(lines):
  """Makes a list of comment lines visible to Doxygen."""
  if not lines:
    return []
  indent, _ = _split(lines[0])
  return [_doxygenate_line(x) for x in lines] + [indent + '///']


def _is_comment(line):
  """Whether the given line is a comment."""
  stripped = line.lstrip()
  return stripped[0] == '/'


def _find_end_of_block(line_iterator):
  """Consumes the next comment block, returning (comment_list, terminator)."""
  lines = []
  last_line = None

  any_lines = False
  for line in line_iterator:
    any_lines = True
    if not line or not _is_comment(line):
      last_line = line
      break
    lines.append(line)
  if not any_lines:
    raise StopIteration

  return lines, last_line


def doxygenate(input_file_paths, output_directory):
  """Converts a list of source files into more doxygen-friendly files."""
  common_prefix_path = os.path.commonprefix(input_file_paths)
  output_file_paths = []
  os.makedirs(output_directory)
  for input_file_path in input_file_paths:
    output_file_path = os.path.join(output_directory,
                                    input_file_path[len(common_prefix_path):])
    _mkdirs(os.path.dirname(output_file_path))
    input_contents = environment.read_lines(input_file_path)
    output_contents = []
    line_iterator = iter(x.rstrip() for x in input_contents)
    try:
      # Remove copyright header.
      _, _ = _find_end_of_block(line_iterator)

      # Doxygenate module overview.
      lines, last_line = _find_end_of_block(line_iterator)
      if not lines:
        continue
      output_contents.append(f'/// \\file {os.path.basename(input_file_path)}')
      if _MODULE_OVERVIEW_RE.search(lines[0]):
        del lines[0]
      output_contents.extend(_doxygenate_lines(lines))
      output_contents.append(last_line)

      # Doxygenate module overview.
      lines, last_line = _find_end_of_block(line_iterator)

      # Doxygenate the rest of the file, block by block.
      while True:
        lines, last_line = _find_end_of_block(line_iterator)
        if not last_line:
          continue
        if lines:
          output_contents.extend(_doxygenate_lines(lines))
        output_contents.append(last_line)
    except StopIteration:
      pass

    environment.write_file(output_file_path, '\n'.join(output_contents))
    output_file_paths.append(output_file_path)
  return output_file_paths
