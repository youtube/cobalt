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

  def _parse_flag_value(self, value_str):
    """
    Parses a single flag value string, attempting to convert it to an int,
    a list of strings/ints, or keeping it as a string.
    The final return value will always be a string representation.
    """
    # Try converting to an integer first
    try:
      # If it's an integer, convert it to a string
      return str(int(value_str))
    except ValueError:
      pass

    # If it's not an integer, check if it's a comma-separated list
    if ',' in value_str:
      # Split by comma and try to parse each sub-value
      parts = [part.strip() for part in value_str.split(',')]
      parsed_parts = []
      for part in parts:
        try:
          parsed_parts.append(int(part))
        except ValueError:
          parsed_parts.append(part)
      # Convert the list to its string representation
      return str(parsed_parts)

    # If it's not an int or a comma-separated list, return as a string directly
    return value_str

  def parse_cobalt_cli_flags(self):
    """
    Takes a comma-separated string of flags (key=value or standalone key)
    and formats it into a single string where each key-value pair is
    formatted as --key=value and separated by a comma.

    Supports "key=value" pairs and "key" standalone flags.
    Expected input format: "key1=val1,key2=val2,etc." or "key1,key2=val2".
    Output format: "--key1=val1,--key2=val2,etc."
    Standalone keys will have a value of "True" in the output.
    """
    formatted_flags_list = []
    if not self.cobalt_flags:
      return ''

    # Split the main string by comma to get
    # individual key=value pairs or standalone keys
    pairs = [pair.strip() for pair in self.cobalt_flags.split(',')]

    for pair in pairs:
      parsed_value = None  # Assign "True" as the value for standalone flags
      # Check if it's a key=value pair or a standalone key
      if '=' in pair:
        first_equals_index = pair.find('=')
        key = pair[:first_equals_index].strip()
        value_str = pair[first_equals_index + 1:].strip()
        if not key:
          print(f'Warning: Skipping malformed flag \'{pair}\' with empty key.')
          continue
        parsed_value = self._parse_flag_value(value_str)
      else:
        # It's a standalone key
        key = pair.strip()
        if not key:
          print('Warning: Skipping empty standalone flag.')
          continue

      if parsed_value is None:
        formatted_flags_list.append(f'--{key}')
      else:
        # Format as --key=value
        formatted_flags_list.append(f'--{key}={parsed_value}')

    # Join all formatted key-value pairs with a comma
    joined_flag_list = ','.join(formatted_flags_list)
    joined_flag_list = '"' + joined_flag_list + '"'
    return joined_flag_list
