# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
#
"""Deploy script for UWP. Generates AppxManifest.xml"""

from __future__ import print_function

import argparse
import datetime
import glob
import os
import sys
import time
import msvcrt

try:
  from internal.starboard.xb1 import appx_product_settings
  PRODUCT_SETTINGS = appx_product_settings.PRODUCT_SETTINGS
except ImportError:
  from starboard.xb1 import appx_product_settings
  PRODUCT_SETTINGS = appx_product_settings.PRODUCT_SETTINGS

# Sometimes we must up the appx version number even if 24 hours have not
# elapsed.
# This number must not exceed floor((999 - 537) / 10) = 46, or the resulting
# version would exceed the XML schema limit of 999 for that version field.
_MANUAL_APPX_VERSION_OFFSET = 0

THIRD_PARTY_PATH = os.path.normpath(
    os.path.join(
        os.path.realpath(__file__), os.pardir, os.pardir, os.pardir, os.pardir,
        'third_party'))
sys.path.append(THIRD_PARTY_PATH)

from jinja2 import Template  # pylint: disable=wrong-import-position

_APPLICATION_SECTION_REGEX = 'ApplicationSection.*.xml'


def _GenerateAppxManifestContent(appx_template, template_directory, config,
                                 product):
  """ Generates content of the AppxManifest.

  This function finds all files that match |_APPLICATION_SECTION_REGEX| in
  |template_directory|, and uses them to create the body that will replace
  __APPLICATIONS__.  It then generates the content for AppxManifest by
  parsing |appx_template|.

  Args:
    appx_template: Path to AppxManifest.xml.template
    template_directory: Path to directory containing files with Application
      section of AppxManifest
    config: Current config being built.  E.g. gold, devel, or qa  Reads
      AppxManifest.xml.template from |appx_template|, and then in the generates
      the new content by replacing __APPLICATIONS__.
  """
  if product not in PRODUCT_SETTINGS:
    raise ValueError(
        f'Unknown product value: {product}. '
        'Product settings must be defined in appx_product_settings.py')
  application_sections = []
  search_path = os.path.join(template_directory, _APPLICATION_SECTION_REGEX)
  for filename in glob.glob(search_path):
    with open(filename, encoding='utf-8') as f:
      application_sections.append(f.read())
  application_section = '\n'.join(application_sections)

  today = datetime.datetime.utcnow().isocalendar()
  # The appx version number is formatted as a dotted-quad like
  # "a.b.c.d" where a!=0, d==0, and the other elements must be 0-999
  # In order to generate a distinct one every day, we set 'b' to
  # the year since 2017 and 'c' to ((week_number*10) + (day_of_week) +
  # config_build_offset). The reference for Identity can be found here:
  # https://docs.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-identity
  # Note that MS Partner Center mandates the revision number must be 0.
  config_build_offsets = {'debug': 0, 'devel': 1, 'qa': 2, 'gold': 3}
  if config not in config_build_offsets:
    raise ValueError(f'Unknown config value: {config}')
  config_build_offset = config_build_offsets[config]
  build_number = ((today[1] * 10) + today[2] +
                  (_MANUAL_APPX_VERSION_OFFSET * 10) + config_build_offset)
  major_version = today[0] - 2017
  appx_version = f'1.{major_version}.{build_number}.0'

  with open(appx_template, encoding='utf-8') as source:
    manifest_template = Template(
        source.read(), trim_blocks=True, lstrip_blocks=True)
    return manifest_template.render(
        __APPX_VERSION__=appx_version,
        __CONFIG__=config,
        __APPLICATIONS__=application_section,
        __PRODUCT_SETTINGS__=PRODUCT_SETTINGS[product])


def _WriteAppxManifest(out_filename, appx_template, template_directory, config,
                       product):
  """ Generate AppxManifest and write the contents to |out_filename|.

  Args:
    out_filename: Name of the destination AppxManifest file.
    appx_template: Path to AppxManifest.xml.template
    template_directory: Path to directory containing files with Application
      section of AppxManifest
    config: Current |config| being built, e.g. gold, debug, etc
  """
  num_bytes_to_lock = 1

  with open(out_filename, 'w', encoding='utf-8') as dest:
    # Sometimes, we're building tens of targets, and the retry logic
    # in msvcrt.locking is not enough.  Thus, additional waits
    # are added, with backoffs to help each task finish.

    # Wait longer between retries.  These times are in seconds.
    for retry_time in [3, 5, 10, 20]:
      try:
        # According to the documentation, this will retry the lock up to 10
        # times, and wait 1 second between each retry.  If this fails, an
        # IOError exception is raised.  Source:
        # https://docs.python.org/2/library/msvcrt.html
        msvcrt.locking(dest.fileno(), msvcrt.LK_LOCK, num_bytes_to_lock)
        break
      except IOError:
        print(
            f'Unable to lock {out_filename}. Retrying in {retry_time} seconds.')
        time.sleep(retry_time)
    else:
      # Try one last time, this time without a try/catch block,
      # so that the exception can propagate.
      msvcrt.locking(dest.fileno(), msvcrt.LK_LOCK, num_bytes_to_lock)

    try:
      # Note that both the manifest generation, and writing
      # the content are done under a lock.
      manifest_content = _GenerateAppxManifestContent(appx_template,
                                                      template_directory,
                                                      config, product)
      dest.write(manifest_content)
    finally:
      # Writing to the appx manifest moves the current file position.
      # Therefore, seek to position 0, as that is the locked position.
      dest.seek(0)
      msvcrt.locking(dest.fileno(), msvcrt.LK_UNLCK, num_bytes_to_lock)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--manifest_out',
      dest='manifest_out',
      required=True,
      help='output manifest filename',
      type=str)
  parser.add_argument(
      '--config',
      dest='config',
      required=True,
      help='config (e.g. debug/devel/qa/gold)',
      type=str)
  parser.add_argument(
      '--appx_template',
      dest='appx_template',
      required=True,
      help='Path to AppxManifest.xml.template',
      type=str)
  parser.add_argument(
      '--template_directory',
      dest='template_directory',
      required=True,
      help='Path to directory containing files with Application section of '
      'AppxManifest',
      type=str)
  parser.add_argument(
      '--product',
      dest='product',
      required=False,
      default='cobalt',
      help='Name of product being built (e.g. cobalt/youtube/youtubetv)',
      type=str)
  if len(sys.argv) == 1:
    parser.print_help()
    sys.exit(1)
  options = parser.parse_args(sys.argv[1:])
  _WriteAppxManifest(options.manifest_out, options.appx_template,
                     options.template_directory, options.config,
                     options.product)


if __name__ == '__main__':
  main()
