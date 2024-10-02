#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for xcode_log_parser.py."""

import json
import mock
import os
import unittest

from test_result_util import TestStatus
import test_runner
import test_runner_test
import xcode_log_parser


OUTPUT_PATH = '/tmp/attempt_0'
XCRESULT_PATH = '/tmp/attempt_0.xcresult'
XCODE11_DICT = {
    'path': '/Users/user1/Xcode.app',
    'version': '11.0',
    'build': '11M336w',
}
# A sample of json result when executing xcresulttool on .xcresult dir without
# --id. Some unused keys and values were removed.
XCRESULT_ROOT = """
{
  "_type" : {
    "_name" : "ActionsInvocationRecord"
  },
  "actions" : {
    "_values" : [
      {
        "actionResult" : {
          "_type" : {
            "_name" : "ActionResult"
          },
          "diagnosticsRef" : {
            "id" : {
              "_value" : "DIAGNOSTICS_REF_ID"
            }
          },
          "logRef" : {
            "id" : {
              "_value" : "0~6jr1GkZxoWVzWfcUNA5feff3l7g8fPHJ1rqKetCBa3QXhCGY74PnEuRwzktleMTFounMfCdDpSr1hRfhUGIUEQ=="
            }
          },
          "testsRef" : {
            "id" : {
              "_value" : "0~iRbOkDnmtKVIvHSV2jkeuNcg4RDTUaCLZV7KijyxdCqvhqtp08MKxl0MwjBAPpjmruoI7qNHzBR1RJQAlANNHA=="
            }
          }
        }
      }
    ]
  },
  "issues" : {
    "testFailureSummaries" : {
      "_values" : [
        {
          "documentLocationInCreatingWorkspace" : {
            "url" : {
              "_value" : "file:\/\/\/..\/..\/ios\/web\/shell\/test\/page_state_egtest.mm#CharacterRangeLen=0&EndingLineNumber=130&StartingLineNumber=130"
            }
          },
          "message" : {
            "_value": "Fail. Screenshots: {\\n\\"Failure\\": \\"path.png\\"\\n}"
          },
          "testCaseName" : {
            "_value": "-[PageStateTestCase testZeroContentOffsetAfterLoad]"
          }
        }
      ]
    }
  },
  "metrics" : {
    "testsCount" : {
      "_value" : "2"
    },
    "testsFailedCount" : {
      "_value" : "1"
    }
  }
}"""

REF_ID = b"""
  {
    "actions": {
      "_values": [{
        "actionResult": {
          "testsRef": {
            "id": {
              "_value": "REF_ID"
            }
          }
        }
      }]
    }
  }"""

