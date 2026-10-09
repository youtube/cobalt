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
"""Host smoke test driving Cobalt through ChromeDriver."""

import sys

import pytest
from selenium import webdriver
from selenium.webdriver.chrome.service import Service


def test_chromedriver_smoke(launch_cobalt, pytestconfig):
  launch_cobalt()
  options = webdriver.ChromeOptions()
  # Attach to the running Cobalt (default DevTools port) instead of launching.
  options.add_experimental_option('debuggerAddress', '127.0.0.1:9222')
  service = Service(pytestconfig.getoption('chromedriver'))
  with webdriver.Chrome(service=service, options=options) as driver:
    assert driver.execute_script('return 1 + 1') == 2
    assert driver.get_screenshot_as_png().startswith(b'\x89PNG')


if __name__ == '__main__':
  sys.exit(pytest.main([__file__, '-v', *sys.argv[1:]]))
