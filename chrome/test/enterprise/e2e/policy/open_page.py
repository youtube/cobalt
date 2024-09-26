# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time
from absl import app, flags

from test_util import create_chrome_webdriver

FLAGS = flags.FLAGS

flags.DEFINE_string('url', None, 'The url to open in Chrome.')
flags.mark_flag_as_required('url')

flags.DEFINE_integer(
    'wait', 0,
    'How many seconds to wait between loading the page and printing the source.'
)

flags.DEFINE_bool('incognito', False,
                  'Set flag to open Chrome in incognito mode.')

flags.DEFINE_bool(
    'text_only', False,
    'Set flag to print only page text (defaults to full source).')


def main(argv):
  driver = create_chrome_webdriver(incognito=FLAGS.incognito)
  driver.get(FLAGS.url)

  if FLAGS.wait > 0:
    time.sleep(FLAGS.wait)

  if FLAGS.text_only:
    print(driver.find_element_by_css_selector('html').text.encode('utf-8'))
  else:
    print(driver.page_source.encode('utf-8'))

  driver.quit()


if __name__ == '__main__':
  app.run(main)