# A sample of json result when executing xcresulttool on .xcresult dir with
# "testsRef" as --id input. Some unused keys and values were removed.
TESTS_REF = """
  {
    "summaries": {
      "_values": [{
        "testableSummaries": {
          "_type": {
            "_name": "Array"
          },
          "_values": [{
            "tests": {
              "_type": {
                "_name": "Array"
              },
              "_values": [{
                "identifier" : {
                  "_value" : "All tests"
                },
                "name" : {
                  "_value" : "All tests"
                },
                "subtests": {
                  "_values": [{
                    "identifier" : {
                      "_value" : "ios_web_shell_eg2tests_module.xctest"
                    },
                    "name" : {
                      "_value" : "ios_web_shell_eg2tests_module.xctest"
                    },
                    "subtests": {
                      "_values": [{
                        "identifier" : {
                          "_value" : "PageStateTestCase"
                        },
                        "name" : {
                          "_value" : "PageStateTestCase"
                        },
                        "subtests": {
                          "_values": [{
                            "testStatus": {
                              "_value": "Success"
                            },
                            "duration" : {
                              "_type" : {
                                 "_name" : "Double"
                              },
                              "_value" : "35.38412606716156"
                            },
                            "identifier": {
                              "_value": "PageStateTestCase/testMethod1"
                            },
                            "name": {
                              "_value": "testMethod1"
                            }
                          },
                          {
                            "summaryRef": {
                              "id": {
                                "_value": "0~7Q_uAuUSJtx9gtHM08psXFm3g_xiTTg5bpdoDO88nMXo_iMwQTXpqlrlMe5AtkYmnZ7Ux5uEgAe83kJBfoIckw=="
                              }
                            },
                            "testStatus": {
                              "_value": "Failure"
                            },
                            "identifier": {
                              "_value": "PageStateTestCase\/testZeroContentOffsetAfterLoad"
                            },
                            "name": {
                              "_value": "testZeroContentOffsetAfterLoad"
                            }
                          },
                          {
                            "testStatus": {
                              "_value": "Expected Failure"
                            },
                            "duration" : {
                              "_type" : {
                                 "_name" : "Double"
                              },
                              "_value" : "28.988606716156"
                            },
                            "identifier": {
                              "_value": "PageStateTestCase/testMethod2"
                            },
                            "name": {
                              "_value": "testMethod2"
                            }
                          },
                          {
                            "testStatus": {
                              "_value": "Skipped"
                            },
                            "duration" : {
                              "_type" : {
                                 "_name" : "Double"
                              },
                              "_value" : "0.0606716156"
                            },
                            "identifier": {
                              "_value": "PageStateTestCase/testMethod3"
                            },
                            "name": {
                              "_value": "testMethod3"
                            }
                          }]
                        }
                      }]
                    }
                  }]
                }
              }]
            }
          }]
        }
      }]
    }
  }
"""

# A sample of json result when executing xcresulttool on .xcresult dir with
# a single test summaryRef id value as --id input. Some unused keys and values
# were removed.
SINGLE_TEST_SUMMARY_REF = """
{
  "_type" : {
    "_name" : "ActionTestSummary",
    "_supertype" : {
      "_name" : "ActionTestSummaryIdentifiableObject",
      "_supertype" : {
        "_name" : "ActionAbstractTestSummary"
      }
    }
  },
  "activitySummaries" : {
    "_values" : [
      {
        "attachments" : {
          "_values" : [
            {
              "filename" : {
                "_value" : "Screenshot_25659115-F3E4-47AE-AA34-551C94333D7E.jpg"
              },
              "payloadRef" : {
                "id" : {
                  "_value" : "SCREENSHOT_REF_ID_1"
                }
              }
            }
          ]
        },
        "title" : {
          "_value" : "Start Test at 2020-10-19 14:12:58.111"
        }
      },
      {
        "subactivities" : {
          "_values" : [
            {
              "attachments" : {
                "_values" : [
                  {
                    "filename" : {
                      "_value" : "Screenshot_23D95D0E-8B97-4F99-BE3C-A46EDE5999D7.jpg"
                    },
                    "payloadRef" : {
                      "id" : {
                        "_value" : "SCREENSHOT_REF_ID_2"
                      }
                    }
                  }
                ]
              },
              "subactivities" : {
                "_values" : [
                  {
                    "subactivities" : {
                      "_values" : [
                        {
                          "attachments" : {
                            "_values" : [
                              {
                                "filename" : {
                                  "_value" : "Crash_3F0A2B1C-7ADA-436E-A54C-D4C39B8411F8.crash"
                                },
                                "payloadRef" : {
                                  "id" : {
                                    "_value" : "CRASH_REF_ID_IN_ACTIVITY_SUMMARIES"
                                  }
                                }
                              }
                            ]
                          },
                          "title" : {
                            "_value" : "Wait for org.chromium.ios-web-shell-eg2tests to idle"
                          }
                        }
                      ]
                    },
                    "title" : {
                      "_value" : "Activate org.chromium.ios-web-shell-eg2tests"
                    }
                  }
                ]
              },
              "title" : {
                "_value" : "Open org.chromium.ios-web-shell-eg2tests"
              }
            }
          ]
        },
        "title" : {
          "_value" : "Set Up"
        }
      },
      {
        "title" : {
          "_value" : "Find the Target Application 'org.chromium.ios-web-shell-eg2tests'"
        }
      },
      {
        "attachments" : {
          "_values" : [
            {
              "filename" : {
                "_value" : "Screenshot_278BA84B-2196-4CCD-9D31-2C07DDDC9DFC.jpg"
              },
              "payloadRef" : {
                "id" : {
                  "_value" : "SCREENSHOT_REF_ID_3"
                }
              }

            }
          ]
        },
        "title" : {
          "_value" : "Uncaught Exception at page_state_egtest.mm:131: \\nCannot scroll, the..."
        }
      },
      {
        "title" : {
          "_value" : "Uncaught Exception: Immediately halt execution of testcase (EarlGreyInternalTestInterruptException)"
        }
      },
      {
        "title" : {
          "_value" : "Tear Down"
        }
      }
    ]
  },
  "failureSummaries" : {
    "_values" : [
      {
        "attachments" : {
          "_values" : [
            {
              "filename" : {
                "_value" : "kXCTAttachmentLegacyScreenImageData_1_6CED1FE5-96CA-47EA-9852-6FADED687262.jpeg"
              },
              "payloadRef" : {
                "id" : {
                  "_value" : "SCREENSHOT_REF_ID_IN_FAILURE_SUMMARIES"
                }
              }
            }
          ]
        },
        "fileName" : {
          "_value" : "\/..\/..\/ios\/web\/shell\/test\/page_state_egtest.mm"
        },
        "lineNumber" : {
          "_value" : "131"
        },
        "message" : {
          "_value" : "Some logs."
        }
      },
      {
        "message" : {
          "_value" : "Immediately halt execution of testcase (EarlGreyInternalTestInterruptException)"
        }
      }
    ]
  },
  "identifier" : {
    "_value" : "PageStateTestCase\/testZeroContentOffsetAfterLoad"
  },
  "name" : {
    "_value" : "testZeroContentOffsetAfterLoad"
  },
  "testStatus" : {
    "_value" : "Failure"
  }
}"""

