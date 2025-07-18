#!/usr/bin/env vpython3
# coding=utf-8
# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function
from __future__ import unicode_literals


import base64
import httplib2
import json
import os
import socket
import sys
import unittest

if sys.version_info.major == 2:
  from cStringIO import StringIO
  import mock
else:
  from io import StringIO
  from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import gerrit_util
import gclient_utils
import metrics
import metrics_utils
import subprocess2


class CookiesAuthenticatorTest(unittest.TestCase):
  _GITCOOKIES = '\n'.join([
      '\t'.join([
          'chromium.googlesource.com',
          'FALSE',
          '/',
          'TRUE',
          '2147483647',
          'o',
          'git-user.chromium.org=1/chromium-secret',
      ]),
      '\t'.join([
          'chromium-review.googlesource.com',
          'FALSE',
          '/',
          'TRUE',
          '2147483647',
          'o',
          'git-user.chromium.org=1/chromium-secret',
      ]),
      '\t'.join([
          '.example.com',
          'FALSE',
          '/',
          'TRUE',
          '2147483647',
          'o',
          'example-bearer-token',
      ]),
      '\t'.join([
          'another-path.example.com',
          'FALSE',
          '/foo',
          'TRUE',
          '2147483647',
          'o',
          'git-example.com=1/another-path-secret',
      ]),
      '\t'.join([
          'another-key.example.com',
          'FALSE',
          '/',
          'TRUE',
          '2147483647',
          'not-o',
          'git-example.com=1/another-key-secret',
      ]),
      '#' + '\t'.join([
          'chromium-review.googlesource.com',
          'FALSE',
          '/',
          'TRUE',
          '2147483647',
          'o',
          'git-invalid-user.chromium.org=1/invalid-chromium-secret',
      ]),
      'Some unrelated line\t that should not be here',
  ])

  def setUp(self):
    mock.patch('gclient_utils.FileRead', return_value=self._GITCOOKIES).start()
    mock.patch('os.getenv', return_value={}).start()
    mock.patch('os.environ', {'HOME': '$HOME'}).start()
    mock.patch('os.path.exists', return_value=True).start()
    mock.patch(
        'subprocess2.check_output',
        side_effect=[
            subprocess2.CalledProcessError(1, ['cmd'], 'cwd', 'out', 'err')],
    ).start()
    self.addCleanup(mock.patch.stopall)
    self.maxDiff = None

  def testGetNewPasswordUrl(self):
    auth = gerrit_util.CookiesAuthenticator()
    self.assertEqual('https://chromium.googlesource.com/new-password',
                     auth.get_new_password_url('chromium.googlesource.com'))
    self.assertEqual(
        'https://chrome-internal.googlesource.com/new-password',
        auth.get_new_password_url('chrome-internal-review.googlesource.com'))

  def testGetNewPasswordMessage(self):
    auth = gerrit_util.CookiesAuthenticator()
    self.assertIn(
        'https://chromium.googlesource.com/new-password',
        auth.get_new_password_message('chromium-review.googlesource.com'))
    self.assertIn(
        'https://chrome-internal.googlesource.com/new-password',
        auth.get_new_password_message('chrome-internal.googlesource.com'))

  def testGetGitcookiesPath(self):
    self.assertEqual(
        os.path.expanduser(os.path.join('~', '.gitcookies')),
        gerrit_util.CookiesAuthenticator().get_gitcookies_path())

    subprocess2.check_output.side_effect = [b'http.cookiefile']
    self.assertEqual(
        'http.cookiefile',
        gerrit_util.CookiesAuthenticator().get_gitcookies_path())
    subprocess2.check_output.assert_called_with(
        ['git', 'config', '--path', 'http.cookiefile'])

    os.getenv.return_value = 'git-cookies-path'
    self.assertEqual(
        'git-cookies-path',
        gerrit_util.CookiesAuthenticator().get_gitcookies_path())
    os.getenv.assert_called_with('GIT_COOKIES_PATH')

  def testGitcookies(self):
    auth = gerrit_util.CookiesAuthenticator()
    self.assertEqual(auth.gitcookies, {
        'chromium.googlesource.com':
            ('git-user.chromium.org', '1/chromium-secret'),
        'chromium-review.googlesource.com':
            ('git-user.chromium.org', '1/chromium-secret'),
        '.example.com':
            ('', 'example-bearer-token'),
    })

  def testGetAuthHeader(self):
    expected_chromium_header = (
        'Basic Z2l0LXVzZXIuY2hyb21pdW0ub3JnOjEvY2hyb21pdW0tc2VjcmV0')

    auth = gerrit_util.CookiesAuthenticator()
    self.assertEqual(
        expected_chromium_header,
        auth.get_auth_header('chromium.googlesource.com'))
    self.assertEqual(
        expected_chromium_header,
        auth.get_auth_header('chromium-review.googlesource.com'))
    self.assertEqual(
        'Bearer example-bearer-token',
        auth.get_auth_header('some-review.example.com'))

  def testGetAuthEmail(self):
    auth = gerrit_util.CookiesAuthenticator()
    self.assertEqual(
        'user@chromium.org',
        auth.get_auth_email('chromium.googlesource.com'))
    self.assertEqual(
        'user@chromium.org',
        auth.get_auth_email('chromium-review.googlesource.com'))
    self.assertIsNone(auth.get_auth_email('some-review.example.com'))


