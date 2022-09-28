#!/usr/bin/python
#
# Copyright 2022 The Cobalt Authors. All Rights Reserved.
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
"""Runs a test in the mobile harness lab, or watch an already running test.

This wrapper will trigger a mobile harness build either via blaze or the cobalt
mobile harness gateway jar.

Sample Gateway Call from Buildbot:
./mobile_harness_gateway.py --test_type unit_test --platform raspi-2 --gateway_exit_status DONE --remote_archive_path gs://mh_testing_tmp/_test_try_raspi-2_devel_tests/2019-11-05/16/unit/raspi-2_devel/cobalt_archive.tar --branch COBALT --config devel --tag cobalt_postsubmit --sponge_label cobalt_postsubmit --change_id "" --builder_name raspi-2_tests --build_number 87  # pylint:disable=line-too-long

To call with blaze test instead, simply add --use_blaze.
Optionally, add --workspace_name to have it run the blaze test from a particular workspace.
E.G. ./mobile_harness_gateway.py --use_blaze --workspace_name <existing_workspace> ...

Many parameters are only necessary for buildbot. The simplest local call to mobile harness would be:
./mobile_harness_gateway.py --test_type unit_test --platform raspi-2 --config devel --remote_archive_path gs://mh_testing_tmp/_test_try_raspi-2_devel_tests/2019-11-05/16/unit/raspi-2_devel/cobalt_archive.tar  # pylint:disable=line-too-long
This assumes that you've uploaded files to gcs first.
"""

import argparse
import getpass
import logging
import os
import pipes
import platform as system_platform
import re
import stat
import subprocess
import sys
import time
import test_timeouts
import worker_tools

# Maximum time it takes to set up a job. This occurs before finding a device
# or running the test.
_JOB_SETUP_TIME_SEC = 5 * 60

# Platform names in Buildbot are of the form '<base_platform>-<variation>'
# E.G. raspi-2-skia
_COBALT_BASE_PLATFORMS = [
    'android-arm',
    'android-arm64',
    'android-x86',
    'darwin-tvos-arm64',
    'nxswitch',
    'raspi-0',
    'raspi-2',
]

# Blaze platforms that use the device server for testing.
_DEVICE_SERVER_BASE_PLATFORMS = [
    'darwin-tvos-arm64',
    'nxswitch',
]

_GSUTIL_PATH_ENV_VAR = 'GSUTIL_PATH'
_JAVA_PATH_ENV_VAR = 'JAVA_PATH'

_GATEWAY_PAT_ENV_VAR = 'COBALT_MH_GATEWAY_DIR'
_GATEWAY_JAR_NAME = 'cobalt_mh_gateway_deploy.jar'
# From org.mortbay.jetty.alpn.
# See README in _GCS_GATEWAY_DIR for details on where it comes from.
_ALPN_JAR_NAME = 'alpn.jar'
_GCS_GATEWAY_DIR = 'gs://cobalt_tools/mh_gateway'

# Gateway service vars.
_GATEWAY_SERVICE_PROTO = 'tools/mobile_harness_gateway.proto'
_GATEWAY_SERVICE_PROTO_CODE = 'tools/mobile_harness_gateway_pb2.py'
_GATEWAY_SERVICE_GRPC_CODE = 'tools/mobile_harness_gateway_pb2_grpc.py'
_GATEWAY_SERVICE_CLIENT = 'tools/mobile_harness_gateway_client.py'


def _TokenizeCmd(cmd):
  """Tokenize command elements.

  Args:
    cmd: A list of command elements.

  Returns:
    A tokenized list of commend elements.
  """
  # Wrap quotes around all but the first element to handle argument
  # tokenization.
  return [pipes.quote(x) if i > 0 else x for i, x in enumerate(cmd)]


def _PlatformUsesDeviceServer(platform):
  return _PlatformToBasePlatform(platform) in _DEVICE_SERVER_BASE_PLATFORMS


def _GetDefaultGatewayDir():
  """Get the default directory of the gateway on workers and workstations.

  Returns:
    Default directory of gateway files.

  Raises:
    RuntimeError: System has no default.
  """
  system = system_platform.system()

  if system == 'Windows':
    return os.path.join('c:\\', 'mh_gateway')

  if system in ('Linux', 'Darwin'):
    return os.path.join(os.path.expanduser('~'), 'mh_gateway')

  raise RuntimeError('Unhandled system: ' + system)


