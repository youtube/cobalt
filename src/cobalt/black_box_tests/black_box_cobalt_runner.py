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

  def __init__(self,
               launcher_params,
               url,
               log_file=None,
               target_params=None,
               success_message=None):
    # For black box tests, don't log inline script warnings, we intend to
    # explicitly control timings for suspends and resumes, so we are not
    # concerned about a "suspend at the wrong time".
    if target_params is None:
      target_params = []
    target_params.append('--silence_inline_script_warnings')

    super(BlackBoxCobaltRunner, self).__init__(launcher_params, url, log_file,
                                               target_params, success_message)

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
