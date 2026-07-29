#!/usr/bin/env python3
#
# Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
"""
Implements all DinD methods for running builds for Docker images or Cobalt
"""

import subprocess
import logging
import os

# Environment Variables to fetch values for.
_GIT_SHA_VAR = 'KOKORO_GIT_COMMIT_src'
_REGISTRY_PATH_VAR = 'REGISTRY_PATH'
_REGISTRY_IMAGE_VAR = 'REGISTRY_IMAGE_NAME'
_PLATFORM_VAR = 'PLATFORM'
_WORKSPACE_COBALT_VAR = 'WORKSPACE_COBALT'

# These values are used to determine Floating Tag parameters for images used in
# Docker/Cobalt build steps.
_BASE_BRANCH_VAR = 'KOKORO_GOB_BRANCH_src'
_GERRIT_CHANGE_NUMBER_VAR = 'KOKORO_GERRIT_CHANGE_NUMBER_src'
_KOKORO_JOB_TYPE_VAR = 'KOKORO_ROOT_JOB_TYPE'


class EnvArgs(object):
  """
  Simple struct to hold all relevant arguments that come from environment vars.
  """
  # The Git SHA for the current checked out HEAD of the Cobalt repo
  git_sha = None

  # Path to the registry and bucket which constitutes the fully-specified name:
  # [REGISTRY_PATH]/[IMAGE_NAME]:[IMAGE_TAG]
  registry_path = ''

  # Name of the image which is part of the fully-specified name:
  # [REGISTRY_PATH]/[IMAGE_NAME]:[IMAGE_TAG]
  registry_img = ''

  # Target platform to build Cobalt for
  platform = None

  # Path to the checkout of Cobalt repo's root
  src_root = ''

  # Branch name for the Cobalt checkout. This is one of the following:
  #
  # * "COBALT", which represents trunk or main
  # * "NN.lts.1+", which is any numbered LTS release branch.
  #
  # This value is relevant for determining the floating tag that references the
  # latest valid Docker image to use in postsubmit builds of Cobalt.
  base_branch_name = ''

  # This value is the cardinal number of the CL uploaded to Gerrit. This value
  # is set when a change is uploaded to Gerrit for code review and uniquely
  # identifies the change being tested. Additionally, all the PRs that are
  # uploaded to Github are given a corresponding Gerrit CL.
  #
  # This value is used to set a floating tag for Docker images that can be
  # reused for image build cache purposes when running presubmit tests.
  gerrit_change_number = ''

  # Whether the root job of the Kokoro job chain that triggered this build is a
  # postsubmit card. This controls what type of floating tag is applied to any
  # built docker image.
  is_postsubmit = False

  def __init__(self):
    self.git_sha = os.environ[_GIT_SHA_VAR]
    self.registry_path = os.environ[_REGISTRY_PATH_VAR]
    self.platform = os.environ[_PLATFORM_VAR]

    self.registry_img = os.environ.get(_REGISTRY_IMAGE_VAR)
    if self.registry_img is None:
      self.registry_img = f'cobalt-build-{self.platform}'

    self.src_root = os.environ[_WORKSPACE_COBALT_VAR]
    self.base_branch_name = os.environ.get(_BASE_BRANCH_VAR)
    # Normalize LTS branch names: 24.lts.1+ => 24_lts
    self.base_branch_name = self.base_branch_name.replace('.lts.1+', '_lts')
    if self.base_branch_name is None:
      raise ValueError('Branch name was not set in the environment.')
    self.gerrit_change_number = int(
        os.environ.get(_GERRIT_CHANGE_NUMBER_VAR, 0))

    job_type = os.environ.get(_KOKORO_JOB_TYPE_VAR, '')
    self.is_postsubmit = job_type in ['CONTINUOUS_INTEGRATION', 'RELEASE']


def get_args_from_env():
  """
  Helper method to retrieve all relevant arguments from the kokoro environment.
  """
  return EnvArgs()


def logging_info_spacer(msg):
  """
  Prints a message to the INFO channel with ASCII lines for emphasis.
  """
  msg_len = len(msg)
  spacer = '*' * msg_len
  logging.info(spacer)
  logging.info(msg)
  logging.info(spacer)


def exec_cmd(command):
  """
  Helper method to run commands as string in the shell via subprocess.
  """
  try:
    output = subprocess.check_output(command, shell=True, encoding='UTF-8')
    # Leading newline inserted to ensure captured logs appear as if in stdout.
    logging.info('%s\n%s', command, output)
  except subprocess.CalledProcessError as e:
    logging.error('%s\n%s', command, e.output)
    raise e


def set_logging_format():
  """
  Sets up standard config values for logging.
  """
  logging_format = ('[%(asctime)s:%(levelname)s:%(filename)s:%(lineno)s]' +
                    ' ' + '%(message)s')
  logging.basicConfig(
      level=logging.INFO, format=logging_format, datefmt='%H:%M:%S')
