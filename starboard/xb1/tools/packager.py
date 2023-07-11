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
"""Generate a xb1 package.

  Generates a xb1 package from the loose files.
  Called by package_application.py
"""

import argparse
import logging
import os
import platform
import shutil
import subprocess
import sys
from xml.etree import ElementTree as ET
import zipfile

from starboard.tools import package

_INTERNAL_CERT_PATH = os.path.join(
    os.path.dirname(os.path.realpath(__file__)), os.pardir, os.pardir,
    os.pardir, 'internal', 'starboard', 'xb1', 'cert', 'youtube.pfx')
_EXTERNAL_CERT_PATH = os.path.join(
    os.path.dirname(os.path.realpath(__file__)), os.pardir, os.pardir,
    os.pardir, 'starboard', 'xb1', 'cert', 'cobalt.pfx')
_APPX_MANIFEST_XML_NAMESPACE = \
    'http://schemas.microsoft.com/appx/manifest/foundation/windows10'
_NAMESPACE_DICT = {'appx': _APPX_MANIFEST_XML_NAMESPACE}
_PUBLISHER_XPATH = './appx:Identity[@Publisher]'
_PRODUCT_APPX_NAME = {
    'cobalt': 'cobalt',
    'youtube': 'cobalt',
    'mainappbeta': 'mainappbeta',
    'youtubetv': 'youtubetv'
}
_PRODUCT_CERT_PATH = {
    'cobalt': _EXTERNAL_CERT_PATH,
    'youtube': _INTERNAL_CERT_PATH,
    'mainappbeta': _INTERNAL_CERT_PATH,
    'youtubetv': _INTERNAL_CERT_PATH,
}
_DEFAULT_SDK_BIN_DIR = 'C:\\Program Files (x86)\\Windows Kits\\10\\bin'
_DEFAULT_WIN_SDK_VERSION = '10.0.22000.0'
_SOURCE_SPLASH_SCREEN_SUB_PATH = os.path.join('internal', 'cobalt', 'browser',
                                              'splash_screen')
# The splash screen file referenced in starboard/xb1/shared/configuration.cc
# must be in appx/content/data/web/ to be found at runtime.
_DESTINATION__SPLASH_SCREEN_SUB_PATH = os.path.join('appx', 'content', 'data',
                                                    'web', 'splash_screen')
_SPLASH_SCREEN_FILE = {
    'cobalt': '',
    'youtube': 'youtube_splash_screen.html',
    'youtubetv': 'ytv_splash_screen.html',
    'mainappbeta': 'youtube_splash_screen.html',
}


def _SelectBestPath(os_var_name, path):
  if os_var_name in os.environ:
    return os.environ[os_var_name]
  if os.path.exists(path):
    return path
  new_path = path.replace('Program Files (x86)', 'mappedProgramFiles')
  if os.path.exists(new_path):
    return new_path
  return path


def _GetSourceSplashScreenDir():
  # Relative to this file, the path is
  # "../../../internal/cobalt/browser/splash_screen".
  src_dir = os.path.join(
      os.path.dirname(__file__), os.pardir, os.pardir, os.pardir)
  return os.path.join(src_dir, _SOURCE_SPLASH_SCREEN_SUB_PATH)