def _GetConfigFlagName(platform, test_type):
  """Get the name of the flag that declares the Cobalt config of bin files.

  There is some inconsistency in flag names on google3.

  Args:
    platform: Cobalt platform of test
    test_type: worker_tools.TestTypes that will be run

  Returns:
    Name of flag used to define binary config
  """
  if (_PlatformUsesDeviceServer(platform) or
      test_type == worker_tools.TestTypes.ENDURANCE_TEST or
      test_type == worker_tools.TestTypes.INTEGRATION_TEST):
    return 'cobalt_config'
  return 'config'


def _GetPlatformFlagName(test_type):
  """Get the name of the flag that declares the Cobalt platform of bin files.

  There is some inconsistency in flag names on google3.

  Args:
    test_type: worker_tools.TestTypes that will be run

  Returns:
    Name of flag used to define binary platform
  """
  if (test_type in (worker_tools.TestTypes.ENDURANCE_TEST,
                    worker_tools.TestTypes.INTEGRATION_TEST)):
    return 'cobalt_platform'
  return 'platform'


def _GetCobaltArtifactsFlagname(platform, test_type):
  """Get the name of the flag declaring the location of Cobalt files to test.

  There is some inconsistency in flag names on google3.

  Args:
    platform: Cobalt platform of test
    test_type: worker_tools.TestTypes that will be run

  Returns:
    Name of flag used to define cobalt test file path
  """
  if _PlatformUsesDeviceServer(platform):
    return 'cobalt_archive'
  elif (test_type in (worker_tools.TestTypes.ENDURANCE_TEST,
                      worker_tools.TestTypes.INTEGRATION_TEST,
                      worker_tools.TestTypes.PERFORMANCE_TEST)):
    return 'cobalt_build'
  else:
    return 'out'


def _GetJavaPath():
  """Get the expected path to java."""
  system = system_platform.system()

  if os.getenv(_JAVA_PATH_ENV_VAR):
    return os.getenv(_JAVA_PATH_ENV_VAR)
  if system == 'Windows':
    return 'c:\\Program Files\\Java\\jre1.8.0_231\\bin\\java.exe'
  if system == 'Linux':
    return 'java'
  if system == 'Darwin':
    return 'java'

  raise RuntimeError('Unhandled system: ' + system)


def _PlatformToBasePlatform(platform):
  """Get the platform name without variant suffixes.

  E.G. raspi-2-skia -> raspi-2

  Args:
    platform: Cobalt platform

  Returns:
    Base platform string
  """
  # Search prefixes in order of decreasing prefix length,
  # in case one prefix is a a prefix of another.
  for blaze_platform in sorted(_COBALT_BASE_PLATFORMS, key=len, reverse=True):
    if platform.startswith(blaze_platform):
      return blaze_platform
  raise ValueError('"%s" has no associated base platform' % platform)


def _PlatformToBlazePlatform(platform):
  """Convert Cobalt platform to the format used on blaze.

  Platform names in Buildbot are of the form '<base_platform>-<variation>'.
  Blaze discards the variation and uses underscores.
  E.G. raspi-2-skia -> raspi_2

  Args:
    platform: Cobalt platform

  Returns:
    Blaze platform string
  """
  return _PlatformToBasePlatform(platform).replace('-', '_')


