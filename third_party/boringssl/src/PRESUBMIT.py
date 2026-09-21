# Copyright 2025 The BoringSSL Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Presubmit checks for BoringSSL.

Run by the presubmit API in depot_tools, e.g. by running `git cl presubmit`.
"""

PRESUBMIT_VERSION = '2.0.0'
USE_PYTHON3 = True


def _PassResult(stdout, stderr, retcode):
  del stdout, stderr, retcode  # Function does nothing.
  return []  # Check passed.


def _RunTool(input_api,
             output_api,
             command,
             handle_result=_PassResult,
             explain_error=None):
  """
  Runs a command and processes its result.

  handle_result(stdout, stderr, retcode) and explain_error(error) should each
  return a list of output_api.PresubmitResult.

  Args:
    input_api: API to read CL contents.
    output_api: API to generate presubmit errors/warning to return.
    handle_result: function receiving (stdout, stderr, retcode) parsing the
      command result and turning it into a list of output_api.PresubmitResult.
    explain_error: function receiving error turning it into a list of
      output_api.PresubmitResult.

  Returns:
    List of presubmit errors.
  """
  try:
    out, retcode = input_api.subprocess.communicate(
        command,
        stdout=input_api.subprocess.PIPE,
        stderr=input_api.subprocess.PIPE,
        encoding='utf-8')
    return handle_result(out[0], out[1], retcode)
  except input_api.subprocess.CalledProcessError as e:
    errors = []
    if explain_error:
      errors += explain_error(e)
    errors += [
        output_api.PresubmitPromptOrNotify(
            'Command "%s" returned exit code %d. Output: \n\n%s' %
            ' '.join(command), e.returncode, e.output)
    ]
    return errors


def CheckPregeneratedFiles(input_api, output_api):
  """Checks that pregenerated files are properly updated."""
  # TODO(chlily): Make this compatible with the util/bot environment for CI/CQ.
  # Check that `go` is available on the $PATH.
  if error := _RunTool(input_api, output_api, ['go', 'version']):
    return error

  def HandlePregenerateResult(stdout, stderr, retcode):
    del stdout  # All output we care for is in stderr.
    if retcode:
      return [
          output_api.PresubmitError(
              ('Found out-of-date generated files. '
               'Run `go run ./util/pregenerate` to update them.'),
              stderr.splitlines())
      ]
    return []  # Check passed.

  pregenerate_script_path = input_api.os_path.join(
      input_api.change.RepositoryRoot(), 'util', 'pregenerate')
  return _RunTool(input_api,
                  output_api,
                  ['go', 'run', pregenerate_script_path, '-check'],
                  handle_result=HandlePregenerateResult)


def CheckBuildifier(input_api, output_api):
  """
  Runs Buildifier formatting check if the affected files include
  *.bazel or *.bzl files.

  Args:
    input_api: API to read CL contents.
    output_api: API to generate presubmit errors/warning to return.

  Returns:
    List of presubmit errors.
  """
  file_paths = []
  for affected_file in input_api.AffectedFiles(include_deletes=False):
    affected_file_path = affected_file.LocalPath()
    if not affected_file_path.endswith(('.bzl', '.bazel')):
      continue
    if 'third_party' in affected_file_path or 'gen' in affected_file_path:
      continue
    file_paths.append(affected_file_path)
  if not file_paths:
    return []  # Check passed.

  def ExplainBuildifierError(e):
    del e  # Right now only one error string can be output.
    return [
        output_api.PresubmitNotifyResult(
            ('You can download buildifier from '
             'https://github.com/bazelbuild/buildtools/releases'))
    ]

  # Check that `buildifier` is available on the $PATH.
  # TODO(chlily): Make this compatible with the util/bot environment for CI/CQ.
  if error := _RunTool(input_api,
                       output_api, ['buildifier', '--version'],
                       explain_error=ExplainBuildifierError):
    return error

  def HandleBuildifierResult(stdout, stderr, retcode):
    del stderr  # This only parses stdout, as nothing failed.
    if retcode == 4:  # check mode failed (reformat is needed).
      return [
          output_api.PresubmitError(
              ('Found incorrectly formatted *.bzl or *.bazel files. '
               'Run `buildifier` to update them.'), stdout.splitlines())
      ]

  return _RunTool(input_api,
                  output_api, ['buildifier', '--mode=check'] + file_paths,
                  handle_result=HandleBuildifierResult)