def _xcresulttool_get_side_effect(xcresult_path, ref_id=None):
  """Side effect for _xcresulttool_get in Xcode11LogParser tested."""
  if ref_id is None:
    return XCRESULT_ROOT
  if ref_id == 'testsRef':
    return TESTS_REF
  # Other situation in use cases of xcode_log_parser is asking for single test
  # summary ref.
  return SINGLE_TEST_SUMMARY_REF


class UtilMethodsTest(test_runner_test.TestCase):
  """Test case for utility methods not related with Parser class."""

  def testParseTestsForInterruptedRun(self):
    test_output = """
    Test case '-[DownloadManagerTestCase testVisibleFileNameAndOpenInDownloads]' passed on 'Clone 2 of iPhone X 15.0 test simulator - ios_chrome_ui_eg2tests_module-Runner (34498)' (20.715 seconds)
    Test case '-[SyncFakeServerTestCase testSyncDownloadBookmark]' passed on 'Clone 1 of iPhone X 15.0 test simulator - ios_chrome_ui_eg2tests_module-Runner (34249)' (14.880 seconds)
    Random lines
         t =    53.90s Tear Down
    Test Case '-[LinkToTextTestCase testGenerateLinkForSimpleText]' failed (55.316 seconds).
     t =      nans Suite Tear Down
    Test Suite 'LinkToTextTestCase' failed at 2021-06-15 07:13:17.406.
      Executed 1 test, with 6 failures (6 unexpected) in 55.316 (55.338) seconds
    Test Suite 'ios_chrome_ui_eg2tests_module.xctest' failed at 2021-06-15 07:13:17.407.
      Executed 1 test, with 6 failures (6 unexpected) in 55.316 (55.340) seconds
    Test Suite 'Selected tests' failed at 2021-06-15 07:13:17.408.
      Executed 1 test, with 6 failures (6 unexpected) in 55.316 (55.342) seconds
    """
    test_output_list = test_output.split('\n')
    expected_passed = set([
        'DownloadManagerTestCase/testVisibleFileNameAndOpenInDownloads',
        'SyncFakeServerTestCase/testSyncDownloadBookmark'
    ])
    expected_failed = set(['LinkToTextTestCase/testGenerateLinkForSimpleText'])
    expected_failed_message = 'Test failed in interrupted(timedout) run.'

    results = xcode_log_parser.parse_passed_failed_tests_for_interrupted_run(
        test_output_list)
    self.assertEqual(results.expected_tests(), expected_passed)
    self.assertEqual(results.unexpected_tests(), expected_failed)
    for result in results.test_results:
      if result.name == 'LinkToTextTestCase/testGenerateLinkForSimpleText':
        self.assertEqual(result.test_log, expected_failed_message)