def _GetBlazeTarget(test_type, platform):
  """Get the blaze target for given test type and platform.

  Args:
    test_type: worker_tools.TestTypes that will be run
    platform: Cobalt platform

  Returns:
    Blaze target string to run for this test.
  """
  blaze_platform = _PlatformToBlazePlatform(platform)
  if test_type == worker_tools.TestTypes.UNIT_TEST:
    return ('//video/youtube/testing/maneki/cobalt/unit_tests:'
            'cobalt_{}_unit_tests'.format(blaze_platform))
  if test_type == worker_tools.TestTypes.BLACK_BOX_TEST:
    return ('//video/youtube/testing/maneki/cobalt/black_box_tests:'
            'cobalt_{}_black_box_tests'.format(blaze_platform))
  if test_type == worker_tools.TestTypes.EVERGREEN_TEST:
    return ('//video/youtube/testing/maneki/cobalt/evergreen_tests:'
            'cobalt_{}_evergreen_tests'.format(blaze_platform))
  if test_type == worker_tools.TestTypes.PERFORMANCE_TEST:
    return ('//video/youtube/testing/maneki/cobalt/performance:'
            'cobalt_perf_{}'.format(blaze_platform))
  if test_type == worker_tools.TestTypes.ENDURANCE_TEST:
    return ('//video/youtube/testing/maneki/cobalt/endurance:'
            'cobalt_endurance_test_{}_dynamic'.format(blaze_platform))
  if test_type == worker_tools.TestTypes.INTEGRATION_TEST:
    return ('//video/youtube/tv/hh/test/webdriver:'
            '{}_release_test_suite'.format(blaze_platform))
  raise ValueError('No target associated with test type: ' + test_type)


def _CreateDimensionFlags(dimensions):
  """Create the dimension flags used by mobile harness to select a device.

  Args:
    dimensions: Array of dimensions. Must follow the form "<dim>=<value>".

  Returns:
    Array of strings containing the dimension flags.
  """
  flag_arr = []
  for dimension_value in dimensions:
    try:
      dimension, value = filter(None, dimension_value.split('='))
    except ValueError:
      raise ValueError(  # pylint:disable=raise-missing-from
          'Invalid form for the given dimension: {}'.format(dimension_value))
    flag_arr.append('--test_arg=--dimension_{}={}'.format(dimension, value))
  return flag_arr


def _CreateCobaltFlags(test_type,
                       remote_archive_path,
                       platform,
                       config,
                       loader_platform,
                       loader_config,
                       branch,
                       change_id,
                       builder_name,
                       build_number,
                       submit_results,
                       tag,
                       target_names,
                       cl_timestamp_ms=None):
  """Create the flags passed directly to the Cobalt test.

  Args:
    test_type: worker_tools.TestTypes being triggered
    remote_archive_path: Location of cobalt archive to test. Must be in on gcs.
    platform: Cobalt platform
    config: Config of Cobalt artifacts
    loader_platform: Cobalt platform of the loader. Only applicable in Evergreen
        mode.
    loader_config: Config of Cobalt loader. Only applicable in Evergreen mode.
    branch: Cobalt branch
    change_id: Gerrit change id that triggered this build
    builder_name: Name of builder that is requesting the test
    build_number: Builder's build number
    submit_results: Whether to save the results of a performance test
    tag: Field used to differentiate performance tests in sql tables
    target_names: Specific unit tests to run. If empty, run all.
    cl_timestamp_ms: Timestamp of when the code change was made or when the
        binary was built. If that is not available, set to when test was
        triggered.

  Returns:
    String of arguments passed through mobile harness to Cobalt test code
  """
  flag_arr = []

  def _AddFlag(flag, value, required):
    """Add the given flag to a list of flags."""
    if value is None:
      if required:
        raise ValueError('No value for required Cobalt flag: {}'.format(flag))
    else:
      if isinstance(value, bool):
        if value:
          flag_arr.append('--{}'.format(flag))
        else:
          flag_arr.append('--no{}'.format(flag))
      else:
        flag_arr.append('--{}={}'.format(flag, value))

  if loader_platform and loader_config:
    cobalt_artifact_flagname = _GetCobaltArtifactsFlagname(
        loader_platform, test_type)
    cobalt_config_flagname = _GetConfigFlagName(loader_platform, test_type)
  else:
    cobalt_artifact_flagname = _GetCobaltArtifactsFlagname(platform, test_type)
    cobalt_config_flagname = _GetConfigFlagName(platform, test_type)

  if test_type != worker_tools.TestTypes.PERFORMANCE_TEST:
    _AddFlag(_GetPlatformFlagName(test_type), platform, True)
    _AddFlag(cobalt_config_flagname, config, True)
  _AddFlag(cobalt_artifact_flagname, remote_archive_path, True)

  _AddFlag('loader_platform', loader_platform, False)
  _AddFlag('loader_{}'.format(cobalt_config_flagname), loader_config, False)

  if test_type not in [
      worker_tools.TestTypes.INTEGRATION_TEST,
      worker_tools.TestTypes.PERFORMANCE_TEST
  ]:
    _AddFlag('branch', branch, True)
    _AddFlag('builder_name', builder_name, False)
    _AddFlag('build_number', build_number, False)
    _AddFlag('change_id', change_id, False)

  if test_type == worker_tools.TestTypes.ENDURANCE_TEST and submit_results:
    _AddFlag('submit_test_results', True, True)
    _AddFlag('tag', tag, False)

  if test_type == worker_tools.TestTypes.PERFORMANCE_TEST and submit_results:
    _AddFlag('perfgate_tags', 'execution_mode=manual', True)
    _AddFlag('perfgate_tags', 'builder=%s' % builder_name, False)
    _AddFlag('perfgate_tags', 'build_number=%s' % build_number, False)
    _AddFlag('perfgate_tags', 'branch=%s' % branch, True)
    _AddFlag('save_to_perfgate', True, True)
    _AddFlag('cl', change_id, False)
    _AddFlag('cl_timestamp_ms', cl_timestamp_ms or time.time(), False)

  if test_type == worker_tools.TestTypes.UNIT_TEST and target_names is not None:
    for target_name in target_names:
      _AddFlag('target_name', target_name, False)
  if test_type == worker_tools.TestTypes.INTEGRATION_TEST:
    _AddFlag('device', True, True)

  return '--test_arg=--param_test_flags_guitar=' + ','.join(flag_arr)


