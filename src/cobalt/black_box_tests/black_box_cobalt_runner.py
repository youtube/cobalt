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
_JS_TEST_SUCCESS_MESSAGE = 'JavaScript_test_succeeded'
_JS_TEST_SETUP_DONE_MESSAGE = 'JavaScript_setup_done'


class BlackBoxCobaltRunner(cobalt_runner.CobaltRunner):
  """Custom CobaltRunner made for BlackBoxTests' need."""

  def JSTestsSucceeded(self):
    """Check test assertions in HTML page."""

    # Call onTestEnd() in black_box_js_test_utils.js to unblock the waiting for
    # JavaScript test logic completion.
    self.PollUntilFound('[' + _TEST_STATUS_ELEMENT_NAME + ']')
    body_element = self.UniqueFind('body')
    return body_element.get_attribute(
        _TEST_STATUS_ELEMENT_NAME) == _JS_TEST_SUCCESS_MESSAGE

  def WaitForJSTestsSetup(self):
    """Poll setup status until JavaScript gives green light."""

    # Calling setupFinished() in black_box_js_test_utils.js to unblock the
    # waiting logic here.
    self.PollUntilFound('#{}'.format(_JS_TEST_SETUP_DONE_MESSAGE))