class XCode11LogParserTest(test_runner_test.TestCase):
  """Test case to test Xcode11LogParser."""

  def setUp(self):
    super(XCode11LogParserTest, self).setUp()
    self.mock(test_runner, 'get_current_xcode_info', lambda: XCODE11_DICT)

  @mock.patch('xcode_util.version', autospec=True)
  def testGetParser(self, mock_xcode_version):
    mock_xcode_version.return_value = ('12.0', '12A7209')
    self.assertEqual(xcode_log_parser.get_parser().__class__.__name__,
        'Xcode11LogParser')
    mock_xcode_version.return_value = ('11.4', '11E146')
    self.assertEqual(xcode_log_parser.get_parser().__class__.__name__,
        'Xcode11LogParser')
    mock_xcode_version.return_value = ('10.3', '10G8')
    self.assertEqual(xcode_log_parser.get_parser().__class__.__name__,
        'XcodeLogParser')

  @mock.patch('subprocess.check_output', autospec=True)
  def testXcresulttoolGetRoot(self, mock_process):
    mock_process.return_value = b'%JSON%'
    xcode_log_parser.Xcode11LogParser()._xcresulttool_get('xcresult_path')
    self.assertTrue(
        os.path.join(XCODE11_DICT['path'], 'usr', 'bin') in os.environ['PATH'])
    self.assertEqual(
        ['xcresulttool', 'get', '--format', 'json', '--path', 'xcresult_path'],
        mock_process.mock_calls[0][1][0])

  @mock.patch('subprocess.check_output', autospec=True)
  def testXcresulttoolGetRef(self, mock_process):
    mock_process.side_effect = [REF_ID, b'JSON']
    xcode_log_parser.Xcode11LogParser()._xcresulttool_get('xcresult_path',
                                                          'testsRef')
    self.assertEqual(
        ['xcresulttool', 'get', '--format', 'json', '--path', 'xcresult_path'],
        mock_process.mock_calls[0][1][0])
    self.assertEqual([
        'xcresulttool', 'get', '--format', 'json', '--path', 'xcresult_path',
        '--id', 'REF_ID'], mock_process.mock_calls[1][1][0])

  def testXcresulttoolListFailedTests(self):
    failure_message = (
        'file:///../../ios/web/shell/test/page_state_egtest.mm#'
        'CharacterRangeLen=0&EndingLineNumber=130&StartingLineNumber=130\n'
        'Fail. Screenshots: {\n\"Failure\": \"path.png\"\n}')
    expected = set(['PageStateTestCase/testZeroContentOffsetAfterLoad'])
    results = xcode_log_parser.Xcode11LogParser()._list_of_failed_tests(
        json.loads(XCRESULT_ROOT))
    self.assertEqual(expected, results.failed_tests())
    log = results.test_results[0].test_log
    self.assertEqual(log, failure_message)

  def testXcresulttoolListFailedTestsExclude(self):
    excluded = set(['PageStateTestCase/testZeroContentOffsetAfterLoad'])
    results = xcode_log_parser.Xcode11LogParser()._list_of_failed_tests(
        json.loads(XCRESULT_ROOT), excluded=excluded)
    self.assertEqual(set([]), results.all_test_names())

  @mock.patch('xcode_log_parser.Xcode11LogParser._export_data')
  @mock.patch('xcode_log_parser.Xcode11LogParser._xcresulttool_get')
  def testGetTestStatuses(self, mock_xcresult, mock_export):
    mock_xcresult.side_effect = _xcresulttool_get_side_effect
    #   self.assertEqual(test_result.test_log, lo
    expected_failure_log = (
        'Logs from "failureSummaries" in .xcresult:\n'
        'file: /../../ios/web/shell/test/page_state_egtest.mm, line: 131\n'
        'Some logs.\n'
        'file: , line: \n'
        'Immediately halt execution of testcase '
        '(EarlGreyInternalTestInterruptException)\n')
    expected_expected_tests = set([
        'PageStateTestCase/testMethod1', 'PageStateTestCase/testMethod2',
        'PageStateTestCase/testMethod3'
    ])
    results = xcode_log_parser.Xcode11LogParser()._get_test_statuses(
        OUTPUT_PATH)
    self.assertEqual(expected_expected_tests, results.expected_tests())
    seen_failed_test = False
    for test_result in results.test_results:
      if test_result.name == 'PageStateTestCase/testZeroContentOffsetAfterLoad':
        seen_failed_test = True
        self.assertEqual(test_result.test_log, expected_failure_log)
        self.assertEqual(test_result.duration, None)
        crash_file_name = (
            'attempt_0_PageStateTestCase_testZeroContentOffsetAfterLoad_'
            'Crash_3F0A2B1C-7ADA-436E-A54C-D4C39B8411F8.crash'
        )
        jpeg_file_name = (
            'attempt_0_PageStateTestCase_testZeroContentOffsetAfterLoad'
            '_kXCTAttachmentLegacyScreenImageData_1'
            '_6CED1FE5-96CA-47EA-9852-6FADED687262.jpeg')
        self.assertDictEqual(
            {
                crash_file_name: '/tmp/%s' % crash_file_name,
                jpeg_file_name: '/tmp/%s' % jpeg_file_name,
            }, test_result.attachments)
      if test_result.name == 'PageStateTestCase/testMethod1':
        self.assertEqual(test_result.duration, 35384)
      if test_result.name == 'PageStateTestCase/testMethod2':
        self.assertEqual(test_result.duration, 28988)
      if test_result.name == 'PageStateTestCase/testMethod3':
        self.assertEqual(test_result.duration, 60)

    self.assertTrue(seen_failed_test)

  @mock.patch('file_util.zip_and_remove_folder')
  @mock.patch('xcode_log_parser.Xcode11LogParser._extract_artifacts_for_test')
  @mock.patch('xcode_log_parser.Xcode11LogParser.export_diagnostic_data')
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('xcode_log_parser.Xcode11LogParser._xcresulttool_get')
  def testCollectTestTesults(self, mock_root, mock_exist_file, *args):
    expected_passed = set([
        'PageStateTestCase/testMethod1', 'PageStateTestCase/testMethod2',
        'PageStateTestCase/testMethod3'
    ])
    expected_failed = set(['PageStateTestCase/testZeroContentOffsetAfterLoad'])

    mock_root.side_effect = _xcresulttool_get_side_effect
    mock_exist_file.return_value = True
    results = xcode_log_parser.Xcode11LogParser().collect_test_results(
        OUTPUT_PATH, [])

    # Length ensures no duplicate results from |_get_test_statuses| and
    # |_list_of_failed_tests|.
    self.assertEqual(len(results.test_results), 4)
    self.assertEqual(expected_passed, results.expected_tests())
    self.assertEqual(expected_failed, results.unexpected_tests())
    # Ensure format.
    for test in results.test_results:
      self.assertTrue(isinstance(test.name, str))
      if test.status == TestStatus.FAIL:
        self.assertTrue(isinstance(test.test_log, str))

  @mock.patch('file_util.zip_and_remove_folder')
  @mock.patch('xcode_log_parser.Xcode11LogParser.copy_artifacts')
  @mock.patch('xcode_log_parser.Xcode11LogParser.export_diagnostic_data')
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('xcode_log_parser.Xcode11LogParser._xcresulttool_get')
  def testCollectTestsRanZeroTests(self, mock_root, mock_exist_file, *args):
    metrics_json = '{"metrics": {}}'
    mock_root.return_value = metrics_json
    mock_exist_file.return_value = True
    results = xcode_log_parser.Xcode11LogParser().collect_test_results(
        OUTPUT_PATH, [])
    self.assertTrue(results.crashed)
    self.assertEqual(results.crash_message, '0 tests executed!')
    self.assertEqual(len(results.all_test_names()), 0)

  @mock.patch('os.path.exists', autospec=True)
  def testCollectTestsDidNotRun(self, mock_exist_file):
    mock_exist_file.return_value = False
    results = xcode_log_parser.Xcode11LogParser().collect_test_results(
        OUTPUT_PATH, [])
    self.assertTrue(results.crashed)
    self.assertEqual(results.crash_message,
                     '/tmp/attempt_0 with staging data does not exist.\n')
    self.assertEqual(len(results.all_test_names()), 0)

  @mock.patch('os.path.exists', autospec=True)
  def testCollectTestsInterruptedRun(self, mock_exist_file):
    mock_exist_file.side_effect = [True, False]
    results = xcode_log_parser.Xcode11LogParser().collect_test_results(
        OUTPUT_PATH, [])
    self.assertTrue(results.crashed)
    self.assertEqual(
        results.crash_message,
        '/tmp/attempt_0.xcresult with test results does not exist.\n')
    self.assertEqual(len(results.all_test_names()), 0)

  @mock.patch('subprocess.check_output', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('xcode_log_parser.Xcode11LogParser._xcresulttool_get')
  def testCopyScreenshots(self, mock_xcresulttool_get, mock_path_exists,
                          mock_process):
    mock_path_exists.return_value = True
    mock_xcresulttool_get.side_effect = _xcresulttool_get_side_effect
    xcode_log_parser.Xcode11LogParser().copy_artifacts(OUTPUT_PATH)
    mock_process.assert_any_call([
        'xcresulttool', 'export', '--type', 'file', '--id',
        'SCREENSHOT_REF_ID_IN_FAILURE_SUMMARIES', '--path', XCRESULT_PATH,
        '--output-path',
        '/tmp/attempt_0_PageStateTestCase_testZeroContentOffsetAfterLoad'
        '_kXCTAttachmentLegacyScreenImageData_1'
        '_6CED1FE5-96CA-47EA-9852-6FADED687262.jpeg'
    ])
    mock_process.assert_any_call([
        'xcresulttool', 'export', '--type', 'file', '--id',
        'CRASH_REF_ID_IN_ACTIVITY_SUMMARIES', '--path', XCRESULT_PATH,
        '--output-path',
        '/tmp/attempt_0_PageStateTestCase_testZeroContentOffsetAfterLoad'
        '_Crash_3F0A2B1C-7ADA-436E-A54C-D4C39B8411F8.crash'
    ])
    # Ensures screenshots in activitySummaries are not copied.
    self.assertEqual(2, mock_process.call_count)

  @mock.patch('file_util.zip_and_remove_folder')
  @mock.patch('subprocess.check_output', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('xcode_log_parser.Xcode11LogParser._xcresulttool_get')
  def testExportDiagnosticData(self, mock_xcresulttool_get, mock_path_exists,
                               mock_process, _):
    mock_path_exists.return_value = True
    mock_xcresulttool_get.side_effect = _xcresulttool_get_side_effect
    xcode_log_parser.Xcode11LogParser.export_diagnostic_data(OUTPUT_PATH)
    mock_process.assert_called_with([
        'xcresulttool', 'export', '--type', 'directory', '--id',
        'DIAGNOSTICS_REF_ID', '--path', XCRESULT_PATH, '--output-path',
        '/tmp/attempt_0.xcresult_diagnostic'
    ])

  @mock.patch('file_util.zip_and_remove_folder')
  @mock.patch('shutil.copy')
  @mock.patch('subprocess.check_output', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('xcode_log_parser.Xcode11LogParser._xcresulttool_get')
  def testStdoutCopiedInExportDiagnosticData(self, mock_xcresulttool_get,
                                             mock_path_exists, mock_process,
                                             mock_copy, _):
    output_path_in_test = 'test_data/attempt_0'
    xcresult_path_in_test = 'test_data/attempt_0.xcresult'
    mock_path_exists.return_value = True
    mock_xcresulttool_get.side_effect = _xcresulttool_get_side_effect
    xcode_log_parser.Xcode11LogParser.export_diagnostic_data(
        output_path_in_test)
    # os.walk() walks folders in unknown sequence. Use try-except blocks to
    # assert that any of the 2 assertions is true.
    try:
      mock_copy.assert_any_call(
          'test_data/attempt_0.xcresult_diagnostic/test_module-UUID/test_module-UUID1/StandardOutputAndStandardError.txt',
          'test_data/attempt_0/../attempt_0_simulator#1_StandardOutputAndStandardError.txt'
      )
    except AssertionError:
      mock_copy.assert_any_call(
          'test_data/attempt_0.xcresult_diagnostic/test_module-UUID/test_module-UUID1/StandardOutputAndStandardError.txt',
          'test_data/attempt_0/../attempt_0_simulator#0_StandardOutputAndStandardError.txt'
      )
    try:
      mock_copy.assert_any_call(
          'test_data/attempt_0.xcresult_diagnostic/test_module-UUID/test_module-UUID2/StandardOutputAndStandardError-org.chromium.gtest.ios-chrome-eg2tests.txt',
          'test_data/attempt_0/../attempt_0_simulator#1_StandardOutputAndStandardError-org.chromium.gtest.ios-chrome-eg2tests.txt'
      )
    except AssertionError:
      mock_copy.assert_any_call(
          'test_data/attempt_0.xcresult_diagnostic/test_module-UUID/test_module-UUID2/StandardOutputAndStandardError-org.chromium.gtest.ios-chrome-eg2tests.txt',
          'test_data/attempt_0/../attempt_0_simulator#0_StandardOutputAndStandardError-org.chromium.gtest.ios-chrome-eg2tests.txt'
      )

  @mock.patch('os.path.exists', autospec=True)
  def testCollectTestResults_interruptedTests(self, mock_path_exists):
    mock_path_exists.side_effect = [True, False]
    output = [
        '[09:03:42:INFO] Test case \'-[TestCase1 method1]\' passed on device.',
        '[09:06:40:INFO] Test Case \'-[TestCase2 method1]\' passed on device.',
        '[09:09:00:INFO] Test case \'-[TestCase2 method1]\' failed on device.',
        '** BUILD INTERRUPTED **',
    ]
    not_found_message = ['%s with test results does not exist.' % XCRESULT_PATH]
    res = xcode_log_parser.Xcode11LogParser().collect_test_results(
        OUTPUT_PATH, output)
    self.assertTrue(res.crashed)
    self.assertEqual('\n'.join(not_found_message + output), res.crash_message)
    self.assertEqual(
        set(['TestCase1/method1', 'TestCase2/method1']), res.expected_tests())

  @mock.patch('file_util.zip_and_remove_folder')
  @mock.patch('xcode_log_parser.Xcode11LogParser._extract_artifacts_for_test')
  @mock.patch('xcode_log_parser.Xcode11LogParser.export_diagnostic_data')
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('xcode_log_parser.Xcode11LogParser._xcresulttool_get')
  @mock.patch('xcode_log_parser.Xcode11LogParser._list_of_failed_tests')
  def testArtifactsDiagnosticLogsExportedInCollectTestTesults(
      self, mock_get_failed_tests, mock_root, mock_exist_file,
      mock_export_diagnostic_data, mock_extract_artifacts, mock_zip):
    mock_root.side_effect = _xcresulttool_get_side_effect
    mock_exist_file.return_value = True
    xcode_log_parser.Xcode11LogParser().collect_test_results(OUTPUT_PATH, [])
    mock_export_diagnostic_data.assert_called_with(OUTPUT_PATH)
    mock_extract_artifacts.assert_called()


if __name__ == '__main__':
  unittest.main()
