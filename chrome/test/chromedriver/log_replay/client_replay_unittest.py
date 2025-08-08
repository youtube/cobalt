#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the client_replay.CommandSequence class."""

import io
import optparse
import os
import sys
import unittest

import client_replay

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
_PARENT_DIR = os.path.join(_THIS_DIR, os.pardir)
_TEST_DIR = os.path.join(_PARENT_DIR, "test")
# pylint: disable=g-import-not-at-top
sys.path.insert(1, _TEST_DIR)
import unittest_util
sys.path.remove(_TEST_DIR)

sys.path.insert(1, _PARENT_DIR)
import util
sys.path.insert(1, _PARENT_DIR)
# pylint: enable=g-import-not-at-top

_SESSION_ID = "b15232d5497ec0d8300a5a1ea56f33ce"
_SESSION_ID_ALT = "a81dc5521092a5ba132b9c0b6cf6e84f"

_NO_PARAMS = ("[1531428669.535][INFO]: [b15232d5497ec0d8300a5a1ea56f33ce] "
              "COMMAND GetTitle {\n\n}\n"
              "[1531428670.535][INFO]: [b15232d5497ec0d8300a5a1ea56f33ce] "
              "RESPONSE GetTitle\n")
_WITH_PARAMS = ('[1531428669.535][INFO]: [b15232d5497ec0d8300a5a1ea56f33ce] '
                'COMMAND GetTitle {\n"param1": 7\n}\n'
                '[1531428670.535][INFO]: [b15232d5497ec0d8300a5a1ea56f33ce] '
                'RESPONSE GetTitle {\n"param2": 42\n}\n')
_COMMAND_ONLY = ('[1531428670.535][INFO]: [b15232d5497ec0d8300a5a1ea56f33ce] '
                 'COMMAND GetTitle {\n"param1": 7\n}\n')
_RESPONSE_ONLY = ('[1531428670.535][INFO]: [b15232d5497ec0d8300a5a1ea56f33ce] '
                  'RESPONSE GetTitle {\n"param2": 42\n}\n')
_PAYLOAD_SCRIPT = ('[1531428670.535][INFO]: [b15232d5497ec0d8300a5a1ea56f33ce]'
                   ' RESPONSE GetTitle {\n"param2": "function(){func()}"\n}\n')
_PAYLOAD_READABLE_TIME_LINUX = (
    '[08-12-2019 15:45:34.824002][INFO]: [b15232d5497ec0d8300a5a1ea56f33ce]'
    ' RESPONSE GetTitle {\n"param2": "function(){func()}"\n}\n')
_PAYLOAD_READABLE_TIME_WINDOWS = (
    '[08-12-2019 15:45:34.824][INFO]: [b15232d5497ec0d8300a5a1ea56f33ce]'
    ' RESPONSE GetTitle {\n"param2": "function(){func()}"\n}\n')
_BAD_SCRIPT = ('[1531428670.535][INFO]: [b15232d5497ec0d8300a5a1ea56f33ce]'
               ' RESPONSE GetTitle {\n"param2": "))}\\})}/{)}({(})}"\n}\n')
_MULTI_SESSION = ('[1531428669.535][INFO]: [b15232d5497ec0d8300a5a1ea56f33ce] '
                  'COMMAND GetSessions {\n\n}\n'
                  '[1531428669.535][INFO]: [a81dc5521092a5ba132b9c0b6cf6e84f] '
                  'COMMAND GetSessions {\n\n}\n'
                  '[1531428670.535][INFO]: [b15232d5497ec0d8300a5a1ea56f33ce] '
                  'RESPONSE GetSessions {\n"param2": 42\n}\n'
                  '[1531428670.535][INFO]: [a81dc5521092a5ba132b9c0b6cf6e84f] '
                  'RESPONSE GetSessions {\n"param2": 42\n}\n' + _COMMAND_ONLY)

