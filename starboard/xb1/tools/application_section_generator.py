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
"""Generated the application section for xb1."""

from __future__ import print_function

import argparse
import os
import sys

try:
  from internal.starboard.xb1 import appx_product_settings
  PRODUCT_SETTINGS = appx_product_settings.PRODUCT_SETTINGS
except ImportError:
  from starboard.xb1 import appx_product_settings
  PRODUCT_SETTINGS = appx_product_settings.PRODUCT_SETTINGS

THIRD_PARTY_PATH = os.path.normpath(
    os.path.join(
        os.path.realpath(__file__), os.pardir, os.pardir, os.pardir, os.pardir,
        'third_party'))
sys.path.append(THIRD_PARTY_PATH)
from jinja2 import Template  # pylint: disable=wrong-import-position

#pylint: disable=anomalous-backslash-in-string


def _GenerateApplicationSection(application_name, application_template_filename,
                                config, product):
  """Uses templates to generate an application section for 1 application in the
  appx xml file.

  Example input: 'cobalt', 'application.template', 'extension.template'
  Example output:
     <Application Id="cobalt"
                  Executable="cobalt.exe"
                  EntryPoint="foo.bar">
       <uap:VisualElements
         DisplayName="cobalt"
         Description="cobalt"
         Square150x150Logo="Assets\Square150x150Logo.scale-100.png"
         Square44x44Logo="Assets\Square44x44Logo.scale-100.png"
         BackgroundColor="transparent"/>
       <Extensions>
         <uap:Extension Category="windows.dialProtocol">
           <uap:DialProtocol Name="cobalt.foo"/>
         </uap:Extension>
         <uap:Extension Category="windows.protocol">
           <uap:Protocol Name="cobalt-starboard"/>
         </uap:Extension>
       </Extensions>
     </Application>
  """
  if product not in PRODUCT_SETTINGS:
    raise ValueError(
        f'Unknown product value: {product}. '
        'Product settings must be defined in appx_product_settings.py')

  with open(application_template_filename, 'r', encoding='utf-8') as fileobj:
    application_template = Template(
        fileobj.read(), trim_blocks=True, lstrip_blocks=True)

  return application_template.render(
      __APPLICATION_NAME__=application_name,
      __CONFIG__=config,
      __IS_COBALT__=application_name == 'cobalt',
      __PRODUCT_SETTINGS__=PRODUCT_SETTINGS[product])


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--application_name',
      dest='application_name',
      required=True,
      help='application name',
      type=str)
  parser.add_argument(
      '--application_template',
      dest='application_template',
      required=True,
      help='path to the application section template',
      type=str)
  parser.add_argument(
      '--product',
      dest='product',
      required=False,
      default='cobalt',
      help='Name of product being built (e.g. cobalt/youtube/youtubetv)',
      type=str)
  parser.add_argument(
      '--output',
      dest='output',
      required=True,
      help='output filename',
      type=str)
  parser.add_argument(
      '--config',
      dest='config',
      required=True,
      help='config (e.g. debug/devel/qa/gold)',
      type=str)
  if len(sys.argv) == 1:
    parser.print_help()
    sys.exit(1)
  options = parser.parse_args(sys.argv[1:])
  with open(options.output, 'w', encoding='utf-8') as outfile:
    outfile.write(
        _GenerateApplicationSection(options.application_name,
                                    options.application_template,
                                    options.config, options.product))


if __name__ == '__main__':
  main()