def _CreateSpongeLabels(test_type, user, platform, config, build_number, branch,
                        additional_sponge_label_list):
  """Create comma-separated string of sponge labels.

  Args:
    test_type: worker_tools.TestTypes being run
    user: User triggering the test
    platform: Cobalt platform of test
    config: Config of Cobalt artifacts
    build_number: Build number of builder
    branch: Branch of Cobalt artifacts
    additional_sponge_label_list: List of non-standard labels to include

  Returns:
    Comma-separated string of sponge labels
  """
  sponge_label_arr = [test_type]

  def _AddLabel(prefix, value):
    if value is not None:
      sponge_label_arr.append('{}_{}'.format(prefix, value))

  _AddLabel('user', user)
  _AddLabel('platform', platform)
  _AddLabel('config', config)
  _AddLabel('build_number', build_number)
  _AddLabel('branch', branch)
  sponge_label_arr.extend(additional_sponge_label_list)

  return ','.join(sponge_label_arr)


def _SetLoggingConfig(brief):
  """Reset the logging config.

  Args:
    brief: If True, print a bare log line (no prefix)
  """
  # Successive calls to basicConfig will fail if old handlers are not removed.
  for handler in logging.root.handlers[:]:
    logging.root.removeHandler(handler)

  logging.basicConfig(
      stream=sys.stdout,
      level=logging.INFO,
      format=('%(message)s'
              if brief else '[%(filename)s:%(lineno)s] %(message)s'))


def _Execute(cmd, working_dir, dry_run):
  """Run the given command in a subprocess.

  Args:
    cmd: Array of command elements
    working_dir: Directory in which to run the command
    dry_run: If True, do not actually run the command

  Returns:
    Integer return code of command.
  """
  logging.info('Executing: %s', ' '.join(cmd))
  logging.info('Working Directory: %s', working_dir)

  if dry_run:
    logging.info('Dry run skipping execution.')
    return 0

  popen = subprocess.Popen(
      cmd,
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,
      universal_newlines=True,
      cwd=working_dir,
      bufsize=0)
  logging.info('------------------- Begin Subprocess Logs -------------------')
  _SetLoggingConfig(brief=True)
  for stdout_line in iter(popen.stdout.readline, ''):
    logging.info(stdout_line.rstrip())
  _SetLoggingConfig(brief=False)
  logging.info('-------------------- End Subprocess Logs --------------------')
  popen.stdout.close()
  return popen.wait()


def _BuildWatchGatewayCommand(cmd, gateway_exit_status, session_id):
  """
  Create the command that will be watch gateway test

  Args:
    cmd: Command to trigger mobile harness gateway (no arguments)
    gateway_exit_status: Exit status on which to stop watching.
    session_id: ID of the test running on Mobile Harness.

  Returns:
    cmd: Command array to start watching test session.
  """
  cmd.extend(
      ['-exit_on_status', gateway_exit_status, '-session_id', session_id])

  return cmd, '.'