class Package(package.PackageBase):
  """A class representing an installable UWP Appx package."""

  @classmethod
  def AddArguments(cls, arg_parser):
    """Add xb1-specific command-line arguments to the ArgumentParser."""
    super(Package, cls).AddArguments(arg_parser)
    arg_parser.add_argument(
        '-p',
        '--publisher',
        dest='publisher',
        default=None,
        help='Publisher Identity in Package.')

  @classmethod
  def ExtractArguments(cls, options):
    kwargs = super(Package, cls).ExtractArguments(options)
    kwargs['publisher'] = options.publisher
    kwargs['product'] = options.product
    return kwargs

  def _GetDestinationSplashScreenDir(self):
    return os.path.join(self.source_dir, _DESTINATION__SPLASH_SCREEN_SUB_PATH)

  def _CleanSplashScreenDir(self):
    splash_screen_dir = self._GetDestinationSplashScreenDir()

    if not os.path.exists(splash_screen_dir):
      return

    shutil.rmtree(splash_screen_dir)

  def _CopySplashScreen(self):
    splash_screen_dir = self._GetDestinationSplashScreenDir()
    # Create the splash screen directory if necessary.
    if not os.path.exists(splash_screen_dir):
      try:
        os.makedirs(splash_screen_dir)
      except OSError as e:
        raise RuntimeError(f'Failed to create {splash_screen_dir}: {e}') from e
    # Copy the correct splash screen for the current product into content.
    splash_screen_file = _SPLASH_SCREEN_FILE[self.product]
    src_splash_screen_dir = _GetSourceSplashScreenDir()
    src_splash_screen_file = os.path.join(src_splash_screen_dir,
                                          splash_screen_file)
    if not os.path.exists(src_splash_screen_file):
      logging.error('Failed to find splash screen file in source : %s',
                    src_splash_screen_file)
      return
    shutil.copy(src_splash_screen_file, splash_screen_dir)

  @classmethod
  def SupportedPlatforms(cls):
    if platform.system() == 'Windows':
      return ['xb1']
    else:
      return []

  def __init__(self, publisher, product, **kwargs):
    windows_sdk_bin_dir = _SelectBestPath('WindowsSdkBinPath',
                                          _DEFAULT_SDK_BIN_DIR)
    self.windows_sdk_host_tools = os.path.join(windows_sdk_bin_dir,
                                               _DEFAULT_WIN_SDK_VERSION, 'x64')
    self.publisher = publisher
    self.product = product
    super().__init__(**kwargs)

    if not os.path.exists(self.source_dir):
      raise RuntimeError(
          f'Package source directory {self.source_dir} not found.')

    logging.debug('Building package')
    if self.publisher:
      self._UpdateAppxManifestPublisher(publisher)

    # Remove any previous splash screen from content.
    self._CleanSplashScreenDir()
    # Copy the correct splash screen into content.
    self._CopySplashScreen()

    self.package = self._BuildPackage()
    if not os.path.exists(self.package):
      raise RuntimeError(f'Package file {self.package} does not exist.')

    appxsym_out_file = self.appxsym_location
    with zipfile.ZipFile(appxsym_out_file, 'w', zipfile.ZIP_DEFLATED) as z:
      if os.path.isfile(os.path.join(self.source_dir, 'cobalt.exe.pdb')):
        z.write(os.path.join(self.source_dir, 'cobalt.exe.pdb'))

    with zipfile.ZipFile(self.appxupload_location, 'w',
                         zipfile.ZIP_DEFLATED) as z:
      z.write(appxsym_out_file)
      z.write(self.appx_location)

    logging.info('Finished.')

  def _UpdateAppxManifestPublisher(self, publisher):
    logging.info('Updating Appx with publisher [%s]', publisher)
    tree = ET.parse(self.appxmanifest_location)
    for element in tree.findall(_PUBLISHER_XPATH, _NAMESPACE_DICT):
      assert 'Publisher' in element.attrib
      element.attrib['Publisher'] = publisher
    tree.write(self.appxmanifest_location)

  def Install(self, targets=None):
    # TODO: implement
    pass

  @property
  def appx_folder_location(self):
    product_locations = {
        'cobalt': 'appx',
        'youtube': 'appx',
        'mainappbeta': 'mainappbeta-appx',
        'youtubetv': 'youtubetv-appx'
    }
    return os.path.join(self.source_dir, product_locations[self.product])

  @property
  def appx_location(self):
    return os.path.join(self.output_dir,
                        _PRODUCT_APPX_NAME[self.product] + '.appx')

  @property
  def appxsym_location(self):
    return os.path.join(self.output_dir,
                        _PRODUCT_APPX_NAME[self.product] + '.appxsym')

  @property
  def appxupload_location(self):
    return os.path.join(self.output_dir,
                        _PRODUCT_APPX_NAME[self.product] + '.appxupload')

  @property
  def appxmanifest_location(self):
    return os.path.join(self.appx_folder_location, 'AppxManifest.xml')

  def _BuildPackage(self):
    """Build the package for XB1.

    Raises:
      RuntimeError: if packaging or verification (if requested) fails.
    Returns:
      path to package location.
    """

    logging.info('Creating the package at: %s', self.appx_location)

    tools_path = self.windows_sdk_host_tools

    makeappx_args = [
        os.path.join(tools_path, 'makeappx.exe'), 'pack', '/l', '/o', '/d',
        self.appx_folder_location, '/p', self.appx_location
    ]
    logging.info('Running %s', ' '.join(makeappx_args))
    subprocess.check_call(makeappx_args)

    cert_path = _PRODUCT_CERT_PATH[self.product]

    try:
      signtool_args = [
          os.path.join(tools_path, 'signtool.exe'), 'sign', '/fd', 'SHA256',
          '/a', '/f', cert_path, '/v', '/debug', self.appx_location
      ]
      logging.info('Running %s', ' '.join(signtool_args))
      subprocess.check_call(signtool_args)
    except subprocess.CalledProcessError as sub_err:
      if '0x80070070' in str(sub_err):
        print('\n\n*** ERROR CAUSED BY LOW DISK SPACE!! ***\n\n')
      raise  # Rethrow original error with original stack trace.

    return self.appx_location


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-s',
      '--source',
      required=True,
      help='Source directory from which to create a package.')
  parser.add_argument(
      '-o',
      '--output',
      default=os.path.join(
          os.path.dirname(os.path.realpath(__file__)), 'package'),
      help='Output directory to place the packaged app. Defaults to ./package/')
  parser.add_argument(
      '-p',
      '--product',
      default='cobalt',
      help=(
          'Product name. This must be one of [cobalt, youtube, youtubetv,'
          'mainappbeta]. Any builds that are not internal to YouTube should use'
          'cobalt.'))
  args, _ = parser.parse_known_args()

  if not os.path.exists(args.output):
    os.makedirs(args.output)

  Package(
      publisher=None,
      product=args.product,
      source_dir=args.source,
      output_dir=args.output)


if __name__ == '__main__':
  sys.exit(main())
