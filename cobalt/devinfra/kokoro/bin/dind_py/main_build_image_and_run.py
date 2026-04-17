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
Main script that pulls, builds, tags, and pushes the corresponding Docker image
for a given platform. Then it reuses the image for a Cobalt build.

When it fails to build the image, a prebuilt failure image placeholder is
downloaded and reuploaded as the resulting image. Other Kokoro jobs that detect
this image will fail when the image is run.

   Kokoro Instance
   └── Generic DinD Image
       ├── dind_builder_runner.sh
       │   ├── main_build_image_and_run.py                      <= THIS SCRIPT
       │   │   └── Specific Cobalt Image
       │   │       └── dind_build.sh
       │   └── run_package_release_pipeline (common.sh)
       └── dind_runner.sh
           ├── main_pull_image_and_run.py
           │   └── Specific Cobalt Image
           │       └── dind_build.sh
           └── run_package_release_pipeline (common.sh)
"""

import logging
import subprocess
import sys
import re
import time

import dind_common as dind
import utils


def try_pull_image(image):
  """
  Helper method to try pulling an image, and silence the CalledProcessError
  exception if it is raised.
  """
  # This step may fail if there is no image, so we suppress the error.
  try:
    dind.pull_image(image)
  except subprocess.CalledProcessError:
    pass


def main():
  args = utils.get_args_from_env()

  target_image = f'{args.registry_path}/{args.registry_img}:{args.git_sha}'
  default_crosstool_image = (
      f'{args.registry_path}/{args.registry_img}:{dind.CROSSTOOL_IMAGE_TAG}')
  failure_image = f'{args.registry_path}/{dind.get_failure_image()}'

  # The floating tag is a an image tag that indicates this image was built on
  # the HEAD of the current branch (for postsubmits), or on the latest patchset
  # for a gerrit change. This allows it to be pulled by future Kokoro jobs that
  # can reuse the image layers in the docker build.
  floating_tag = 'FLOATING_TAG_PLACEHOLDER'
  if args.is_postsubmit:
    # Check for postsubmit flags before assigning the floating tag for a branch.
    # Sanitize the branch name to be a valid Docker tag.  Replace any characters
    # that are not letters, numbers, underscores, periods, or dashes with an
    # underscore. Also, added a step to truncate the tag to 128 characters to
    # ensure it complies wiht the Docker's tag length limit.  This should cover
    # all potential illegal characters for a Docker tag.
    sanitized_branch_name = re.sub(r'[^a-zA-Z0-9_.-]+', '_',
                                   args.base_branch_name)
    floating_tag = f'branch-{sanitized_branch_name}'[:128]
  else:
    floating_tag = f'gerrit-cl-{args.gerrit_change_number}'
  floating_image = f'{args.registry_path}/{args.registry_img}:{floating_tag}'

  try:
    utils.logging_info_spacer('Pulling Image from Registry')
    try_pull_image(target_image)
    try_pull_image(floating_image)

    if args.platform == dind.LINUX_CLANG_CROSSTOOL:
      # TODO(b/350542958): Currently we're missing some implementation changes
      # to enable building images for Crosstool within the Kokoro Job. For now
      # we pull the default pre-built image.
      utils.logging_info_spacer('Skipping Docker build, pulling backup image.')
      dind.pull_image(default_crosstool_image)
      dind.tag_image(default_crosstool_image, target_image)
      dind.push_image(target_image)
    else:
      utils.logging_info_spacer('Building Docker Image')
      build_start_time = time.time()
      try:
        dind.run_docker_build(args.platform, target_image,
                              args.src_root + '/docker-compose.yaml')
        dind.tag_image(target_image, floating_image)
        dind.push_image(floating_image)
        docker_build_duration = time.time() - build_start_time
        logging.info('Docker build took %.2f seconds', docker_build_duration)
      except subprocess.CalledProcessError:
        # The docker build failed so we download a prebuilt failure image, and
        # re-tag it as the new image. Then upload it which then makes it
        # available to non-docker-build jobs.
        utils.logging_info_spacer(
            'Docker Build failed. Uploading Failure Image.')
        dind.pull_image(failure_image)
        dind.tag_image(failure_image, target_image)
      # Upload the newly built (or re-tagged) image to the registry to become
      # available to Kokoro jobs that are waiting on the image.
      dind.push_image(target_image)

    # Regardless of whether the image was built in the previous step, or if a
    # failure image was downloaded instead. Running the Cobalt Build with the
    # failure placeholder image will cause the Cobalt build to fail.
    utils.logging_info_spacer('Running Cobalt Build')
    dind.run_cobalt_build(target_image, args.src_root)
  except subprocess.CalledProcessError:
    logging.info('Python script execution for DinD failed. See logs above')
    # Signals fail to enclosing script, which is picked up by Kokoro.
    sys.exit(1)


if '__main__' == __name__:
  utils.set_logging_format()
  main()