def _BuildTriggerGatewayCommand(cmd, gateway_exit_status, blaze_target, run_as,
                                dimension_flags, cobalt_flags, sponge_labels,
                                status_check_delay, start_timeout, test_timeout,
                                job_timeout, session_id):
  """
  Create the command that will be executed to start a gateway test.

  Args:
    cmd: Command to trigger mobile harness gateway (no arguments)
    gateway_exit_status: When this session status is reached,
                         end process (but not the test)
    blaze_target: Test target on g3 to run
    run_as: Mobile harness user that determines device access
    dimension_flags: Result of _CreateDimensionFlags
    cobalt_flags: Result of _CreateCobaltFlags
    sponge_labels: Result of _CreateSpongeLabels
    status_check_delay: How often the process will check test status
    start_timeout: Max time for test to start
    test_timeout: Max time for test to run
    job_timeout: Max time for gateway to run
    session_id: If passed, watch this session rather than triggering a new one

  Returns:
    cmd: Command array to run to trigger the test, workspace to run in.
  """
  additional_blaze_flags = ' '.join([cobalt_flags] + dimension_flags)
  cmd.extend([
      '-blaze_target', blaze_target, '-additional_blaze_flags',
      additional_blaze_flags, '-sponge_labels', sponge_labels,
      '-exit_on_status', gateway_exit_status, '-status_check_delay',
      status_check_delay, '-run_as', run_as, '-start_timeout', start_timeout,
      '-test_timeout', test_timeout, '-job_timeout', job_timeout
  ])

  if session_id:
    cmd.extend(['-session_id', session_id])

  return cmd, '.'


def _WorkspaceDir(user, workspace_name):
  if workspace_name is None:
    return '/google/src/head/depot/google3/'
  return '/google/src/cloud/%s/%s/google3' % (user, workspace_name)


def _BuildBlazeTestCommand(workspace_name, user, accounting_group, blaze_target,
                           run_as, dimension_flags, cobalt_flags, sponge_labels,
                           test_timeout):
  working_dir = _WorkspaceDir(user, workspace_name)
  cmd = [
      'blaze',
      'test',
      blaze_target,
      '--notest_loasd',
      '--sponge_labels=' + sponge_labels,
      '--accounting_group',
      accounting_group,
      '--test_timeout',
      test_timeout,
      '--test_arg=--run_as=' + run_as,
      cobalt_flags,
  ]
  cmd.extend(dimension_flags)
  return (cmd, working_dir)


class GSUtilDownloadError(Exception):
  """Exception thrown when gsutil fails to download files."""


def _DownloadGatewayFiles(dest_dir, dry_run):
  """Download gateway files from gcs with gsutil.

  Args:
    dest_dir: Directory path to download to
    dry_run: If True, do not actually download files

  Raises:
    GSUtilDownloadError: gsutil failed to download files
  """
  if not os.path.exists(dest_dir):
    os.makedirs(dest_dir)
  gsutil_path = os.getenv(_GSUTIL_PATH_ENV_VAR, 'gsutil')
  cmd = [gsutil_path, 'rsync', '-R', _GCS_GATEWAY_DIR, dest_dir]
  if system_platform.system() == 'Windows':
    cmd = ['python'] + cmd
  return_code = _Execute(cmd, None, dry_run)
  if return_code:
    raise GSUtilDownloadError('Unable to download gateway files.')
  for root, dirs, files in os.walk(dest_dir):
    for d in dirs + files:
      os.chmod(os.path.join(root, d), stat.S_IRWXU)


def _DurationStrToSecondsInt(duration_str):
  """Convert Java Duration strings to a seconds integer.

  Args:
    duration_str: String time duration of the form '[Xh] [Ym] [Zs]'

  Returns:
    Integer number of seconds equal to given duration string

  Raises:
    ValueError when the string is malformed
  """
  if duration_str:
    match = re.match(
        r'(?:(?P<hours>\d+)h)?'
        r'(?:(?P<minutes>\d+)m)?'
        r'(?:(?P<seconds>\d+)s)?', duration_str)
    if match:
      seconds = 0
      valid_term = False
      if match.group('hours'):
        valid_term = True
        seconds += int(match.group('hours')) * 60 * 60
      if match.group('minutes'):
        valid_term = True
        seconds += int(match.group('minutes')) * 60
      if match.group('seconds'):
        valid_term = True
        seconds += int(match.group('seconds'))

    if valid_term:
      return seconds

  raise ValueError('Unable to convert duration to seconds: %s' % duration_str)


