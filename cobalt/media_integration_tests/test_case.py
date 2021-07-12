# Copyright 2021 The Cobalt Authors. All Rights Reserved.
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
"""The module of base media integration test case."""

import logging
import unittest

import _env  # pylint: disable=unused-import
from cobalt.media_integration_tests.test_app import TestApp

# Global variables
_launcher_params = None
_supported_features = None


def SetLauncherParams(launcher_params):
  global _launcher_params
  _launcher_params = launcher_params


def SetSupportedFeatures(supported_features):
  global _supported_features
  _supported_features = supported_features


class TestCase(unittest.TestCase):
  """The base class for media integration test cases."""

  def __init__(self, *args, **kwargs):
    super(TestCase, self).__init__(*args, **kwargs)
    self.launcher_params = _launcher_params
    self.supported_features = _supported_features

  def setUp(self):
    logging.info('Running %s', str(self.id()))

  def tearDown(self):
    logging.info('Done %s', str(self.id()))

  def CreateCobaltApp(self, url):
    app = TestApp(
        launcher_params=self.launcher_params,
        supported_features=self.supported_features,
        url=url)
    return app

  @staticmethod
  def IsFeatureSupported(feature):
    global _supported_features
    return _supported_features[feature]

  @staticmethod
  def CreateTest(test_class, test_name, test_function, *args):
    test_method = lambda self: test_function(self, *args)
    test_method.__name__ = 'test_%s' % test_name
    setattr(test_class, test_method.__name__, test_method)
