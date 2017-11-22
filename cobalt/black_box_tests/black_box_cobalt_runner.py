"""The base class for black box tests."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import _env  # pylint: disable=unused-import
from cobalt.tools.automated_testing import cobalt_runner

# The following constants and logics are shared between this module and
# the JavaScript test environment. Anyone making changes here should also
# ensure necessary changes are made to testdata/black_box_js_test_utils.js
_TEST_STATUS_ELEMENT_NAME = 'black_box_test_status'
_HTML_TEST_SUCCESS_MESSAGE = 'HTMLTestsSucceeded'


class BlackBoxCobaltRunner(cobalt_runner.CobaltRunner):

  def HTMLTestsSucceeded(self):
    """Check test assertions in HTML page."""

    self.PollUntilFound('[' + _TEST_STATUS_ELEMENT_NAME + ']')
    body_element = self.UniqueFind('body')
    return body_element.get_attribute(
        _TEST_STATUS_ELEMENT_NAME) == _HTML_TEST_SUCCESS_MESSAGE
