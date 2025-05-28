# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pywinauto.application import Application
from pywinauto.findwindows import ElementNotFoundError
from selenium import webdriver

from test_util import create_chrome_webdriver

options = webdriver.ChromeOptions()
options.add_argument("--force-renderer-accessibility")

driver = create_chrome_webdriver(chrome_options=options)

try:
  app = Application(backend="uia")
  app.connect(title_re='.*Chrome|.*Chromium')
  app.top_window().child_window(title="Bookmarks", control_type="ToolBar") \
      .print_control_identifiers()
  print("Bookmarkbar is found")
except ElementNotFoundError as error:
  print(error)
  print("Bookmarkbar is missing")
finally:
  driver.quit()
