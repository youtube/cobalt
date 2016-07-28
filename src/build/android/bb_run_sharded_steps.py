#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to shard build bot steps and save results to disk.

Our buildbot infrastructure requires each slave to run steps serially.
This is sub-optimal for android, where these steps can run independently on
multiple connected devices.

The buildbots will run this script multiple times per cycle:
- First, without params: all steps will be executed in parallel using all
connected devices. Step results will be pickled to disk (each step has a unique
name).
The buildbot will treat this step as a regular step, and will not process any
graph data.

- Then, with -p STEP_NAME: at this stage, we'll simply print the file with the
step results previously saved. The buildbot will then process the graph data
accordingly.

The JSON config contains is a file containing a dictionary in the format:
{
  'step_name_foo': 'script_to_execute foo',
  'step_name_bar': 'script_to_execute bar'
}

Note that script_to_execute necessarily have to take at least the following
options:
  --device: the serial number to be passed to all adb commands.
  --keep_test_server_ports: indicates it's being run as a shard, and shouldn't
  reset test server port allocation.
"""


import datetime
import json
import logging
import multiprocessing
import optparse
import pexpect
import pickle
import os
import signal
import shutil
import sys

from pylib import android_commands
from pylib import cmd_helper
from pylib import constants
from pylib import ports


_OUTPUT_DIR = os.path.join(constants.CHROME_DIR, 'out', 'step_results')


def _SaveResult(result):
  with file(os.path.join(_OUTPUT_DIR, result['name']), 'w') as f:
    f.write(pickle.dumps(result))


def _RunStepsPerDevice(steps):
  results = []
  for step in steps:
    start_time = datetime.datetime.now()
    print 'Starting %s: %s %s at %s' % (step['name'], step['cmd'],
                                        start_time, step['device'])
    output, exit_code  = pexpect.run(
        step['cmd'], cwd=os.path.abspath(constants.CHROME_DIR),
        withexitstatus=True, logfile=sys.stdout, timeout=1800,
        env=os.environ)
    end_time = datetime.datetime.now()
    print 'Finished %s: %s %s at %s' % (step['name'], step['cmd'],
                                        end_time, step['device'])
    result = {'name': step['name'],
              'output': output,
              'exit_code': exit_code or 0,
              'total_time': (end_time - start_time).seconds,
              'device': step['device']}
    _SaveResult(result)
    results += [result]
  return results


def _RunShardedSteps(steps, devices):
  assert steps
  assert devices, 'No devices connected?'
  if os.path.exists(_OUTPUT_DIR):
    assert '/step_results' in _OUTPUT_DIR
    shutil.rmtree(_OUTPUT_DIR)
  if not os.path.exists(_OUTPUT_DIR):
    os.makedirs(_OUTPUT_DIR)
  step_names = sorted(steps.keys())
  all_params = []
  num_devices = len(devices)
  shard_size = (len(steps) + num_devices - 1) / num_devices
  for i, device in enumerate(devices):
    steps_per_device = []
    for s in steps.keys()[i * shard_size:(i + 1) * shard_size]:
      steps_per_device += [{'name': s,
                            'device': device,
                            'cmd': steps[s] + ' --device ' + device +
                            ' --keep_test_server_ports'}]
    all_params += [steps_per_device]
  print 'Start sharding (note: output is not synchronized...)'
  print '*' * 80
  start_time = datetime.datetime.now()
  pool = multiprocessing.Pool(processes=num_devices)
  async_results = pool.map_async(_RunStepsPerDevice, all_params)
  results_per_device = async_results.get(999999)
  end_time = datetime.datetime.now()
  print '*' * 80
  print 'Finished sharding.'
  print 'Summary'
  total_time = 0
  for results in results_per_device:
    for result in results:
      print('%s : exit_code=%d in %d secs at %s' %
            (result['name'], result['exit_code'], result['total_time'],
             result['device']))
      total_time += result['total_time']
  print 'Step time: %d secs' % ((end_time - start_time).seconds)
  print 'Bots time: %d secs' % total_time
  # No exit_code for the sharding step: the individual _PrintResults step
  # will return the corresponding exit_code.
  return 0


def _PrintStepOutput(step_name):
  file_name = os.path.join(_OUTPUT_DIR, step_name)
  if not os.path.exists(file_name):
    print 'File not found ', file_name
    return 1
  with file(file_name, 'r') as f:
    result = pickle.loads(f.read())
  print result['output']
  return result['exit_code']


def _KillPendingServers():
  for retry in range(5):
    for server in ['lighttpd', 'web-page-replay']:
      pids = cmd_helper.GetCmdOutput(['pgrep', '-f', server])
      pids = [pid.strip() for pid in pids.split('\n') if pid.strip()]
      for pid in pids:
        try:
          logging.warning('Killing %s %s', server, pid)
          os.kill(int(pid), signal.SIGQUIT)
        except Exception as e:
          logging.warning('Failed killing %s %s %s', server, pid, e)


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('-s', '--steps',
                    help='A JSON file containing all the steps to be '
                         'sharded.')
  parser.add_option('-p', '--print_results',
                    help='Only prints the results for the previously '
                         'executed step, do not run it again.')
  options, urls = parser.parse_args(argv)
  if options.print_results:
    return _PrintStepOutput(options.print_results)

  # At this point, we should kill everything that may have been left over from
  # previous runs.
  _KillPendingServers()

  # Reset the test port allocation. It's important to do it before starting
  # to dispatch any step.
  if not ports.ResetTestServerPortAllocation():
    raise Exception('Failed to reset test server port.')

  # Sort the devices so that we'll try to always run a step in the same device.
  devices = sorted(android_commands.GetAttachedDevices())
  if not devices:
    print 'You must attach a device'
    return 1

  with file(options.steps, 'r') as f:
    steps = json.load(f)
  return _RunShardedSteps(steps, devices)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