_WINDOW_ID_1 = "11111111111111111111111111111111"
_WINDOW_ID_2 = "22222222222222222222222222222222"
_WINDOW_IDS = [
    _WINDOW_ID_1,
    _WINDOW_ID_2,
    "other thing",  # Random string not in the targetID format.
    "1234567890123456789012345678901",  # Too short string.
    "123456789012345678901234567890123",  # Too long string.
    "GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",  # String with not allowed symbol.
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",  # String with not allowed symbol.
]
_ELEMENT_ID = {"element-6066-11e4-a52e-4f735466cecf": "0.87-1"}
_ELEMENT_IDS = [{"element-6066-11e4-a52e-4f735466cecf": "0.87-1"},
                {"element-6066-11e4-a52e-4f735466cecf": "0.87-2"}]


class ChromeDriverClientReplayUnitTest(unittest.TestCase):
  """base class for test cases"""

  def __init__(self, *args, **kwargs):
    super(ChromeDriverClientReplayUnitTest, self).__init__(*args, **kwargs)

  def testNextCommandEmptyParams(self):
    string_buffer = io.StringIO(_NO_PARAMS)
    command_sequence = client_replay.CommandSequence()
    command_sequence._parser = client_replay._Parser(string_buffer)
    command = command_sequence.NextCommand(None)
    response = command_sequence._last_response

    self.assertEqual(command.name, "GetTitle")
    self.assertEqual(command.GetPayloadPrimitive(), {"sessionId": _SESSION_ID})
    self.assertEqual(command.session_id, _SESSION_ID)

    self.assertEqual(response.name, "GetTitle")
    self.assertEqual(response.GetPayloadPrimitive(), None)
    self.assertEqual(response.session_id, _SESSION_ID)

  def testNextCommandWithParams(self):
    string_buffer = io.StringIO(_WITH_PARAMS)
    command_sequence = client_replay.CommandSequence()
    command_sequence._parser = client_replay._Parser(string_buffer)
    command = command_sequence.NextCommand(None)
    response = command_sequence._last_response

    self.assertEqual(command.name, "GetTitle")
    self.assertEqual(command.GetPayloadPrimitive(), {"param1": 7,
                                                     "sessionId": _SESSION_ID})
    self.assertEqual(command.session_id, _SESSION_ID)

    self.assertEqual(response.name, "GetTitle")
    self.assertEqual(response.GetPayloadPrimitive(), {"param2": 42})
    self.assertEqual(response.session_id, _SESSION_ID)

  def testParserGetNext(self):
    string_buffer = io.StringIO(_WITH_PARAMS)
    command_sequence = client_replay.CommandSequence()
    command_sequence._parser = client_replay._Parser(string_buffer)
    command = command_sequence._parser.GetNext()

    self.assertEqual(command.name, "GetTitle")
    self.assertEqual(command.GetPayloadPrimitive(), {"param1": 7})
    self.assertEqual(command.session_id, _SESSION_ID)

  def testGetNextClientHeaderLine(self):
    string_buffer = io.StringIO(_PAYLOAD_SCRIPT)
    command_sequence = client_replay.CommandSequence()
    command_sequence._parser = client_replay._Parser(string_buffer)
    self.assertEqual(command_sequence._parser._GetNextClientHeaderLine(),
        ("[1531428670.535][INFO]: [b15232d5497ec0d8300a5a1ea56f33ce]"
            " RESPONSE GetTitle {\n"))

  def testGetNextClientHeaderLine_readableTimeLinux(self):
    string_buffer = io.StringIO(_PAYLOAD_READABLE_TIME_LINUX)
    command_sequence = client_replay.CommandSequence()
    command_sequence._parser = client_replay._Parser(string_buffer)
    self.assertEqual(command_sequence._parser._GetNextClientHeaderLine(),
        ("[08-12-2019_15:45:34.824002][INFO]:"
         " [b15232d5497ec0d8300a5a1ea56f33ce] RESPONSE GetTitle {\n"))

  def testGetNextClientHeaderLine_readableTimeWindows(self):
    string_buffer = io.StringIO(_PAYLOAD_READABLE_TIME_WINDOWS)
    command_sequence = client_replay.CommandSequence()
    command_sequence._parser = client_replay._Parser(string_buffer)
    self.assertEqual(command_sequence._parser._GetNextClientHeaderLine(),
        ("[08-12-2019_15:45:34.824][INFO]:"
         " [b15232d5497ec0d8300a5a1ea56f33ce] RESPONSE GetTitle {\n"))

  def testIngestLoggedResponse(self):
    string_buffer = io.StringIO(_RESPONSE_ONLY)
    command_sequence = client_replay.CommandSequence()
    command_sequence._parser = client_replay._Parser(string_buffer)
    response = command_sequence._parser.GetNext()

    self.assertEqual(response.name, "GetTitle")
    self.assertEqual(response.GetPayloadPrimitive(), {"param2": 42})
    self.assertEqual(response.session_id, _SESSION_ID)

  def testIngestRealResponseInitSession(self):
    real_resp = {'value': {
        'sessionId': 'b15232d5497ec0d8300a5a1ea56f33ce',
        'capabilities': {
            'browserVersion': '76.0.3809.100',
            'browserName': 'chrome',
        }
    }}

    command_sequence = client_replay.CommandSequence()
    command_sequence._staged_logged_session_id = _SESSION_ID_ALT
    command_sequence._IngestRealResponse(real_resp)

    self.assertEqual(
        command_sequence._id_map[_SESSION_ID_ALT], _SESSION_ID)
    self.assertEqual(command_sequence._staged_logged_session_id, None)

  def testIngestRealResponseNone(self):
    real_resp = {'value': None}

    command_sequence = client_replay.CommandSequence()
    command_sequence._IngestRealResponse(real_resp)

    self.assertEqual(command_sequence._last_response, None)

  def testIngestRealResponseInt(self):
    real_resp = {'value': 1}

    command_sequence = client_replay.CommandSequence()
    command_sequence._IngestRealResponse(real_resp)

    #last response is not changed by IngestRealResponse,
    #but we want to verify that int response content does not
    #cause error.
    self.assertEqual(command_sequence._last_response, None)

  def testGetPayload_simple(self):
    string_buffer = io.StringIO(_RESPONSE_ONLY)
    header = string_buffer.readline()
    command_sequence = client_replay.CommandSequence()
    command_sequence._parser = client_replay._Parser(string_buffer)
    payload_string = command_sequence._parser._GetPayloadString(header)
    self.assertEqual(payload_string, '{"param2": 42\n}\n')

  def testGetPayload_script(self):
    string_buffer = io.StringIO(_PAYLOAD_SCRIPT)
    header = string_buffer.readline()
    command_sequence = client_replay.CommandSequence()
    command_sequence._parser = client_replay._Parser(string_buffer)
    payload_string = command_sequence._parser._GetPayloadString(header)
    self.assertEqual(payload_string, '{"param2": "function(){func()}"\n}\n')

  def testGetPayload_badscript(self):
    string_buffer = io.StringIO(_BAD_SCRIPT)
    header = string_buffer.readline()
    command_sequence = client_replay.CommandSequence()
    command_sequence._parser = client_replay._Parser(string_buffer)
    payload_string = command_sequence._parser._GetPayloadString(header)
    self.assertEqual(payload_string, '{"param2": "))}\\})}/{)}({(})}"\n}\n')

  def testSubstitutePayloadIds_element(self):
    id_map = {"0.78-1": "0.00-0", "0.78-2": "0.00-1"}
    substituted = {"ELEMENT": "0.78-1"}
    client_replay._ReplaceWindowAndElementIds(substituted, id_map)
    self.assertEqual(substituted, {"ELEMENT": "0.00-0"})

  def testSubstitutePayloadIds_elements(self):
    id_map = {"0.78-1": "0.00-0", "0.78-2": "0.00-1"}
    substituted = [{"ELEMENT": "0.78-1"}, {"ELEMENT": "0.78-2"}]
    client_replay._ReplaceWindowAndElementIds(substituted, id_map)
    self.assertEqual(substituted,
                     [{"ELEMENT": "0.00-0"}, {"ELEMENT": "0.00-1"}])

  def testSubstitutePayloadIds_windows(self):
    id_map = {_WINDOW_ID_2: _WINDOW_ID_1}
    substituted = [_WINDOW_ID_2]
    client_replay._ReplaceWindowAndElementIds(substituted, id_map)
    self.assertEqual(substituted, [_WINDOW_ID_1])

  def testSubstitutePayloadIds_recursion(self):
    id_map = {"0.78-1": "0.00-0", "0.78-2": "0.00-1"}
    substituted = {"args": [{"1": "0.78-1", "2": "0.78-2"}]}
    client_replay._ReplaceWindowAndElementIds(substituted, id_map)
    self.assertEqual(substituted, {"args": [{"1": "0.00-0", "2": "0.00-1"}]})

  def testGetAnyElementids_window(self):
    ids = client_replay._GetAnyElementIds(_WINDOW_IDS)
    self.assertEqual(ids, [_WINDOW_ID_1, _WINDOW_ID_2])

  def testGetAnyElementids_element(self):
    ids = client_replay._GetAnyElementIds(_ELEMENT_ID)
    self.assertEqual(ids, ["0.87-1"])

  def testGetAnyElementids_elements(self):
    ids = client_replay._GetAnyElementIds(_ELEMENT_IDS)
    self.assertEqual(ids, ["0.87-1", "0.87-2"])

  def testGetAnyElementids_string(self):
    ids = client_replay._GetAnyElementIds("true")
    self.assertEqual(ids, None)

  def testGetAnyElementids_invalid(self):
    ids = client_replay._GetAnyElementIds("[ gibberish ]")
    self.assertEqual(ids, None)

  def testCountChar_positive(self):
    self.assertEqual(client_replay._CountChar("{;{;{)]", "{", "}"), 3)

  def testCountChar_onebrace(self):
    self.assertEqual(client_replay._CountChar("{", "{", "}"), 1)

  def testCountChar_nothing(self):
    self.assertEqual(client_replay._CountChar("", "{", "}"), 0)

  def testCountChar_negative(self):
    self.assertEqual(client_replay._CountChar("}){((}{(/)}=}", "{", "}"), -2)

  def testCountChar_quotes(self):
    self.assertEqual(
        client_replay._CountChar('[[][]"[[]]]]]"[[]', "[", "]"), 2)

  def testReplaceUrl_simple(self):
    base_url = "https://base.url.test.com:0000"
    payload = {"url": "https://localhost:12345/"}
    client_replay._ReplaceUrl(payload, base_url)
    self.assertEqual(payload, {"url": "https://base.url.test.com:0000/"})

  def testReplaceUrl_nothing(self):
    payload = {"url": "https://localhost:12345/"}
    client_replay._ReplaceUrl(payload, None)
    self.assertEqual(payload, {"url": "https://localhost:12345/"})

  def testReplaceBinary(self):
    payload_dict = {
        "desiredCapabilities": {
            "goog:chromeOptions": {
                "binary": "/path/to/logged binary/with spaces/"
            },
            "other_things": ["some", "uninteresting", "strings"]
        }
    }
    payload_replaced = {
        "desiredCapabilities": {
            "goog:chromeOptions": {
                "binary": "replacement_binary"
            },
            "other_things": ["some", "uninteresting", "strings"]
        }
    }
    client_replay._ReplaceBinary(payload_dict, "replacement_binary")
    self.assertEqual(payload_replaced, payload_dict)

  def testReplaceBinary_none(self):
    payload_dict = {
        "desiredCapabilities": {
            "goog:chromeOptions": {
                "binary": "/path/to/logged binary/with spaces/"
            },
            "other_things": ["some", "uninteresting", "strings"]
        }
    }
    payload_replaced = {
        "desiredCapabilities": {
            "goog:chromeOptions": {},
            "other_things": ["some", "uninteresting", "strings"]
        }
    }
    client_replay._ReplaceBinary(payload_dict, None)
    self.assertEqual(payload_replaced, payload_dict)

  def testReplaceBinary_nocapabilities(self):
    payload_dict = {"desiredCapabilities": {}}
    payload_replaced = {
        "desiredCapabilities": {
            "goog:chromeOptions": {
                "binary": "replacement_binary"
            }
        }
    }
    client_replay._ReplaceBinary(payload_dict, "replacement_binary")
    self.assertEqual(payload_replaced, payload_dict)

  def testGetCommandName(self):
    self.assertEqual(client_replay._GetCommandName(_PAYLOAD_SCRIPT),
        "GetTitle")

  def testGetSessionId(self):
    self.assertEqual(client_replay._GetSessionId(_PAYLOAD_SCRIPT),
                     _SESSION_ID)

  def testParseCommand_true(self):
    string_buffer = io.StringIO(_COMMAND_ONLY)
    command_sequence = client_replay.CommandSequence()
    command_sequence._parser = client_replay._Parser(string_buffer)
    self.assertTrue(command_sequence._parser.GetNext().IsCommand())

  def testParseCommand_false(self):
    string_buffer = io.StringIO(_RESPONSE_ONLY)
    command_sequence = client_replay.CommandSequence()
    command_sequence._parser = client_replay._Parser(string_buffer)
    self.assertFalse(command_sequence._parser.GetNext().IsCommand())

  def testParseResponse_true(self):
    string_buffer = io.StringIO(_RESPONSE_ONLY)
    command_sequence = client_replay.CommandSequence()
    command_sequence._parser = client_replay._Parser(string_buffer)
    self.assertTrue(command_sequence._parser.GetNext().IsResponse())

  def testParseResponse_false(self):
    string_buffer = io.StringIO(_COMMAND_ONLY)
    command_sequence = client_replay.CommandSequence()
    command_sequence._parser = client_replay._Parser(string_buffer)
    self.assertFalse(command_sequence._parser.GetNext().IsResponse())

  def testHandleGetSessions(self):
    string_buffer = io.StringIO(_MULTI_SESSION)
    command_sequence = client_replay.CommandSequence(string_buffer)
    first_command = command_sequence._parser.GetNext()
    command = command_sequence._HandleGetSessions(
        first_command)
    responses = command_sequence._last_response

    self.assertEqual(command.name, "GetSessions")
    self.assertEqual(command.GetPayloadPrimitive(), {})
    self.assertEqual(command.session_id, _SESSION_ID)

    self.assertEqual(responses.name, "GetSessions")
    self.assertEqual(responses.GetPayloadPrimitive(), [
        {
            "capabilities": {"param2": 42},
            "id": _SESSION_ID
        }, {
            "capabilities": {"param2": 42},
            "id": _SESSION_ID_ALT
        }
    ])
    self.assertEqual(responses.session_id, "")
    self.assertEqual(command_sequence._parser._saved_log_entry.name, "GetTitle")


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      "", "--filter", type="string", default="*",
      help=('Filter for specifying what tests to run, "*" will run all. E.g., '
            "*testReplaceUrl_nothing"))
  parser.add_option(
      "", "--isolated-script-test-output",
      help="JSON output file used by swarming")
  # this option is ignored
  parser.add_option("--isolated-script-test-perf-output", type=str)

  options, _ = parser.parse_args()

  all_tests_suite = unittest.defaultTestLoader.loadTestsFromModule(
      sys.modules[__name__])
  test_suite = unittest_util.FilterTestSuite(all_tests_suite, options.filter)
  result = unittest.TextTestRunner(stream=sys.stdout, verbosity=2)\
          .run(test_suite)

  if options.isolated_script_test_output:
    util.WriteResultToJSONFile([test_suite], [result],
                               options.isolated_script_test_output)

  sys.exit(len(result.failures) + len(result.errors))

if __name__ == "__main__":
  main()