class GceAuthenticatorTest(unittest.TestCase):
  def setUp(self):
    super(GceAuthenticatorTest, self).setUp()
    mock.patch('httplib2.Http').start()
    mock.patch('os.getenv', return_value=None).start()
    mock.patch('gerrit_util.time_sleep').start()
    mock.patch('gerrit_util.time_time').start()
    self.addCleanup(mock.patch.stopall)
    # GceAuthenticator has class variables that cache the results. Build a new
    # class for every test to avoid inter-test dependencies.
    class GceAuthenticator(gerrit_util.GceAuthenticator):
      pass
    self.GceAuthenticator = GceAuthenticator

  def testIsGce_EnvVarSkip(self, *_mocks):
    os.getenv.return_value = '1'
    self.assertFalse(self.GceAuthenticator.is_gce())
    os.getenv.assert_called_once_with('SKIP_GCE_AUTH_FOR_GIT')

  def testIsGce_Error(self):
    httplib2.Http().request.side_effect = httplib2.HttpLib2Error
    self.assertFalse(self.GceAuthenticator.is_gce())

  def testIsGce_500(self):
    httplib2.Http().request.return_value = (mock.Mock(status=500), None)
    self.assertFalse(self.GceAuthenticator.is_gce())
    last_call = gerrit_util.time_sleep.mock_calls[-1]
    self.assertLessEqual(last_call, mock.call(43.0))

  def testIsGce_FailsThenSucceeds(self):
    response = mock.Mock(status=200)
    response.get.return_value = 'Google'
    httplib2.Http().request.side_effect = [
        (mock.Mock(status=500), None),
        (response, 'who cares'),
    ]
    self.assertTrue(self.GceAuthenticator.is_gce())

  def testIsGce_MetadataFlavorIsNotGoogle(self):
    response = mock.Mock(status=200)
    response.get.return_value = None
    httplib2.Http().request.return_value = (response, 'who cares')
    self.assertFalse(self.GceAuthenticator.is_gce())
    response.get.assert_called_once_with('metadata-flavor')

  def testIsGce_ResultIsCached(self):
    response = mock.Mock(status=200)
    response.get.return_value = 'Google'
    httplib2.Http().request.side_effect = [(response, 'who cares')]
    self.assertTrue(self.GceAuthenticator.is_gce())
    self.assertTrue(self.GceAuthenticator.is_gce())
    httplib2.Http().request.assert_called_once()

  def testGetAuthHeader_Error(self):
    httplib2.Http().request.side_effect = httplib2.HttpLib2Error
    self.assertIsNone(self.GceAuthenticator().get_auth_header(''))

  def testGetAuthHeader_500(self):
    httplib2.Http().request.return_value = (mock.Mock(status=500), None)
    self.assertIsNone(self.GceAuthenticator().get_auth_header(''))

  def testGetAuthHeader_Non200(self):
    httplib2.Http().request.return_value = (mock.Mock(status=403), None)
    self.assertIsNone(self.GceAuthenticator().get_auth_header(''))

  def testGetAuthHeader_OK(self):
    httplib2.Http().request.return_value = (
        mock.Mock(status=200),
        '{"expires_in": 125, "token_type": "TYPE", "access_token": "TOKEN"}')
    gerrit_util.time_time.return_value = 0
    self.assertEqual('TYPE TOKEN', self.GceAuthenticator().get_auth_header(''))

  def testGetAuthHeader_Cache(self):
    httplib2.Http().request.return_value = (
        mock.Mock(status=200),
        '{"expires_in": 125, "token_type": "TYPE", "access_token": "TOKEN"}')
    gerrit_util.time_time.return_value = 0
    self.assertEqual('TYPE TOKEN', self.GceAuthenticator().get_auth_header(''))
    self.assertEqual('TYPE TOKEN', self.GceAuthenticator().get_auth_header(''))
    httplib2.Http().request.assert_called_once()

  def testGetAuthHeader_CacheOld(self):
    httplib2.Http().request.return_value = (
        mock.Mock(status=200),
        '{"expires_in": 125, "token_type": "TYPE", "access_token": "TOKEN"}')
    gerrit_util.time_time.side_effect = [0, 100, 200]
    self.assertEqual('TYPE TOKEN', self.GceAuthenticator().get_auth_header(''))
    self.assertEqual('TYPE TOKEN', self.GceAuthenticator().get_auth_header(''))
    self.assertEqual(2, len(httplib2.Http().request.mock_calls))


