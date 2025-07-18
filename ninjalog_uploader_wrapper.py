#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import json
import os
import platform
import subprocess
import sys

import ninjalog_uploader
import subprocess2

THIS_DIR = os.path.dirname(__file__)
UPLOADER = os.path.join(THIS_DIR, 'ninjalog_uploader.py')
CONFIG = os.path.join(THIS_DIR, 'ninjalog.cfg')
VERSION = 3


def LoadConfig():
  if os.path.isfile(CONFIG):
    with open(CONFIG, 'r') as f:
      try:
        config = json.load(f)
      except Exception:
        # Set default value when failed to load config.
        config = {
            'is-googler': ninjalog_uploader.IsGoogler(),
            'countdown': 10,
            'version': VERSION,
        }

      if config['version'] == VERSION:
        config['countdown'] = max(0, config['countdown'] - 1)
        return config

  return {
      'is-googler': ninjalog_uploader.IsGoogler(),
      'countdown': 10,
      'version': VERSION,
  }


def SaveConfig(config):
  with open(CONFIG, 'w') as f:
    json.dump(config, f)


def ShowMessage(countdown):
  whitelisted = '\n'.join(
      ['  * %s' % config for config in ninjalog_uploader.ALLOWLISTED_CONFIGS])
  print("""
Your ninjalog will be uploaded to build stats server. The uploaded log will be
used to analyze user side build performance.

The following information will be uploaded with ninjalog.
* OS (e.g. Win, Mac or Linux)
* number of cpu cores of building machine
* build targets (e.g. chrome, browser_tests)
* parallelism passed by -j flag
* following build configs
%s

Uploading ninjalog will be started after you run autoninja another %d time(s).

If you don't want to upload ninjalog, please run following command.
$ python3 %s opt-out

If you want to allow upload ninjalog from next autoninja run, please run the
following command.
$ python3 %s opt-in

If you have questions about this, please send mail to infra-dev@chromium.org

You can find a more detailed explanation in
%s
or
https://chromium.googlesource.com/chromium/tools/depot_tools/+/main/ninjalog.README.md

""" % (whitelisted, countdown, __file__, __file__,
       os.path.abspath(os.path.join(THIS_DIR, "ninjalog.README.md"))))


def main():
  config = LoadConfig()

  if len(sys.argv) == 2 and sys.argv[1] == 'opt-in':
    config['opt-in'] = True
    config['countdown'] = 0
    SaveConfig(config)
    print('ninjalog upload is opted in.')
    return 0

  if len(sys.argv) == 2 and sys.argv[1] == 'opt-out':
    config['opt-in'] = False
    SaveConfig(config)
    print('ninjalog upload is opted out.')
    return 0

  if 'opt-in' in config and not config['opt-in']:
    # Upload is opted out.
    return 0

  if not config.get("is-googler", False):
    # Not googler.
    return 0

  if config.get("countdown", 0) > 0:
    # Need to show message.
    ShowMessage(config["countdown"])
    # Only save config if something has meaningfully changed.
    SaveConfig(config)
    return 0

  if len(sys.argv) == 1:
    # dry-run for debugging.
    print("upload ninjalog dry-run")
    return 0

  # Run upload script without wait.
  devnull = open(os.devnull, "w")
  creationnflags = 0
  if platform.system() == 'Windows':
    creationnflags = subprocess.CREATE_NEW_PROCESS_GROUP
  subprocess2.Popen([sys.executable, UPLOADER] + sys.argv[1:],
                    stdout=devnull,
                    stderr=devnull,
                    creationflags=creationnflags)


if __name__ == '__main__':
  sys.exit(main())
