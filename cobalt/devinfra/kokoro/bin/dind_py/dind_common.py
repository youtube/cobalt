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
Implements methods for running Docker commands for building images or Cobalt.
"""

import json
import logging
import os
import subprocess
import time
import urllib.error
import urllib.request
import yaml

import utils

# Within the docker-compose.yml file, we use this service as a generic caller
# for any Docker image run that uses the Cobalt build scripts.
_DOCKER_COMPOSE_SERVICE_TARGET = 'kokoro-internal-cobalt-image'

# This build script is what needs to be invoked to run the full Cobalt build
# within the inner Docker container.
_INTERNAL_KOKORO_BUILD_SCRIPT = 'cobalt/devinfra/kokoro/bin/dind_build.sh'

_PLATFORM_TO_SERVICE_MAP = {
    'android-x86': 'linux',
    'android-arm': 'linux',
    'android-arm64': 'linux',
    'linux-x64x11-internal': 'linux',
    'evergreen-arm64-sbversion-18': 'linux',
    'evergreen-arm-hardfp-rdk-sbversion-18': 'rdk',
    'evergreen-arm-softfp-sbversion-18': 'linux',
    'evergreen-x64-sbversion-18': 'linux',
}

_REGISTRY_FAILURE_IMAGE = 'docker-build-failure'
_REGISTRY_FAILURE_TAG = 'latest'
_REGISTRY_METADATA_VAR = 'REGISTRY_METADATA'
_REGISTRY_HOSTNAME_VAR = 'REGISTRY_HOSTNAME'
_DEFAULT_REGISTRY_HOSTNAME = 'us-central1-docker.pkg.dev'
_METADATA_TOKEN_RETRIES = 3
_METADATA_TOKEN_RETRY_DELAY_SECONDS = 2

# Public constants for handling edge-cases with Clang Crosstool
LINUX_CLANG_CROSSTOOL = 'linux-x64x11-clang-crosstool'
CROSSTOOL_IMAGE_TAG = 'google3.433911828'


def get_failure_image():
  return f'{_REGISTRY_FAILURE_IMAGE}:{_REGISTRY_FAILURE_TAG}'


def refresh_registry_auth():
  """Refreshes Docker authentication with the configured Artifact Registry."""
  registry_metadata = os.environ.get(_REGISTRY_METADATA_VAR)
  if not registry_metadata:
    logging.debug('%s is not set; skipping registry auth refresh.',
                  _REGISTRY_METADATA_VAR)
    return False

  registry_hostname = os.environ.get(_REGISTRY_HOSTNAME_VAR,
                                     _DEFAULT_REGISTRY_HOSTNAME)
  token_url = (
      f'{registry_metadata.rstrip("/")}/instance/service-accounts/default/token'
  )

  access_token = None
  for attempt in range(1, _METADATA_TOKEN_RETRIES + 1):
    try:
      req = urllib.request.Request(
          token_url, headers={'Metadata-Flavor': 'Google'})
      with urllib.request.urlopen(req, timeout=10) as response:
        payload = json.loads(response.read().decode('UTF-8'))
        access_token = payload.get('access_token')
      if access_token:
        break
    except (urllib.error.URLError, ValueError, OSError) as e:
      logging.warning(
          'Failed to fetch registry token from metadata server '
          '(attempt %d/%d): %s', attempt, _METADATA_TOKEN_RETRIES, e)
    if attempt < _METADATA_TOKEN_RETRIES:
      time.sleep(_METADATA_TOKEN_RETRY_DELAY_SECONDS)

  if not access_token:
    logging.error('Unable to obtain registry access token from %s.', token_url)
    return False

  login_cmd = [
      'docker', 'login', '-u', 'oauth2accesstoken', '--password-stdin',
      f'https://{registry_hostname}',
  ]
  try:
    subprocess.run(
        login_cmd,
        input=access_token,
        encoding='UTF-8',
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT)
    logging.info('Refreshed Docker registry authentication for %s',
                 registry_hostname)
    return True
  except subprocess.CalledProcessError as e:
    logging.error('Failed to login to %s: %s', registry_hostname, e.output)
    return False


def pull_image(target_image):
  """
  Pull an image from the passed in registry, image name and tag
  """
  command = f'docker pull {target_image}'
  try:
    utils.exec_cmd(command)
  except subprocess.CalledProcessError:
    if not refresh_registry_auth():
      raise
    utils.exec_cmd(command)


def push_image(target_image):
  """
  Push an image to the passed in registry, image name and tag
  """
  refresh_registry_auth()
  command = f'docker push {target_image}'
  try:
    utils.exec_cmd(command)
  except subprocess.CalledProcessError:
    if not refresh_registry_auth():
      raise
    utils.exec_cmd(command)


def get_local_image_name(service, compose_file):
  """
  Parses the docker-compose file to determine the image-name from the
  service-name. When the compose-file is used to build the image, it will get
  the local image name applied.
  """
  with open(compose_file, 'r', encoding='UTF-8') as f:
    content = f.read()
    parsed_yaml = yaml.safe_load(content)
    return parsed_yaml['services'][service]['image']


def tag_image(src_img, dest_img):
  """
  Tags the src image to be the dest image name.
  """
  tag_command = f'docker tag {src_img} {dest_img}'
  utils.exec_cmd(tag_command)


def run_docker_build(platform, target_image, compose_file):
  """
  Runs the docker build for the provided platform, and tags the newly built
  image with the registry/image/tag arguments. If the build fails, instead it
  retags the failure image instead.
  """
  target_service = _PLATFORM_TO_SERVICE_MAP[platform]
  compose_build_command = (f'docker compose -f {compose_file} ' +
                           f'up --build --no-start {target_service}')
  utils.exec_cmd(compose_build_command)

  # Tag the image.
  local_image = get_local_image_name(target_service, compose_file)
  logging.info('Docker image built with name %s is re-tagged as %s',
               local_image, target_image)
  tag_image(local_image, target_image)


def run_cobalt_build(image_to_run, src_root):
  """
  Invokes the cobalt build using the input image and source root. The source
  root is the absolute path to the build script within the repo.
  """
  os.environ['FULL_IMAGE_TARGET'] = image_to_run
  service = _DOCKER_COMPOSE_SERVICE_TARGET
  script = os.path.join(src_root, _INTERNAL_KOKORO_BUILD_SCRIPT)
  compose_file = os.path.join(src_root,
                              'cobalt/devinfra/kokoro/docker-compose.yaml')
  compose_cmd = (f'docker compose -f {compose_file} ' +
                 f'run -T {service} /bin/bash -c {script}')
  utils.exec_cmd(compose_cmd)
