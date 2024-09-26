#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import mock
import requests
import unittest

import result_sink_util


SINK_ADDRESS = 'sink/address'
SINK_POST_URL = 'http://%s/prpc/luci.resultsink.v1.Sink/ReportTestResults' % SINK_ADDRESS
AUTH_TOKEN = 'some_sink_token'
LUCI_CONTEXT_FILE_DATA = """
{
  "result_sink": {
    "address": "%s",
    "auth_token": "%s"
  }
}
""" % (SINK_ADDRESS, AUTH_TOKEN)
HEADERS = {
    'Content-Type': 'application/json',
    'Accept': 'application/json',
    'Authorization': 'ResultSink %s' % AUTH_TOKEN
}
CRASH_TEST_LOG = """
Exception Reason:
App crashed and disconnected.

Recovery Suggestion:
"""


class UnitTest(unittest.TestCase):

  def test_compose_test_result(self):
    """Tests compose_test_result function."""
    # Test a test result without log_path.
    test_result = result_sink_util._compose_test_result(
        'TestCase/testSomething', 'PASS', True)
    expected = {
        'testId': 'TestCase/testSomething',
        'status': 'PASS',
        'expected': True,
        'tags': [],
        'testMetadata': {
            'name': 'TestCase/testSomething',
            'location': None,
        },
    }
    self.assertEqual(test_result, expected)
    short_log = 'Some logs.'
    # Tests a test result with log_path.
    test_result = result_sink_util._compose_test_result(
        'TestCase/testSomething',
        'PASS',
        True,
        test_log=short_log,
        duration=1233,
        file_artifacts={'name': '/path/to/name'})
    expected = {
        'testId': 'TestCase/testSomething',
        'status': 'PASS',
        'expected': True,
        'summaryHtml': '<text-artifact artifact-id="Test Log" />',
        'artifacts': {
            'Test Log': {
                'contents':
                    base64.b64encode(short_log.encode('utf-8')).decode('utf-8')
            },
            'name': {
                'filePath': '/path/to/name'
            },
        },
        'duration': '1.233000000s',
        'tags': [],
        'testMetadata': {
            'name': 'TestCase/testSomething',
            'location': None,
        },
    }
    self.assertEqual(test_result, expected)

  def test_parsing_crash_message(self):
    """Tests parsing crash message from test log and setting it as the
    failure reason"""
    test_result = result_sink_util._compose_test_result(
        'TestCase/testSomething', 'FAIL', False, test_log=CRASH_TEST_LOG)
    expected = {
        'testId': 'TestCase/testSomething',
        'status': 'FAIL',
        'expected': False,
        'summaryHtml': '<text-artifact artifact-id="Test Log" />',
        'tags': [],
        'failureReason': {
            'primaryErrorMessage': 'App crashed and disconnected.'
        },
        'artifacts': {
            'Test Log': {
                'contents':
                    base64.b64encode(CRASH_TEST_LOG.encode('utf-8')
                                    ).decode('utf-8')
            },
        },
        'testMetadata': {
            'name': 'TestCase/testSomething',
            'location': None,
        },
    }
    self.assertEqual(test_result, expected)

  def test_long_test_log(self):
    """Tests long test log is reported as expected."""
    len_32_str = 'This is a string in length of 32'
    self.assertEqual(len(len_32_str), 32)
    len_4128_str = (4 * 32 + 1) * len_32_str
    self.assertEqual(len(len_4128_str), 4128)

    expected = {
        'testId': 'TestCase/testSomething',
        'status': 'PASS',
        'expected': True,
        'summaryHtml': '<text-artifact artifact-id="Test Log" />',
        'artifacts': {
            'Test Log': {
                'contents':
                    base64.b64encode(len_4128_str.encode('utf-8')
                                    ).decode('utf-8')
            },
        },
        'tags': [],
        'testMetadata': {
            'name': 'TestCase/testSomething',
            'location': None,
        },
    }
    test_result = result_sink_util._compose_test_result(
        'TestCase/testSomething', 'PASS', True, test_log=len_4128_str)
    self.assertEqual(test_result, expected)

  def test_compose_test_result_assertions(self):
    """Tests invalid status is rejected"""
    with self.assertRaises(AssertionError):
      test_result = result_sink_util._compose_test_result(
          'TestCase/testSomething', 'SOME_INVALID_STATUS', True)

    with self.assertRaises(AssertionError):
      test_result = result_sink_util._compose_test_result(
          'TestCase/testSomething', 'PASS', True, tags=('a', 'b'))

    with self.assertRaises(AssertionError):
      test_result = result_sink_util._compose_test_result(
          'TestCase/testSomething',
          'PASS',
          True,
          tags=[('a', 'b', 'c'), ('d', 'e')])

    with self.assertRaises(AssertionError):
      test_result = result_sink_util._compose_test_result(
          'TestCase/testSomething', 'PASS', True, tags=[('a', 'b'), ('c', 3)])

  def test_composed_with_tags(self):
    """Tests tags is in correct format."""
    expected = {
        'testId': 'TestCase/testSomething',
        'status': 'SKIP',
        'expected': True,
        'tags': [{
            'key': 'disabled_test',
            'value': 'true',
        }],
        'testMetadata': {
            'name': 'TestCase/testSomething',
            'location': None,
        },
    }
    test_result = result_sink_util._compose_test_result(
        'TestCase/testSomething',
        'SKIP',
        True,
        tags=[('disabled_test', 'true')])
    self.assertEqual(test_result, expected)

  def test_composed_with_location(self):
    """Tests with test locations"""
    test_loc = {'repo': 'https://test', 'fileName': '//test.cc'}
    expected = {
        'testId': 'TestCase/testSomething',
        'status': 'SKIP',
        'expected': True,
        'tags': [{
            'key': 'disabled_test',
            'value': 'true',
        }],
        'testMetadata': {
            'name': 'TestCase/testSomething',
            'location': test_loc,
        },
    }
    test_result = result_sink_util._compose_test_result(
        'TestCase/testSomething',
        'SKIP',
        True,
        test_loc=test_loc,
        tags=[('disabled_test', 'true')])
    self.assertEqual(test_result, expected)

  @mock.patch.object(requests.Session, 'post')
  @mock.patch('%s.open' % 'result_sink_util',
              mock.mock_open(read_data=LUCI_CONTEXT_FILE_DATA))
  @mock.patch('os.environ.get', return_value='filename')
  def test_post_test_result(self, mock_open_file, mock_session_post):
    test_result = {
        'testId': 'TestCase/testSomething',
        'status': 'SKIP',
        'expected': True,
        'tags': [{
            'key': 'disabled_test',
            'value': 'true',
        }],
        'testMetadata': {
            'name': 'TestCase/testSomething',
            'location': None,
        },
    }
    client = result_sink_util.ResultSinkClient()

    client._post_test_result(test_result)
    mock_session_post.assert_called_with(
        url=SINK_POST_URL,
        headers=HEADERS,
        data=json.dumps({'testResults': [test_result]}))

  @mock.patch.object(requests.Session, 'close')
  @mock.patch.object(requests.Session, 'post')
  @mock.patch('%s.open' % 'result_sink_util',
              mock.mock_open(read_data=LUCI_CONTEXT_FILE_DATA))
  @mock.patch('os.environ.get', return_value='filename')
  def test_close(self, mock_open_file, mock_session_post, mock_session_close):

    client = result_sink_util.ResultSinkClient()

    client._post_test_result({'some': 'result'})
    mock_session_post.assert_called()

    client.close()
    mock_session_close.assert_called()

  def test_post(self):
    client = result_sink_util.ResultSinkClient()
    client.sink = 'Make sink not None so _compose_test_result will be called'
    client._post_test_result = mock.MagicMock()

    client.post(
        'testname',
        'PASS',
        True,
        test_log='some_log',
        tags=[('tag key', 'tag value')])
    client._post_test_result.assert_called_with(
        result_sink_util._compose_test_result(
            'testname',
            'PASS',
            True,
            test_log='some_log',
            tags=[('tag key', 'tag value')]))

    client.post('testname', 'PASS', True, test_log='some_log')
    client._post_test_result.assert_called_with(
        result_sink_util._compose_test_result(
            'testname', 'PASS', True, test_log='some_log'))

    client.post('testname', 'PASS', True)
    client._post_test_result.assert_called_with(
        result_sink_util._compose_test_result('testname', 'PASS', True))


if __name__ == '__main__':
  unittest.main()
