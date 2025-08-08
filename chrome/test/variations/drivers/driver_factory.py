# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import subprocess

from contextlib import contextmanager
from pkg_resources import packaging
from typing import Optional

import attr

from selenium import webdriver

@attr.attrs()
class DriverFactory:
  """The factory to create webdriver for the pre-defined environment"""

  chromedriver_path: str = attr.attrib()

  @functools.cached_property
  def driver_version(self) -> packaging.version.Version:
    # Sample version output: ChromeDriver 117.0.5938.0 (branch_name)
    version_str = str(subprocess.check_output([self.chromedriver_path, '-v']))
    return packaging.version.parse(version_str.split(' ')[1])

  @property
  def supports_startup_timeout(self) -> bool:
    """Whether the driver supports browserStartupTimeout option."""
    return self.driver_version >= packaging.version.parse('117.0.5913.0')

  @property
  def default_options(self) -> webdriver.ChromeOptions:
    """The default ChromeOptions that can be used for all platforms."""
    options = webdriver.ChromeOptions()
    options.add_argument('disable-field-trial-config')

    # browserStartupTimeout is added at 117.0.5913.0
    if self.supports_startup_timeout:
      # The default timeout is 60 seconds, in case of crashes, this increases
      # the test time. Chrome should start in 10 seconds.
      options.add_experimental_option('browserStartupTimeout', 10000)
    return options

  @contextmanager
  def create_driver(self,
                    seed_file: Optional[str] = None,
                    options: Optional[webdriver.ChromeOptions] = None
    ) -> webdriver.Remote:
    """Creates a webdriver.

    Args:
      seed_file: the path to the seed file for Chrome to launch with.
      options: the ChromeOptions to launch the Chrome that applies to all
               platforms. If None, the default_options is used.

    Returns:
      An instance of webdriver.Remote
    """
    raise NotImplemented

  def close(self):
    """Cleans up anything that is created during the session."""
    pass
