#!/usr/bin/env python3
#
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
"""Encapsulates all configuration parameters."""


class PackageConfig:
  DEFAULT_COBALT_PACKAGE_NAME = 'dev.cobalt.coat'
  DEFAULT_COBALT_ACTIVITY_NAME = 'dev.cobalt.app.MainActivity'

  def __init__(self,
               package_name: str = DEFAULT_COBALT_PACKAGE_NAME,
               activity_name: str = DEFAULT_COBALT_ACTIVITY_NAME):
    self.package_name = package_name
    self.activity_name = activity_name


class OutputConfig:
  """Holds the application's output configuration parameters."""
  DEFAULT_OUTPUT_DIRECTORY = 'cobalt_monitoring_data'
  DEFAULT_POLL_INTERVAL_MILLISECONDS = 100

  def __init__(
      self,
      output_format: str = 'both',
      output_directory: str = DEFAULT_OUTPUT_DIRECTORY,
      poll_interval_ms: int = DEFAULT_POLL_INTERVAL_MILLISECONDS,
  ):
    self.output_format = output_format
    self.output_directory = output_directory
    self.poll_interval_ms = poll_interval_ms


class AppConfig:
  """Holds the application's configuration parameters."""

  DEFAULT_COBALT_URL = 'https://youtube.com/tv/watch?v=1La4QzGeaaQ'
  EXPECTED_FLAGS_FORMAT = 'Expected format '\
    + '"key1=val1,key2=val2,...,key(n)=val(n)"'

  def __init__(self,
               package_config: PackageConfig = PackageConfig(),
               url: str = DEFAULT_COBALT_URL,
               cobalt_flags: str = '',
               output_config: OutputConfig = OutputConfig()):
    self.package_name = package_config.package_name
    self.activity_name = package_config.activity_name
    self.url = url
    self.output_format = output_config.output_format
    self.poll_interval_ms = output_config.poll_interval_ms
    self.output_directory = output_config.output_directory
    self.cobalt_flags = cobalt_flags

  def parse_cobalt_cli_flags(self) -> str:
    """Parses and formats Cobalt CLI and experiment flags."""
    flags = f'--remote-allow-origins=*,--url="{self.url}",'
    cobalt_flags = self.cobalt_flags.split(',')
    for flag_kv in cobalt_flags:
      if flag_kv:
        if 'url' in flag_kv:
          raise ValueError(
              'Overriding the --url flag inside of the cobalt flags is '\
              + 'disallowed in this script'
          )
        kv = flag_kv.split('=')
        if len(kv) != 2:
          raise ValueError(self.EXPECTED_FLAGS_FORMAT)
        flags += f'--{kv[0]}={kv[1]},'
    return flags.rstrip(',')  # Remove trailing comma
