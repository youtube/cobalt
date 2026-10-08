#!/usr/bin/env vpython3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
"""Smoke test: Chromedriver/Selenium can attach to a running Cobalt."""

import subprocess
import unittest

from selenium import webdriver

import gtest_main

PORT = 9222


class ChromedriverSmokeTest(unittest.TestCase):

  def test_attach_and_navigate(self):
    cobalt = subprocess.Popen([
        gtest_main.flag('cobalt', 'out/linux-x64x11_devel/cobalt'),
        f'--remote-debugging-port={PORT}', '--no-sandbox'
    ])
    self.addCleanup(cobalt.kill)
    options = webdriver.ChromeOptions()
    options.debugger_address = f'127.0.0.1:{PORT}'
    driver = webdriver.Chrome(options=options)
    self.addCleanup(driver.quit)
    driver.get('https://www.youtube.com/tv')
    self.assertIn('youtube.com', driver.current_url)


if __name__ == '__main__':
  gtest_main.main()
