#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys
import subprocess
import time

def GetChromiumSrcDir():
  return os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir,
                                      os.pardir, os.pardir, os.pardir,
                                      os.pardir))

def GetIosDir():
  return os.path.join(GetChromiumSrcDir(), 'ios')

sys.path.append(os.path.join(GetIosDir(), 'build', 'bots', 'scripts'))

import constants
import iossim_util
import test_apps
import xcodebuild_runner

def GetDefaultBuildDir():
  return os.path.join(GetChromiumSrcDir(), 'out', 'Release')

parser=argparse.ArgumentParser(
    formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument('--port', default='9999',
    help='The port to listen on for WebDriver commands')
parser.add_argument('--build-dir', default=GetDefaultBuildDir(),
    help='Chrome build directory')
parser.add_argument('--out-dir', default='/tmp/cwt_chromedriver',
    help='Output directory for CWTChromeDriver\'s dummy test case')
parser.add_argument('--os', default='15.5', help='iOS version')
parser.add_argument('--device', default='iPhone 11 Pro', help='Device type')
parser.add_argument('--asan-build', help='Use ASan-related libraries',
    dest='asan_build', action='store_true')
parser.set_defaults(asan_build=False)
parser.add_argument('--enable-chrome-logs', action='store_true',
    help='No-op, recognized as an argument for compatiblity with ChromeDriver')
parser.add_argument('--verbose', action='store_true',
    help='No-op, recognized as an argument for compatiblity with ChromeDriver')
parser.add_argument('--log-path',
    help='No-op, recognized as an argument for compatiblity with ChromeDriver')
args=parser.parse_args()

test_app = os.path.join(
    args.build_dir, 'ios_cwt_chromedriver_tests_module-Runner.app')
host_app = os.path.join(args.build_dir, 'ios_cwt_chromedriver_tests.app')
destination = iossim_util.get_simulator(args.device, args.os)

if not os.path.exists(args.out_dir):
  os.mkdir(args.out_dir)

# Make sure each run produces a unique output directory, since reusing an
# existing directory will cause CWTChromeDriver's dummy test case to get
# skipped, meaning that CWTChromeDriver's http server won't get launched.
output_directory = os.path.join(args.out_dir, 'run%d' %  int(time.time()))

inserted_libs = []
if args.asan_build:
  inserted_libs = [os.path.join(args.build_dir,
      'libclang_rt.asan_iossim_dynamic.dylib')]

egtests_app = test_apps.EgtestsApp(
    egtests_app=test_app, test_args=['--port %s' % args.port],
    host_app_path=host_app, inserted_libs=inserted_libs)

launch_command = xcodebuild_runner.LaunchCommand(egtests_app, destination,
    shards=1, retries=1, readline_timeout=constants.READLINE_TIMEOUT,
    out_dir=output_directory,
    cert_path='../../wpt_tools/wpt/tools/certs/cacert.pem')

launch_command.launch()
