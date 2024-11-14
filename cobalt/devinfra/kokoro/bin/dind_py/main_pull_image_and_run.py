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
Main script for the Kokoro Job that only pulls and image and runs Cobalt.

Implements a periodic check for the image on the registry with a timeout.

   Kokoro Instance
   └── Generic DinD Image
       ├── dind_builder_runner.sh
       │   ├── configure_environment (common.sh)
       │   ├── main_build_image_and_run.py
       │   │   └── Specific Cobalt Image
       │   │       └── dind_build.sh
       │   └── run_package_release_pipeline (common.sh)
       └── dind_runner.sh
           ├── configure_environment (common.sh)
           ├── main_pull_image_and_run.py                       <= THIS SCRIPT
           │   └── Specific Cobalt Image
           │       └── dind_build.sh
           └── run_package_release_pipeline (common.sh)
"""

import logging
import subprocess
import sys
import time

import dind_common as dind
import utils

_CYCLE_IMAGE_WAIT_SECS = 1 * 60
_INITIAL_IMAGE_WAIT_SECS = 2 * 60
_TOTAL_IMAGE_WAIT_SECS = 20 * 60


def main():
  args = utils.get_args_from_env()

  target_image = f'{args.registry_path}/{args.registry_img}:{args.git_sha}'
  default_crosstool_image = (
      f'{args.registry_path}/{args.registry_img}:{dind.CROSSTOOL_IMAGE_TAG}')
  failure_image = f'{args.registry_path}/{dind.get_failure_image()}'

  # This job is run in parallel with the image-builder job. We introduce a short
  # wait here to allow the image-builder job a head start to finish its tasks,
  # in the case when the image has no changes and just needs to be retagged.
  utils.logging_info_spacer('Initial wait before checking for image')
  time.sleep(_INITIAL_IMAGE_WAIT_SECS)

  utils.logging_info_spacer('Pulling Image from Registry')
  if args.platform == dind.LINUX_CLANG_CROSSTOOL:
    # TODO(b/b/350542958): Currently we're missing some implementation changes
    # to enable building images for Crosstool within the Kokoro Job. For now we
    # pull the default pre-built image.
    utils.logging_info_spacer(
        'Crosstool detected. Pulling backup image instead.')
    dind.pull_image(default_crosstool_image)
    dind.tag_image(default_crosstool_image, target_image)
  else:
    image_pulled = False
    start_time = time.time()
    num_pull_attempts = 0
    while not (image_pulled or
               (time.time() - start_time) > _TOTAL_IMAGE_WAIT_SECS):
      num_pull_attempts += 1
      try:
        logging.info('Pull Attempt starting ...')
        dind.pull_image(target_image)
        image_pulled = True
      except subprocess.CalledProcessError:
        logging.info('Failed pull attempt, sleeping ...')
        time.sleep(_CYCLE_IMAGE_WAIT_SECS)
    logging.info('Attempted %d pulls from the registry', num_pull_attempts)
    if not image_pulled:
      logging.warning('Did not successfully pull target image before timeout')
      dind.pull_image(failure_image)
      dind.tag_image(failure_image, target_image)

  try:
    utils.logging_info_spacer('Running Cobalt Build')
    dind.run_cobalt_build(target_image, args.src_root)
  except subprocess.CalledProcessError:
    logging.info('Python script execution for DinD failed. See logs above')
    # Signals fail to enclosing script, which is picked up by Kokoro.
    sys.exit(1)


if '__main__' == __name__:
  utils.set_logging_format()
  main()
