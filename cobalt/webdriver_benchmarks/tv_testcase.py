"""Base class for WebDriver tests."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import logging
import os
import sys
import time

# This directory is a package
sys.path.insert(0, os.path.abspath('.'))
# pylint: disable=C6204,C6203
import tv
import tv_testcase_util

import default_query_param_constants
try:
  import custom_query_param_constants as query_param_constants
except ImportError:
  query_param_constants = default_query_param_constants

import _env  # pylint: disable=unused-import
from cobalt.tools.automated_testing import cobalt_test
from cobalt.tools.automated_testing import webdriver_utils
from starboard.tools import command_line

keys = webdriver_utils.import_selenium_module('webdriver.common.keys')

AD_TIMEOUT_SECONDS = 60
TITLE_CARD_HIDDEN_TIMEOUT_SECONDS = 30

MAX_SURVEY_SHELF_COUNT = 3

_is_initialized = False

_default_url = 'https://www.youtube.com/tv'
_sample_size = tv_testcase_util.SAMPLE_SIZE_STANDARD
_disable_videos = False


class TvTestCase(cobalt_test.CobaltTestCase):
  """Base class for WebDriver tests.

  Style note: snake_case is used for function names here so as to match
  with an internal class with the same name.
  """

  class TitleCardHiddenTimeoutException(Exception):
    """Exception thrown when title card did not disappear in time."""

  class SurveyShelfLimitExceededException(Exception):
    """Exception thrown when too many surveys are encountered."""

  def setUp(self):
    self.survey_shelf_count = 0
    global _is_initialized
    if not _is_initialized:
      # Initialize the tests.
      constants = query_param_constants
      if not self.get_default_url().startswith(constants.INIT_QUERY_PARAMS_URL):
        constants = default_query_param_constants
      query_params = constants.INIT_QUERY_PARAMS
      triggers_reload = constants.INIT_QUERY_PARAMS_TRIGGER_RELOAD
      self.load_tv(query_params=query_params, triggers_reload=triggers_reload)
      _is_initialized = True

  def get_default_url(self):
    return _default_url

  def get_sample_size(self):
    return _sample_size

  def are_videos_disabled(self):
    return _disable_videos

  def load_tv(self, query_params=None, triggers_reload=False):
    """Loads the main TV page and waits for it to display.

    Args:
      query_params: A dict containing additional query parameters.
      triggers_reload: Whether or not the navigation will trigger a reload.
    Raises:
      Underlying WebDriver exceptions
    """
    load_url = True
    while load_url:
      self.clear_url_loaded_events()
      self.get_webdriver().get(
          tv_testcase_util.generate_url(self.get_default_url(), query_params))
      self.wait_for_url_loaded_events()
      # The reload will only trigger on the first URL load.
      if triggers_reload:
        self.clear_url_loaded_events()
        self.wait_for_url_loaded_events()
        triggers_reload = False
      self.poll_until_found(tv.SHELF)
      self.wait_for_processing_complete()
      # Stop loading the url if there is no survey shelf.
      if not self.check_for_survey_shelf():
        load_url = False
    self.assert_displayed(tv.FOCUSED_SHELF_TITLE)

  def wait_for_usable_after_launch(self):
    self.poll_until_found(tv.SHELF)
    self.wait_for_processing_complete()
    self.wait_for_html_script_element_execute_count(2)

  def wait_for_processing_complete_after_focused_shelf(self):
    """Waits for Cobalt to focus on a shelf and complete pending layouts."""
    self.poll_until_found(tv.FOCUSED_SHELF)
    self.assert_displayed(tv.FOCUSED_SHELF_TITLE)
    self.wait_for_processing_complete()

  def maybe_wait_for_advertisement(self):
    """Waits for an advertisement if it is encountered.

    Returns:
      True if an advertisement was encountered
    """
    start_time = time.time()
    advertisement_encountered = self.find_elements(tv.AD_SHOWING)
    while self.find_elements(tv.AD_SHOWING):
      if self.find_elements(tv.SKIP_AD_BUTTON_CAN_SKIP):
        self.send_keys(keys.Keys.ENTER)
        self.wait_for_processing_complete(False)
        break
      if time.time() - start_time > AD_TIMEOUT_SECONDS:
        break
      time.sleep(0.1)

    return advertisement_encountered

  def wait_for_title_card_hidden(self):
    """Waits for the title to disappear while a video is playing.

    Raises:
      TitleCardHiddenTimeoutException: The title card did not become hidden in
      the required time.
    """
    start_time = time.time()
    while not self.find_elements(tv.TITLE_CARD_HIDDEN):
      if time.time() - start_time > TITLE_CARD_HIDDEN_TIMEOUT_SECONDS:
        raise TvTestCase.TitleCardHiddenTimeoutException()
      time.sleep(1)

  def check_for_survey_shelf(self):
    """Check for a survey shelf in the DOM.

    Raises:
      SurveyShelfLimitExceededException: The limit for the number of surveys
      during a test was exceeded.
    Returns:
      Whether or not a survey was found in the DOM.
    """
    if not self.find_elements(tv.SURVEY_SHELF):
      return False
    self.survey_shelf_count += 1
    print('Encountered survey shelf! {} total.'.format(self.survey_shelf_count))
    if self.survey_shelf_count > MAX_SURVEY_SHELF_COUNT:
      raise TvTestCase.SurveyShelfLimitExceededException()
    return True


def main():
  global _default_url
  global _sample_size
  global _disable_videos

  logging.basicConfig(level=logging.DEBUG)

  arg_parser = command_line.CreateParser()
  arg_parser.add_argument(
      '--url', help='Specifies the URL to run the tests against.')
  arg_parser.add_argument(
      '--sample_size',
      choices=[
          tv_testcase_util.SAMPLE_SIZE_STANDARD,
          tv_testcase_util.SAMPLE_SIZE_REDUCED,
          tv_testcase_util.SAMPLE_SIZE_MINIMAL
      ],
      default=tv_testcase_util.SAMPLE_SIZE_STANDARD,
      help='The sample size to use with the tests.')
  arg_parser.add_argument(
      '--disable_videos',
      action='store_true',
      help='Disables video testing during the tests.')

  args, _ = arg_parser.parse_known_args()

  if args.url is not None:
    _default_url = args.url
  url = tv_testcase_util.generate_url(_default_url,
                                      query_param_constants.BASE_QUERY_PARAMS)
  _sample_size = args.sample_size
  _disable_videos = args.disable_videos

  cobalt_test.run(url=url)