class GerritUtilTest(unittest.TestCase):
  def setUp(self):
    super(GerritUtilTest, self).setUp()
    mock.patch('gerrit_util.LOGGER').start()
    mock.patch('gerrit_util.time_sleep').start()
    mock.patch('metrics.collector').start()
    mock.patch(
        'metrics_utils.extract_http_metrics',
        return_value='http_metrics').start()
    self.addCleanup(mock.patch.stopall)

  def testQueryString(self):
    self.assertEqual('', gerrit_util._QueryString([]))
    self.assertEqual(
        'first%20param%2B', gerrit_util._QueryString([], 'first param+'))
    self.assertEqual(
        'key:val+foo:bar',
         gerrit_util._QueryString([('key', 'val'), ('foo', 'bar')]))
    self.assertEqual(
        'first%20param%2B+key:val+foo:bar',
         gerrit_util._QueryString(
            [('key', 'val'), ('foo', 'bar')], 'first param+'))

  @mock.patch('gerrit_util.Authenticator')
  def testCreateHttpConn_Basic(self, mockAuth):
    mockAuth.get().get_auth_header.return_value = None
    conn = gerrit_util.CreateHttpConn('host.example.com', 'foo/bar')
    self.assertEqual('host.example.com', conn.req_host)
    self.assertEqual({
        'uri': 'https://host.example.com/foo/bar',
        'method': 'GET',
        'headers': {},
        'body': None,
    }, conn.req_params)

  @mock.patch('gerrit_util.Authenticator')
  def testCreateHttpConn_Authenticated(self, mockAuth):
    mockAuth.get().get_auth_header.return_value = 'Bearer token'
    conn = gerrit_util.CreateHttpConn(
        'host.example.com', 'foo/bar', headers={'header': 'value'})
    self.assertEqual('host.example.com', conn.req_host)
    self.assertEqual({
        'uri': 'https://host.example.com/a/foo/bar',
        'method': 'GET',
        'headers': {'Authorization': 'Bearer token', 'header': 'value'},
        'body': None,
    }, conn.req_params)

  @mock.patch('gerrit_util.Authenticator')
  def testCreateHttpConn_Body(self, mockAuth):
    mockAuth.get().get_auth_header.return_value = None
    conn = gerrit_util.CreateHttpConn(
        'host.example.com', 'foo/bar', body={'l': [1, 2, 3], 'd': {'k': 'v'}})
    self.assertEqual('host.example.com', conn.req_host)
    self.assertEqual({
        'uri': 'https://host.example.com/foo/bar',
        'method': 'GET',
        'headers': {'Content-Type': 'application/json'},
        'body': '{"d": {"k": "v"}, "l": [1, 2, 3]}',
    }, conn.req_params)

  def testReadHttpResponse_200(self):
    conn = mock.Mock()
    conn.req_params = {'uri': 'uri', 'method': 'method'}
    conn.request.return_value = (mock.Mock(status=200), b'content\xe2\x9c\x94')

    content = gerrit_util.ReadHttpResponse(conn)
    self.assertEqual('content✔', content.getvalue())
    metrics.collector.add_repeated.assert_called_once_with(
        'http_requests', 'http_metrics')

  def testReadHttpResponse_AuthenticationIssue(self):
    for status in (302, 401, 403):
      response = mock.Mock(status=status)
      response.get.return_value = None
      conn = mock.Mock(req_params={'uri': 'uri', 'method': 'method'})
      conn.request.return_value = (response, b'')

      with mock.patch('sys.stdout', StringIO()):
        with self.assertRaises(gerrit_util.GerritError) as cm:
          gerrit_util.ReadHttpResponse(conn)

        self.assertEqual(status, cm.exception.http_status)
        self.assertIn(
            'Your Gerrit credentials might be misconfigured',
            sys.stdout.getvalue())

  def testReadHttpResponse_ClientError(self):
    conn = mock.Mock(req_params={'uri': 'uri', 'method': 'method'})
    conn.request.return_value = (mock.Mock(status=404), b'')

    with self.assertRaises(gerrit_util.GerritError) as cm:
      gerrit_util.ReadHttpResponse(conn)

    self.assertEqual(404, cm.exception.http_status)

  def readHttpResponse_ServerErrorHelper(self, status):
    conn = mock.Mock(req_params={'uri': 'uri', 'method': 'method'})
    conn.request.return_value = (mock.Mock(status=status), b'')

    with self.assertRaises(gerrit_util.GerritError) as cm:
      gerrit_util.ReadHttpResponse(conn)

    self.assertEqual(status, cm.exception.http_status)
    self.assertEqual(gerrit_util.TRY_LIMIT, len(conn.request.mock_calls))
    last_call = gerrit_util.time_sleep.mock_calls[-1]
    self.assertLessEqual(last_call, mock.call(422.0))

  def testReadHttpResponse_ServerError(self):
    self.readHttpResponse_ServerErrorHelper(status=404)
    self.readHttpResponse_ServerErrorHelper(status=409)
    self.readHttpResponse_ServerErrorHelper(status=429)
    self.readHttpResponse_ServerErrorHelper(status=500)

  def testReadHttpResponse_ServerErrorAndSuccess(self):
    conn = mock.Mock(req_params={'uri': 'uri', 'method': 'method'})
    conn.request.side_effect = [
        (mock.Mock(status=500), b''),
        (mock.Mock(status=200), b'content\xe2\x9c\x94'),
    ]

    self.assertEqual('content✔', gerrit_util.ReadHttpResponse(conn).getvalue())
    self.assertEqual(2, len(conn.request.mock_calls))
    gerrit_util.time_sleep.assert_called_once_with(10.0)

  def testReadHttpResponse_TimeoutAndSuccess(self):
    conn = mock.Mock(req_params={'uri': 'uri', 'method': 'method'})
    conn.request.side_effect = [
        socket.timeout('timeout'),
        (mock.Mock(status=200), b'content\xe2\x9c\x94'),
    ]

    self.assertEqual('content✔', gerrit_util.ReadHttpResponse(conn).getvalue())
    self.assertEqual(2, len(conn.request.mock_calls))
    gerrit_util.time_sleep.assert_called_once_with(10.0)

  def testReadHttpResponse_Expected404(self):
    conn = mock.Mock()
    conn.req_params = {'uri': 'uri', 'method': 'method'}
    conn.request.return_value = (mock.Mock(status=404), b'content\xe2\x9c\x94')

    content = gerrit_util.ReadHttpResponse(conn, (404,))
    self.assertEqual('', content.getvalue())

  @mock.patch('gerrit_util.ReadHttpResponse')
  def testReadHttpJsonResponse_NotJSON(self, mockReadHttpResponse):
    mockReadHttpResponse.return_value = StringIO('not json')
    with self.assertRaises(gerrit_util.GerritError) as cm:
      gerrit_util.ReadHttpJsonResponse(None)
    self.assertEqual(cm.exception.http_status, 200)
    self.assertEqual(
        cm.exception.message, '(200) Unexpected json output: not json')

  @mock.patch('gerrit_util.ReadHttpResponse')
  def testReadHttpJsonResponse_EmptyValue(self, mockReadHttpResponse):
    mockReadHttpResponse.return_value = StringIO(')]}\'')
    self.assertIsNone(gerrit_util.ReadHttpJsonResponse(None))

  @mock.patch('gerrit_util.ReadHttpResponse')
  def testReadHttpJsonResponse_JSON(self, mockReadHttpResponse):
    expected_value = {'foo': 'bar', 'baz': [1, '2', 3]}
    mockReadHttpResponse.return_value = StringIO(
        ')]}\'\n' + json.dumps(expected_value))
    self.assertEqual(expected_value, gerrit_util.ReadHttpJsonResponse(None))

  @mock.patch('gerrit_util.CreateHttpConn')
  @mock.patch('gerrit_util.ReadHttpJsonResponse')
  def testQueryChanges(self, mockJsonResponse, mockCreateHttpConn):
    gerrit_util.QueryChanges(
        'host', [('key', 'val'), ('foo', 'bar baz')], 'first param', limit=500,
        o_params=['PARAM_A', 'PARAM_B'], start='start')
    mockCreateHttpConn.assert_called_once_with(
        'host', ('changes/?q=first%20param+key:val+foo:bar+baz'
                 '&start=start'
                 '&n=500'
                 '&o=PARAM_A'
                 '&o=PARAM_B'),
        timeout=30.0)

  def testQueryChanges_NoParams(self):
    self.assertRaises(RuntimeError, gerrit_util.QueryChanges, 'host', [])

  @mock.patch('gerrit_util.QueryChanges')
  def testGenerateAllChanges(self, mockQueryChanges):
    mockQueryChanges.side_effect = [
        # First results page
        [
            {'_number': '4'},
            {'_number': '3'},
            {'_number': '2', '_more_changes': True},
        ],
        # Second results page, there are new changes, so second page includes
        # some results from the first page.
        [
            {'_number': '2'},
            {'_number': '1'},
        ],
        # GenerateAllChanges queries again from the start to get any new
        # changes (5 in this case).
        [
            {'_number': '5'},
            {'_number': '4'},
            {'_number': '3', '_more_changes': True},

        ],
    ]

    changes = list(gerrit_util.GenerateAllChanges('host', 'params'))
    self.assertEqual(
        [
            {'_number': '4'},
            {'_number': '3'},
            {'_number': '2', '_more_changes': True},
            {'_number': '1'},
            {'_number': '5'},
        ],
        changes)
    self.assertEqual(
        [
            mock.call('host', 'params', None, 500, None, 0),
            mock.call('host', 'params', None, 500, None, 3),
            mock.call('host', 'params', None, 500, None, 0),
        ],
        mockQueryChanges.mock_calls)

  @mock.patch('gerrit_util.CreateHttpConn')
  @mock.patch('gerrit_util.ReadHttpJsonResponse')
  def testIsCodeOwnersEnabledOnRepo_Disabled(
      self, mockJsonResponse, mockCreateHttpConn):
    mockJsonResponse.return_value = {'status': {'disabled': True}}
    self.assertFalse(gerrit_util.IsCodeOwnersEnabledOnRepo('host', 'repo'))

  @mock.patch('gerrit_util.CreateHttpConn')
  @mock.patch('gerrit_util.ReadHttpJsonResponse')
  def testIsCodeOwnersEnabledOnRepo_Enabled(
      self, mockJsonResponse, mockCreateHttpConn):
    mockJsonResponse.return_value = {'status': {}}
    self.assertTrue(gerrit_util.IsCodeOwnersEnabledOnRepo('host', 'repo'))

if __name__ == '__main__':
  unittest.main()