def _FindGatewayFiles(gateway_dir, resync_gateway, dry_run):
  """Find the directory containing gateway files.

  Try to use the given directory,
  the directory indicated by the _GATEWAY_PAT_ENV_VAR env var,
  then the default directory.

  If the files are not present in the final directory, download them.

  Args:
    gateway_dir: Local directory in which to store gateway files
    resync_gateway: Sync gateway files from gcs, even if already present
    dry_run: If True, do not download files, even if missing

  Returns:
    cmd: Command to trigger mobile harness gateway (no arguments).
  """

  if gateway_dir is None:
    gateway_dir = os.getenv(_GATEWAY_PAT_ENV_VAR)

  if gateway_dir is None:
    gateway_dir = _GetDefaultGatewayDir()

  gateway_dir = os.path.expanduser(gateway_dir)

  alpn_jar_path = os.path.join(gateway_dir, _ALPN_JAR_NAME)
  gateway_jar_path = os.path.join(gateway_dir, _GATEWAY_JAR_NAME)

  if (resync_gateway or not os.path.isdir(gateway_dir) or
      not os.path.isfile(alpn_jar_path) or
      not os.path.isfile(gateway_jar_path)):
    logging.warning('Syncing gateway files from gcs.')
    _DownloadGatewayFiles(gateway_dir, dry_run)

  cmd = [_GetJavaPath(), '-jar', gateway_jar_path]

  if system_platform.system() != 'Linux':
    cmd.insert(1, '-Xbootclasspath/p:' + alpn_jar_path)

  return cmd


def _EnforceMinimumJobTimeout(start_timeout, test_timeout, job_timeout):
  """Ensure job_timeout is large enough to run all job stages.

  The job_timeout should always be large enough to account for the time to set
  up the job, find a device, and run the test.

  Args:
    start_timeout: Max time to wait to find a device as a duration string.
    test_timeout: Max time to wait for test to run as a duration string.
    job_timeout: Max time to wait for the entire job to run.

  Returns:
    A sufficiently large job_timeout duration string to handle all job stages.
  """
  min_job_timeout_sec = (
      _DurationStrToSecondsInt(start_timeout) +
      _DurationStrToSecondsInt(test_timeout) + _JOB_SETUP_TIME_SEC)
  job_timeout_sec = _DurationStrToSecondsInt(job_timeout)

  if job_timeout_sec < min_job_timeout_sec:
    logging.info('Enforcing minimum job_timeout: %s', min_job_timeout_sec)
    return '%ss' % min_job_timeout_sec

  # Return the job_timeout in it's original format if unmodified.
  return job_timeout


def _FindGatewayServiceFiles():
  """
  Generate gRPC code for Mobile Harness Gateway service if
  necessary, and return command to trigger service clienti (without
  arguments).
  """
  if (not os.path.isfile(_GATEWAY_SERVICE_PROTO_CODE) or
      not os.path.isfile(_GATEWAY_SERVICE_GRPC_CODE)):
    logging.info('Generating Mobile Harness Gateway service gRPC code...')
    from grpc.tools import protoc  # pylint: disable=import-outside-toplevel
    protoc.main([
        'grpc_tools.protoc', '-I.', '--python_out=.', '--grpc_python_out=.',
        _GATEWAY_SERVICE_PROTO
    ])

  return [sys.executable, '-u', _GATEWAY_SERVICE_CLIENT]


