# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pywinauto.application import Application
from test_util import create_chrome_webdriver

# Print 'home button exists' if the home button exists.

driver = create_chrome_webdriver()

try:
  app = Application(backend="uia")
  app.connect(title_re='.*Chrome|.*Chromium')

  home_button = app.top_window().child_window(
      title="Home", control_type="Button")
  if home_button.exists(timeout=1):
    print('home button exists')

finally:
  driver.quit()