def main():

  class HelpfulParser(argparse.ArgumentParser):
    """Logs the epilog when there is a parse error."""

    def error(self, message):
      sys.stderr.write('error: %s\n' % message)
      self.print_help()
      sys.exit(2)

  _SetLoggingConfig(brief=False)

  parser = HelpfulParser(
      epilog=(r'Example: ./mobile_harness_gateway.py --test_type=unit_test '
              r'--remote_archive_path=gs://mh_testing_tmp/'
              r'_test_try_raspi-2_devel_tests/2019-11-05/16/unit/raspi-2_devel/'
              r'cobalt_archive.tar '
              r'--platform=raspi-2 --branch=COBALT --config=devel'),
      formatter_class=argparse.RawDescriptionHelpFormatter)

  parser.add_argument(
      '--test_type',
      type=str,
      help='Type of test to run. Valid values: ' +
      ','.join(worker_tools.TestTypes.VALID_TYPES))

  # Blaze test flags
  parser.add_argument(
      '--use_blaze',
      action='store_true',
      help='Trigger the test with blaze instead of '
      'the mobile harness gateway.')
  parser.add_argument(
      '--workspace_name',
      type=str,
      default=None,
      help='If use_blaze is passed, this workspace will be '
      'used to run the test. The test will be run at '
      'head by default.')

  # Gateway flags
  parser.add_argument(
      '--gateway_dir_path',
      type=str,
      default=None,
      help='Path to gateway files used to interact '
      'with mobile harness.')
  parser.add_argument(
      '--gateway_exit_status',
      type=str,
      default='DONE',
      help='When using gateway, exit when the sessions hits '
      'this status. Any of the values of TestStatus in '
      'the gateway. E.G. NEW, ASSIGNED, RUNNING, DONE.')
  parser.add_argument(
      '--status_check_delay',
      type=str,
      default='1m',
      help='Time between gateway status checks to see if '
      'exit state has been reached.'
      'Include units. E.G. 2h 10m 3s')
  parser.add_argument(
      '--session_id',
      type=str,
      default=None,
      help='Session id of a previously triggered mobile '
      'harness test. If passed, the test will not be '
      'triggered, but will be watched until the exit '
      'status is reached.')
  parser.add_argument(
      '--resync_gateway',
      action='store_true',
      help='Resync files from gcs to local gateway directory.')

  # Cobalt flags
  parser.add_argument(
      '--remote_archive_path',
      type=str,
      help='Path to Cobalt archive to be tested. '
      'Must be on gcs.')
  parser.add_argument(
      '--platform', type=str, help='Platform this test was built for.')
  parser.add_argument('--config', type=str, help='Cobalt config being tested.')
  parser.add_argument(
      '--loader_platform',
      type=str,
      help='Platform of the loader to run the test. Only '
      'applicable in Evergreen mode.')
  parser.add_argument(
      '--loader_config',
      type=str,
      help='Cobalt config of the loader to run the test. Only '
      'applicable in Evergreen mode.')
  parser.add_argument(
      '--branch',
      type=str,
      default='COBALT',
      help='Cobalt branch being tested.')
  parser.add_argument(
      '--tag',
      type=str,
      default=None,
      help='Value saved with performance results. '
      'Indicates why this test was triggered.')
  parser.add_argument(
      '--run_as',
      type=str,
      default='youtube-cobalt-mh-owner',
      help='User credentials to use to access '
      'mobile harness devices.')
  parser.add_argument(
      '--accounting_group',
      type=str,
      default='youtube-cobalt',
      help='Accounting group that will pay for '
      'mobile harness usage.')
  parser.add_argument(
      '--sponge_label',
      type=str,
      default=[],
      action='append',
      help='Additional sponge labels to assign to the test.')
  parser.add_argument(
      '--start_timeout',
      type=str,
      default='15m',
      help='Max time to wait for a test to start before '
      'timeout. E.G. "2h10m3s"')
  parser.add_argument(
      '--test_timeout',
      type=str,
      default=None,
      help='Max time for test to run before timeout. '
      'E.G. "2h10m3s"')
  parser.add_argument(
      '--job_timeout',
      type=str,
      default='0s',
      help='Max time to set up the job, find a device, and run'
      'the test. If missing, this will be set based on '
      'start_timeout and test_timeout. E.G. "2h10m3s"')
  parser.add_argument(
      '--dimension',
      type=str,
      action='append',
      help='Mobile harness dimension used to select a device. '
      'Must have the following form: <dimension>=<value>.'
      ' E.G. "id=Maneki_Microsoft_XBoxOneS_1".')

  # Cobalt unit test flags
  parser.add_argument(
      '--target_name',
      type=str,
      action='append',
      help='Specific unit test to run. Run all if not passed.')

  # Cobalt performance test flags
  parser.add_argument(
      '--submit_results',
      action='store_true',
      help='If passed, performance results will be saved '
      'to a database.')
  parser.add_argument(
      '--change_id',
      type=str,
      default=None,
      help='ChangeId that triggered this test, if any. '
      'Saved with performance test results.')
  parser.add_argument(
      '--builder_name',
      type=str,
      default=None,
      help='Name of the builder that built the artifacts, '
      'if any. Saved with performance test results.')
  parser.add_argument(
      '--build_number',
      type=str,
      default=None,
      help='Build number associated with the buildbot build, '
      'if any. Saved with performance test results.')
  parser.add_argument(
      '--cl_timestamp_ms',
      type=str,
      default=None,
      help='When the cl was submitted or when the binary '
      'was built.')

  parser.add_argument(
      '--dry_run',
      action='store_true',
      help='If passed, print executed commands '
      'but to not run them.')
  parser.add_argument(
      '--docker',
      action='store_true',
      help='Use if running Buildbot in docker.')

  args = parser.parse_args()

  if args.session_id:
    if args.use_blaze:
      logging.warning(
          '--use_blaze will be ignored for watching an existing session')
    if args.docker:
      cmd = _FindGatewayServiceFiles()
    else:
      cmd = _FindGatewayFiles(args.gateway_dir_path, args.resync_gateway,
                              args.dry_run)
    cmd, working_dir = _BuildWatchGatewayCommand(cmd, args.gateway_exit_status,
                                                 args.session_id)
    if args.docker:
      cmd = _TokenizeCmd(cmd)

    return _Execute(cmd, working_dir, args.dry_run)

  if (args.test_type is None or args.remote_archive_path is None or
      args.platform is None or args.config is None):
    raise ValueError(
        'In the absence of --session_id, this script requires: '
        '--test_type, --remote_archive_path, --platform, and --config.')

  user_name = getpass.getuser()
  # The blaze target is named after the platform that we are intending to run
  # on (e.g. "raspi-2"), so in Evergreen mode we must use the loader platform.
  if args.loader_platform and args.loader_config:
    blaze_target = _GetBlazeTarget(args.test_type, args.loader_platform)
  else:
    blaze_target = _GetBlazeTarget(args.test_type, args.platform)

  if args.test_timeout is None:
    args.test_timeout = str(
        test_timeouts.GetPlatformTestTimeoutMinutesInt(args.platform,
                                                       args.test_type)) + 'm'

  sponge_labels = _CreateSpongeLabels(args.test_type, user_name, args.platform,
                                      args.config, args.build_number,
                                      args.branch, args.sponge_label)

  dimensions = args.dimension or []
  if args.test_type == worker_tools.TestTypes.INTEGRATION_TEST:
    dimensions.append('platform=' + args.platform)
  dimension_flags = _CreateDimensionFlags(dimensions)

  cobalt_flags = _CreateCobaltFlags(
      args.test_type, args.remote_archive_path, args.platform, args.config,
      args.loader_platform, args.loader_config, args.branch, args.change_id,
      args.builder_name, args.build_number, args.submit_results, args.tag,
      args.target_name, args.cl_timestamp_ms)

  if args.use_blaze:
    cmd, working_dir = _BuildBlazeTestCommand(
        args.workspace_name, user_name, args.accounting_group, blaze_target,
        args.run_as, dimension_flags, cobalt_flags, sponge_labels,
        str(_DurationStrToSecondsInt(args.test_timeout)))
  else:
    if args.docker:
      cmd = _FindGatewayServiceFiles()
    else:
      cmd = _FindGatewayFiles(args.gateway_dir_path, args.resync_gateway,
                              args.dry_run)

    job_timeout_duration = _EnforceMinimumJobTimeout(args.start_timeout,
                                                     args.test_timeout,
                                                     args.job_timeout)

    cmd, working_dir = _BuildTriggerGatewayCommand(
        cmd, args.gateway_exit_status, blaze_target, args.run_as,
        dimension_flags, cobalt_flags, sponge_labels, args.status_check_delay,
        args.start_timeout, args.test_timeout, job_timeout_duration,
        args.session_id)

  if args.docker:
    cmd = _TokenizeCmd(cmd)

  return _Execute(cmd, working_dir, args.dry_run)


if __name__ == '__main__':
  sys.exit(main())
