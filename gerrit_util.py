# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Utilities for requesting information for a Gerrit server via HTTPS.

https://gerrit-review.googlesource.com/Documentation/rest-api.html
"""

from __future__ import print_function
from __future__ import unicode_literals

import base64
import contextlib
import httplib2
import json
import logging
import netrc
import os
import random
import re
import socket
import stat
import sys
import tempfile
import time
from multiprocessing.pool import ThreadPool

import auth
import gclient_utils
import metrics
import metrics_utils
import subprocess2

from third_party import six
from six.moves import urllib

if sys.version_info.major == 2:
  import cookielib
  from StringIO import StringIO
else:
  import http.cookiejar as cookielib
  from io import StringIO

LOGGER = logging.getLogger()
# With a starting sleep time of 10.0 seconds, x <= [1.8-2.2]x backoff, and five
# total tries, the sleep time between the first and last tries will be ~7 min.
TRY_LIMIT = 5
SLEEP_TIME = 10.0
MAX_BACKOFF = 2.2
MIN_BACKOFF = 1.8

# Controls the transport protocol used to communicate with Gerrit.
# This is parameterized primarily to enable GerritTestCase.
GERRIT_PROTOCOL = 'https'


def time_sleep(seconds):
  # Use this so that it can be mocked in tests without interfering with python
  # system machinery.
  return time.sleep(seconds)


def time_time():
  # Use this so that it can be mocked in tests without interfering with python
  # system machinery.
  return time.time()


def log_retry_and_sleep(seconds, attempt):
  LOGGER.info('Will retry in %d seconds (%d more times)...', seconds,
              TRY_LIMIT - attempt - 1)
  time_sleep(seconds)
  return seconds * random.uniform(MIN_BACKOFF, MAX_BACKOFF)


class GerritError(Exception):
  """Exception class for errors commuicating with the gerrit-on-borg service."""
  def __init__(self, http_status, message, *args, **kwargs):
    super(GerritError, self).__init__(*args, **kwargs)
    self.http_status = http_status
    self.message = '(%d) %s' % (self.http_status, message)

  def __str__(self):
    return self.message


def _QueryString(params, first_param=None):
  """Encodes query parameters in the key:val[+key:val...] format specified here:

  https://gerrit-review.googlesource.com/Documentation/rest-api-changes.html#list-changes
  """
  q = [urllib.parse.quote(first_param)] if first_param else []
  q.extend(['%s:%s' % (key, val.replace(" ", "+")) for key, val in params])
  return '+'.join(q)


class Authenticator(object):
  """Base authenticator class for authenticator implementations to subclass."""

  def get_auth_header(self, host):
    raise NotImplementedError()

  @staticmethod
  def get():
    """Returns: (Authenticator) The identified Authenticator to use.

    Probes the local system and its environment and identifies the
    Authenticator instance to use.
    """
    # LUCI Context takes priority since it's normally present only on bots,
    # which then must use it.
    if LuciContextAuthenticator.is_luci():
      return LuciContextAuthenticator()
    # TODO(crbug.com/1059384): Automatically detect when running on cloudtop,
    # and use CookiesAuthenticator instead.
    if GceAuthenticator.is_gce():
      return GceAuthenticator()
    return CookiesAuthenticator()


class CookiesAuthenticator(Authenticator):
  """Authenticator implementation that uses ".netrc" or ".gitcookies" for token.

  Expected case for developer workstations.
  """

  _EMPTY = object()

  def __init__(self):
    # Credentials will be loaded lazily on first use. This ensures Authenticator
    # get() can always construct an authenticator, even if something is broken.
    # This allows 'creds-check' to proceed to actually checking creds later,
    # rigorously (instead of blowing up with a cryptic error if they are wrong).
    self._netrc = self._EMPTY
    self._gitcookies = self._EMPTY

  @property
  def netrc(self):
    if self._netrc is self._EMPTY:
      self._netrc = self._get_netrc()
    return self._netrc

  @property
  def gitcookies(self):
    if self._gitcookies is self._EMPTY:
      self._gitcookies = self._get_gitcookies()
    return self._gitcookies

  @classmethod
  def get_new_password_url(cls, host):
    assert not host.startswith('http')
    # Assume *.googlesource.com pattern.
    parts = host.split('.')

    # remove -review suffix if present.
    if parts[0].endswith('-review'):
      parts[0] = parts[0][:-len('-review')]

    return 'https://%s/new-password' % ('.'.join(parts))

  @classmethod
  def get_new_password_message(cls, host):
    if host is None:
      return ('Git host for Gerrit upload is unknown. Check your remote '
              'and the branch your branch is tracking. This tool assumes '
              'that you are using a git server at *.googlesource.com.')
    url = cls.get_new_password_url(host)
    return 'You can (re)generate your credentials by visiting %s' % url

  @classmethod
  def get_netrc_path(cls):
    path = '_netrc' if sys.platform.startswith('win') else '.netrc'
    return os.path.expanduser(os.path.join('~', path))

  @classmethod
  def _get_netrc(cls):
    # Buffer the '.netrc' path. Use an empty file if it doesn't exist.
    path = cls.get_netrc_path()
    if not os.path.exists(path):
      return netrc.netrc(os.devnull)

    st = os.stat(path)
    if st.st_mode & (stat.S_IRWXG | stat.S_IRWXO):
      print(
          'WARNING: netrc file %s cannot be used because its file '
          'permissions are insecure.  netrc file permissions should be '
          '600.' % path, file=sys.stderr)
    with open(path) as fd:
      content = fd.read()

    # Load the '.netrc' file. We strip comments from it because processing them
    # can trigger a bug in Windows. See crbug.com/664664.
    content = '\n'.join(l for l in content.splitlines()
                        if l.strip() and not l.strip().startswith('#'))
    with tempdir() as tdir:
      netrc_path = os.path.join(tdir, 'netrc')
      with open(netrc_path, 'w') as fd:
        fd.write(content)
      os.chmod(netrc_path, (stat.S_IRUSR | stat.S_IWUSR))
      return cls._get_netrc_from_path(netrc_path)

  @classmethod
  def _get_netrc_from_path(cls, path):
    try:
      return netrc.netrc(path)
    except IOError:
      print('WARNING: Could not read netrc file %s' % path, file=sys.stderr)
      return netrc.netrc(os.devnull)
    except netrc.NetrcParseError as e:
      print('ERROR: Cannot use netrc file %s due to a parsing error: %s' %
          (path, e), file=sys.stderr)
      return netrc.netrc(os.devnull)

  @classmethod
  def get_gitcookies_path(cls):
    if os.getenv('GIT_COOKIES_PATH'):
      return os.getenv('GIT_COOKIES_PATH')
    try:
      path = subprocess2.check_output(
          ['git', 'config', '--path', 'http.cookiefile'])
      return path.decode('utf-8', 'ignore').strip()
    except subprocess2.CalledProcessError:
      return os.path.expanduser(os.path.join('~', '.gitcookies'))

  @classmethod
  def _get_gitcookies(cls):
    gitcookies = {}
    path = cls.get_gitcookies_path()
    if not os.path.exists(path):
      return gitcookies

    try:
      f = gclient_utils.FileRead(path, 'rb').splitlines()
    except IOError:
      return gitcookies

    for line in f:
      try:
        fields = line.strip().split('\t')
        if line.strip().startswith('#') or len(fields) != 7:
          continue
        domain, xpath, key, value = fields[0], fields[2], fields[5], fields[6]
        if xpath == '/' and key == 'o':
          if value.startswith('git-'):
            login, secret_token = value.split('=', 1)
            gitcookies[domain] = (login, secret_token)
          else:
            gitcookies[domain] = ('', value)
      except (IndexError, ValueError, TypeError) as exc:
        LOGGER.warning(exc)
    return gitcookies

  def _get_auth_for_host(self, host):
    for domain, creds in self.gitcookies.items():
      if cookielib.domain_match(host, domain):
        return (creds[0], None, creds[1])
    return self.netrc.authenticators(host)

  def get_auth_header(self, host):
    a = self._get_auth_for_host(host)
    if a:
      if a[0]:
        secret = base64.b64encode(('%s:%s' % (a[0], a[2])).encode('utf-8'))
        return 'Basic %s' % secret.decode('utf-8')

      return 'Bearer %s' % a[2]
    return None

  def get_auth_email(self, host):
    """Best effort parsing of email to be used for auth for the given host."""
    a = self._get_auth_for_host(host)
    if not a:
      return None
    login = a[0]
    # login typically looks like 'git-xxx.example.com'
    if not login.startswith('git-') or '.' not in login:
      return None
    username, domain = login[len('git-'):].split('.', 1)
    return '%s@%s' % (username, domain)


# Backwards compatibility just in case somebody imports this outside of
# depot_tools.
NetrcAuthenticator = CookiesAuthenticator


class GceAuthenticator(Authenticator):
  """Authenticator implementation that uses GCE metadata service for token.
  """

  _INFO_URL = 'http://metadata.google.internal'
  _ACQUIRE_URL = ('%s/computeMetadata/v1/instance/'
                  'service-accounts/default/token' % _INFO_URL)
  _ACQUIRE_HEADERS = {"Metadata-Flavor": "Google"}

  _cache_is_gce = None
  _token_cache = None
  _token_expiration = None

  @classmethod
  def is_gce(cls):
    if os.getenv('SKIP_GCE_AUTH_FOR_GIT'):
      return False
    if cls._cache_is_gce is None:
      cls._cache_is_gce = cls._test_is_gce()
    return cls._cache_is_gce

  @classmethod
  def _test_is_gce(cls):
    # Based on https://cloud.google.com/compute/docs/metadata#runninggce
    resp, _ = cls._get(cls._INFO_URL)
    if resp is None:
      return False
    return resp.get('metadata-flavor') == 'Google'

  @staticmethod
  def _get(url, **kwargs):
    next_delay_sec = 1.0
    for i in range(TRY_LIMIT):
      p = urllib.parse.urlparse(url)
      if p.scheme not in ('http', 'https'):
        raise RuntimeError(
            "Don't know how to work with protocol '%s'" % protocol)
      try:
        resp, contents = httplib2.Http().request(url, 'GET', **kwargs)
      except (socket.error, httplib2.HttpLib2Error,
              httplib2.socks.ProxyError) as e:
        LOGGER.debug('GET [%s] raised %s', url, e)
        return None, None
      LOGGER.debug('GET [%s] #%d/%d (%d)', url, i+1, TRY_LIMIT, resp.status)
      if resp.status < 500:
        return (resp, contents)

      # Retry server error status codes.
      LOGGER.warn('Encountered server error')
      if TRY_LIMIT - i > 1:
        next_delay_sec = log_retry_and_sleep(next_delay_sec, i)
    return None, None

  @classmethod
  def _get_token_dict(cls):
    # If cached token is valid for at least 25 seconds, return it.
    if cls._token_cache and time_time() + 25 < cls._token_expiration:
      return cls._token_cache

    resp, contents = cls._get(cls._ACQUIRE_URL, headers=cls._ACQUIRE_HEADERS)
    if resp is None or resp.status != 200:
      return None
    cls._token_cache = json.loads(contents)
    cls._token_expiration = cls._token_cache['expires_in'] + time_time()
    return cls._token_cache

  def get_auth_header(self, _host):
    token_dict = self._get_token_dict()
    if not token_dict:
      return None
    return '%(token_type)s %(access_token)s' % token_dict


class LuciContextAuthenticator(Authenticator):
  """Authenticator implementation that uses LUCI_CONTEXT ambient local auth.
  """

  @staticmethod
  def is_luci():
    return auth.has_luci_context_local_auth()

  def __init__(self):
    self._authenticator = auth.Authenticator(
        ' '.join([auth.OAUTH_SCOPE_EMAIL, auth.OAUTH_SCOPE_GERRIT]))

  def get_auth_header(self, _host):
    return 'Bearer %s' % self._authenticator.get_access_token().token


def CreateHttpConn(host,
                   path,
                   reqtype='GET',
                   headers=None,
                   body=None,
                   timeout=None):
  """Opens an HTTPS connection to a Gerrit service, and sends a request."""
  headers = headers or {}
  bare_host = host.partition(':')[0]

  a = Authenticator.get()
  # TODO(crbug.com/1059384): Automatically detect when running on cloudtop.
  if isinstance(a, GceAuthenticator):
    print('If you\'re on a cloudtop instance, export '
          'SKIP_GCE_AUTH_FOR_GIT=1 in your env.')

  a = a.get_auth_header(bare_host)
  if a:
    headers.setdefault('Authorization', a)
  else:
    LOGGER.debug('No authorization found for %s.' % bare_host)

  url = path
  if not url.startswith('/'):
    url = '/' + url
  if 'Authorization' in headers and not url.startswith('/a/'):
    url = '/a%s' % url

  if body:
    body = json.dumps(body, sort_keys=True)
    headers.setdefault('Content-Type', 'application/json')
  if LOGGER.isEnabledFor(logging.DEBUG):
    LOGGER.debug('%s %s://%s%s' % (reqtype, GERRIT_PROTOCOL, host, url))
    for key, val in headers.items():
      if key == 'Authorization':
        val = 'HIDDEN'
      LOGGER.debug('%s: %s' % (key, val))
    if body:
      LOGGER.debug(body)
  conn = httplib2.Http(timeout=timeout)
  # HACK: httplib2.Http has no such attribute; we store req_host here for later
  # use in ReadHttpResponse.
  conn.req_host = host
  conn.req_params = {
      'uri': urllib.parse.urljoin('%s://%s' % (GERRIT_PROTOCOL, host), url),
      'method': reqtype,
      'headers': headers,
      'body': body,
  }
  return conn


def ReadHttpResponse(conn, accept_statuses=frozenset([200])):
  """Reads an HTTP response from a connection into a string buffer.

  Args:
    conn: An Http object created by CreateHttpConn above.
    accept_statuses: Treat any of these statuses as success. Default: [200]
                     Common additions include 204, 400, and 404.
  Returns: A string buffer containing the connection's reply.
  """
  sleep_time = SLEEP_TIME
  for idx in range(TRY_LIMIT):
    before_response = time.time()
    try:
      response, contents = conn.request(**conn.req_params)
    except socket.timeout:
      if idx < TRY_LIMIT - 1:
        sleep_time = log_retry_and_sleep(sleep_time, idx)
        continue
      raise
    contents = contents.decode('utf-8', 'replace')

    response_time = time.time() - before_response
    metrics.collector.add_repeated(
        'http_requests',
        metrics_utils.extract_http_metrics(
            conn.req_params['uri'], conn.req_params['method'], response.status,
            response_time))

    # If response.status is an accepted status,
    # or response.status < 500 then the result is final; break retry loop.
    # If the response is 404/409 it might be because of replication lag,
    # so keep trying anyway. If it is 429, it is generally ok to retry after
    # a backoff.
    if (response.status in accept_statuses
        or response.status < 500 and response.status not in [404, 409, 429]):
      LOGGER.debug('got response %d for %s %s', response.status,
                   conn.req_params['method'], conn.req_params['uri'])
      # If 404 was in accept_statuses, then it's expected that the file might
      # not exist, so don't return the gitiles error page because that's not
      # the "content" that was actually requested.
      if response.status == 404:
        contents = ''
      break

    # A status >=500 is assumed to be a possible transient error; retry.
    http_version = 'HTTP/%s' % ('1.1' if response.version == 11 else '1.0')
    LOGGER.warn('A transient error occurred while querying %s:\n'
                '%s %s %s\n'
                '%s %d %s\n'
                '%s',
                conn.req_host, conn.req_params['method'],
                conn.req_params['uri'],
                http_version, http_version, response.status, response.reason,
                contents)

    if idx < TRY_LIMIT - 1:
      sleep_time = log_retry_and_sleep(sleep_time, idx)
  # end of retries loop

  if response.status in accept_statuses:
    return StringIO(contents)

  if response.status in (302, 401, 403):
    www_authenticate = response.get('www-authenticate')
    if not www_authenticate:
      print('Your Gerrit credentials might be misconfigured.')
    else:
      auth_match = re.search('realm="([^"]+)"', www_authenticate, re.I)
      host = auth_match.group(1) if auth_match else conn.req_host
      print('Authentication failed. Please make sure your .gitcookies '
            'file has credentials for %s.' % host)
    print('Try:\n  git cl creds-check')

  reason = '%s: %s' % (response.reason, contents)
  raise GerritError(response.status, reason)


def ReadHttpJsonResponse(conn, accept_statuses=frozenset([200])):
  """Parses an https response as json."""
  fh = ReadHttpResponse(conn, accept_statuses)
  # The first line of the response should always be: )]}'
  s = fh.readline()
  if s and s.rstrip() != ")]}'":
    raise GerritError(200, 'Unexpected json output: %s' % s)
  s = fh.read()
  if not s:
    return None
  return json.loads(s)


def CallGerritApi(host, path, **kwargs):
  """Helper for calling a Gerrit API that returns a JSON response."""
  conn_kwargs = {}
  conn_kwargs.update(
      (k, kwargs[k]) for k in ['reqtype', 'headers', 'body'] if k in kwargs)
  conn = CreateHttpConn(host, path, **conn_kwargs)
  read_kwargs = {}
  read_kwargs.update((k, kwargs[k]) for k in ['accept_statuses'] if k in kwargs)
  return ReadHttpJsonResponse(conn, **read_kwargs)


def QueryChanges(host, params, first_param=None, limit=None, o_params=None,
                 start=None):
  """
  Queries a gerrit-on-borg server for changes matching query terms.

  Args:
    params: A list of key:value pairs for search parameters, as documented
        here (e.g. ('is', 'owner') for a parameter 'is:owner'):
        https://gerrit-review.googlesource.com/Documentation/user-search.html#search-operators
    first_param: A change identifier
    limit: Maximum number of results to return.
    start: how many changes to skip (starting with the most recent)
    o_params: A list of additional output specifiers, as documented here:
        https://gerrit-review.googlesource.com/Documentation/rest-api-changes.html#list-changes

  Returns:
    A list of json-decoded query results.
  """
  # Note that no attempt is made to escape special characters; YMMV.
  if not params and not first_param:
    raise RuntimeError('QueryChanges requires search parameters')
  path = 'changes/?q=%s' % _QueryString(params, first_param)
  if start:
    path = '%s&start=%s' % (path, start)
  if limit:
    path = '%s&n=%d' % (path, limit)
  if o_params:
    path = '%s&%s' % (path, '&'.join(['o=%s' % p for p in o_params]))
  return ReadHttpJsonResponse(CreateHttpConn(host, path, timeout=30.0))


def GenerateAllChanges(host, params, first_param=None, limit=500,
                       o_params=None, start=None):
  """Queries a gerrit-on-borg server for all the changes matching the query
  terms.

  WARNING: this is unreliable if a change matching the query is modified while
  this function is being called.

  A single query to gerrit-on-borg is limited on the number of results by the
  limit parameter on the request (see QueryChanges) and the server maximum
  limit.

  Args:
    params, first_param: Refer to QueryChanges().
    limit: Maximum number of requested changes per query.
    o_params: Refer to QueryChanges().
    start: Refer to QueryChanges().

  Returns:
    A generator object to the list of returned changes.
  """
  already_returned = set()

  def at_most_once(cls):
    for cl in cls:
      if cl['_number'] not in already_returned:
        already_returned.add(cl['_number'])
        yield cl

  start = start or 0
  cur_start = start
  more_changes = True

  while more_changes:
    # This will fetch changes[start..start+limit] sorted by most recently
    # updated. Since the rank of any change in this list can be changed any time
    # (say user posting comment), subsequent calls may overalp like this:
    #   > initial order ABCDEFGH
    #   query[0..3]  => ABC
    #   > E gets updated. New order: EABCDFGH
    #   query[3..6] => CDF   # C is a dup
    #   query[6..9] => GH    # E is missed.
    page = QueryChanges(host, params, first_param, limit, o_params,
                        cur_start)
    for cl in at_most_once(page):
      yield cl

    more_changes = [cl for cl in page if '_more_changes' in cl]
    if len(more_changes) > 1:
      raise GerritError(
          200,
          'Received %d changes with a _more_changes attribute set but should '
          'receive at most one.' % len(more_changes))
    if more_changes:
      cur_start += len(page)

  # If we paged through, query again the first page which in most circumstances
  # will fetch all changes that were modified while this function was run.
  if start != cur_start:
    page = QueryChanges(host, params, first_param, limit, o_params, start)
    for cl in at_most_once(page):
      yield cl


def MultiQueryChanges(host, params, change_list, limit=None, o_params=None,
                      start=None):
  """Initiate a query composed of multiple sets of query parameters."""
  if not change_list:
    raise RuntimeError(
        "MultiQueryChanges requires a list of change numbers/id's")
  q = ['q=%s' % '+OR+'.join([urllib.parse.quote(str(x)) for x in change_list])]
  if params:
    q.append(_QueryString(params))
  if limit:
    q.append('n=%d' % limit)
  if start:
    q.append('S=%s' % start)
  if o_params:
    q.extend(['o=%s' % p for p in o_params])
  path = 'changes/?%s' % '&'.join(q)
  try:
    result = ReadHttpJsonResponse(CreateHttpConn(host, path))
  except GerritError as e:
    msg = '%s:\n%s' % (e.message, path)
    raise GerritError(e.http_status, msg)
  return result


def GetGerritFetchUrl(host):
  """Given a Gerrit host name returns URL of a Gerrit instance to fetch from."""
  return '%s://%s/' % (GERRIT_PROTOCOL, host)


def GetCodeReviewTbrScore(host, project):
  """Given a Gerrit host name and project, return the Code-Review score for TBR.
  """
  conn = CreateHttpConn(
      host, '/projects/%s' % urllib.parse.quote(project, ''))
  project = ReadHttpJsonResponse(conn)
  if ('labels' not in project
      or 'Code-Review' not in project['labels']
      or 'values' not in project['labels']['Code-Review']):
    return 1
  return max([int(x) for x in project['labels']['Code-Review']['values']])


def GetChangePageUrl(host, change_number):
  """Given a Gerrit host name and change number, returns change page URL."""
  return '%s://%s/#/c/%d/' % (GERRIT_PROTOCOL, host, change_number)


def GetChangeUrl(host, change):
  """Given a Gerrit host name and change ID, returns a URL for the change."""
  return '%s://%s/a/changes/%s' % (GERRIT_PROTOCOL, host, change)


def GetChange(host, change):
  """Queries a Gerrit server for information about a single change."""
  path = 'changes/%s' % change
  return ReadHttpJsonResponse(CreateHttpConn(host, path))


def GetChangeDetail(host, change, o_params=None):
  """Queries a Gerrit server for extended information about a single change."""
  path = 'changes/%s/detail' % change
  if o_params:
    path += '?%s' % '&'.join(['o=%s' % p for p in o_params])
  return ReadHttpJsonResponse(CreateHttpConn(host, path))


def GetChangeCommit(host, change, revision='current'):
  """Query a Gerrit server for a revision associated with a change."""
  path = 'changes/%s/revisions/%s/commit?links' % (change, revision)
  return ReadHttpJsonResponse(CreateHttpConn(host, path))


def GetChangeCurrentRevision(host, change):
  """Get information about the latest revision for a given change."""
  return QueryChanges(host, [], change, o_params=('CURRENT_REVISION',))


def GetChangeRevisions(host, change):
  """Gets information about all revisions associated with a change."""
  return QueryChanges(host, [], change, o_params=('ALL_REVISIONS',))


def GetChangeReview(host, change, revision=None):
  """Gets the current review information for a change."""
  if not revision:
    jmsg = GetChangeRevisions(host, change)
    if not jmsg:
      return None

    if len(jmsg) > 1:
      raise GerritError(200, 'Multiple changes found for ChangeId %s.' % change)
    revision = jmsg[0]['current_revision']
  path = 'changes/%s/revisions/%s/review'
  return ReadHttpJsonResponse(CreateHttpConn(host, path))


def GetChangeComments(host, change):
  """Get the line- and file-level comments on a change."""
  path = 'changes/%s/comments' % change
  return ReadHttpJsonResponse(CreateHttpConn(host, path))


def GetChangeRobotComments(host, change):
  """Gets the line- and file-level robot comments on a change."""
  path = 'changes/%s/robotcomments' % change
  return ReadHttpJsonResponse(CreateHttpConn(host, path))


def GetRelatedChanges(host, change, revision='current'):
  """Gets the related changes for a given change and revision."""
  path = 'changes/%s/revisions/%s/related' % (change, revision)
  return ReadHttpJsonResponse(CreateHttpConn(host, path))


def AbandonChange(host, change, msg=''):
  """Abandons a Gerrit change."""
  path = 'changes/%s/abandon' % change
  body = {'message': msg} if msg else {}
  conn = CreateHttpConn(host, path, reqtype='POST', body=body)
  return ReadHttpJsonResponse(conn)


def MoveChange(host, change, destination_branch):
  """Move a Gerrit change to different destination branch."""
  path = 'changes/%s/move' % change
  body = {'destination_branch': destination_branch,
          'keep_all_votes': True}
  conn = CreateHttpConn(host, path, reqtype='POST', body=body)
  return ReadHttpJsonResponse(conn)



def RestoreChange(host, change, msg=''):
  """Restores a previously abandoned change."""
  path = 'changes/%s/restore' % change
  body = {'message': msg} if msg else {}
  conn = CreateHttpConn(host, path, reqtype='POST', body=body)
  return ReadHttpJsonResponse(conn)


def SubmitChange(host, change):
  """Submits a Gerrit change via Gerrit."""
  path = 'changes/%s/submit' % change
  conn = CreateHttpConn(host, path, reqtype='POST')
  return ReadHttpJsonResponse(conn)


def GetChangesSubmittedTogether(host, change):
  """Get all changes submitted with the given one."""
  path = 'changes/%s/submitted_together?o=NON_VISIBLE_CHANGES' % change
  conn = CreateHttpConn(host, path, reqtype='GET')
  return ReadHttpJsonResponse(conn)


def PublishChangeEdit(host, change, notify=True):
  """Publish a Gerrit change edit."""
  path = 'changes/%s/edit:publish' % change
  body = {'notify': 'ALL' if notify else 'NONE'}
  conn = CreateHttpConn(host, path, reqtype='POST', body=body)
  return ReadHttpJsonResponse(conn, accept_statuses=(204, ))


def ChangeEdit(host, change, path, data):
  """Puts content of a file into a change edit."""
  path = 'changes/%s/edit/%s' % (change, urllib.parse.quote(path, ''))
  body = {
      'binary_content':
      'data:text/plain;base64,%s' %
      base64.b64encode(data.encode('utf-8')).decode('utf-8')
  }
  conn = CreateHttpConn(host, path, reqtype='PUT', body=body)
  return ReadHttpJsonResponse(conn, accept_statuses=(204, 409))


def SetChangeEditMessage(host, change, message):
  """Sets the commit message of a change edit."""
  path = 'changes/%s/edit:message' % change
  body = {'message': message}
  conn = CreateHttpConn(host, path, reqtype='PUT', body=body)
  return ReadHttpJsonResponse(conn, accept_statuses=(204, 409))


def HasPendingChangeEdit(host, change):
  conn = CreateHttpConn(host, 'changes/%s/edit' % change)
  try:
    ReadHttpResponse(conn)
  except GerritError as e:
    # 204 No Content means no pending change.
    if e.http_status == 204:
      return False
    raise
  return True


def DeletePendingChangeEdit(host, change):
  conn = CreateHttpConn(host, 'changes/%s/edit' % change, reqtype='DELETE')
  # On success, Gerrit returns status 204; if the edit was already deleted it
  # returns 404.  Anything else is an error.
  ReadHttpResponse(conn, accept_statuses=[204, 404])


def CherryPick(host, change, destination, revision='current'):
  """Create a cherry-pick commit from the given change, onto the given
  destination.
  """
  path = 'changes/%s/revisions/%s/cherrypick' % (change, revision)
  body = {'destination': destination}
  conn = CreateHttpConn(host, path, reqtype='POST', body=body)
  return ReadHttpJsonResponse(conn)


def GetFileContents(host, change, path):
  """Get the contents of a file with the given path in the given revision.

  Returns:
    A bytes object with the file's contents.
  """
  path = 'changes/%s/revisions/current/files/%s/content' % (
      change, urllib.parse.quote(path, ''))
  conn = CreateHttpConn(host, path, reqtype='GET')
  return base64.b64decode(ReadHttpResponse(conn).read())


def SetCommitMessage(host, change, description, notify='ALL'):
  """Updates a commit message."""
  assert notify in ('ALL', 'NONE')
  path = 'changes/%s/message' % change
  body = {'message': description, 'notify': notify}
  conn = CreateHttpConn(host, path, reqtype='PUT', body=body)
  try:
    ReadHttpResponse(conn, accept_statuses=[200, 204])
  except GerritError as e:
    raise GerritError(
        e.http_status,
        'Received unexpected http status while editing message '
        'in change %s' % change)


def GetCommitIncludedIn(host, project, commit):
  """Retrieves the branches and tags for a given commit.

  https://gerrit-review.googlesource.com/Documentation/rest-api-projects.html#get-included-in

  Returns:
    A JSON object with keys of 'branches' and 'tags'.
  """
  path = 'projects/%s/commits/%s/in' % (urllib.parse.quote(project, ''), commit)
  conn = CreateHttpConn(host, path, reqtype='GET')
  return ReadHttpJsonResponse(conn, accept_statuses=[200])


def IsCodeOwnersEnabledOnHost(host):
  """Check if the code-owners plugin is enabled for the host."""
  path = 'config/server/capabilities'
  capabilities = ReadHttpJsonResponse(CreateHttpConn(host, path))
  return 'code-owners-checkCodeOwner' in capabilities


def IsCodeOwnersEnabledOnRepo(host, repo):
  """Check if the code-owners plugin is enabled for the repo."""
  repo = PercentEncodeForGitRef(repo)
  path = '/projects/%s/code_owners.project_config' % repo
  config = ReadHttpJsonResponse(CreateHttpConn(host, path))
  return not config['status'].get('disabled', False)


def GetOwnersForFile(host,
                     project,
                     branch,
                     path,
                     limit=100,
                     resolve_all_users=True,
                     highest_score_only=False,
                     seed=None,
                     o_params=('DETAILS',)):
  """Gets information about owners attached to a file."""
  path = 'projects/%s/branches/%s/code_owners/%s' % (
      urllib.parse.quote(project, ''),
      urllib.parse.quote(branch, ''),
      urllib.parse.quote(path, ''))
  q = ['resolve-all-users=%s' % json.dumps(resolve_all_users)]
  if highest_score_only:
    q.append('highest-score-only=%s' % json.dumps(highest_score_only))
  if seed:
    q.append('seed=%d' % seed)
  if limit:
    q.append('n=%d' % limit)
  if o_params:
    q.extend(['o=%s' % p for p in o_params])
  if q:
    path = '%s?%s' % (path, '&'.join(q))
  return ReadHttpJsonResponse(CreateHttpConn(host, path))


def GetReviewers(host, change):
  """Gets information about all reviewers attached to a change."""
  path = 'changes/%s/reviewers' % change
  return ReadHttpJsonResponse(CreateHttpConn(host, path))


def GetReview(host, change, revision):
  """Gets review information about a specific revision of a change."""
  path = 'changes/%s/revisions/%s/review' % (change, revision)
  return ReadHttpJsonResponse(CreateHttpConn(host, path))


def AddReviewers(host, change, reviewers=None, ccs=None, notify=True,
                 accept_statuses=frozenset([200, 400, 422])):
  """Add reviewers to a change."""
  if not reviewers and not ccs:
    return None
  if not change:
    return None
  reviewers = frozenset(reviewers or [])
  ccs = frozenset(ccs or [])
  path = 'changes/%s/revisions/current/review' % change

  body = {
    'drafts': 'KEEP',
    'reviewers': [],
    'notify': 'ALL' if notify else 'NONE',
  }
  for r in sorted(reviewers | ccs):
    state = 'REVIEWER' if r in reviewers else 'CC'
    body['reviewers'].append({
     'reviewer': r,
     'state': state,
     'notify': 'NONE',  # We handled `notify` argument above.
    })

  conn = CreateHttpConn(host, path, reqtype='POST', body=body)
  # Gerrit will return 400 if one or more of the requested reviewers are
  # unprocessable. We read the response object to see which were rejected,
  # warn about them, and retry with the remainder.
  resp = ReadHttpJsonResponse(conn, accept_statuses=accept_statuses)

  errored = set()
  for result in resp.get('reviewers', {}).values():
    r = result.get('input')
    state = 'REVIEWER' if r in reviewers else 'CC'
    if result.get('error'):
      errored.add(r)
      LOGGER.warn('Note: "%s" not added as a %s' % (r, state.lower()))
  if errored:
    # Try again, adding only those that didn't fail, and only accepting 200.
    AddReviewers(host, change, reviewers=(reviewers-errored),
                 ccs=(ccs-errored), notify=notify, accept_statuses=[200])


def SetReview(host, change, msg=None, labels=None, notify=None, ready=None):
  """Sets labels and/or adds a message to a code review."""
  if not msg and not labels:
    return
  path = 'changes/%s/revisions/current/review' % change
  body = {'drafts': 'KEEP'}
  if msg:
    body['message'] = msg
  if labels:
    body['labels'] = labels
  if notify is not None:
    body['notify'] = 'ALL' if notify else 'NONE'
  if ready:
    body['ready'] = True
  conn = CreateHttpConn(host, path, reqtype='POST', body=body)
  response = ReadHttpJsonResponse(conn)
  if labels:
    for key, val in labels.items():
      if ('labels' not in response or key not in response['labels'] or
          int(response['labels'][key] != int(val))):
        raise GerritError(200, 'Unable to set "%s" label on change %s.' % (
            key, change))
  return response

def ResetReviewLabels(host, change, label, value='0', message=None,
                      notify=None):
  """Resets the value of a given label for all reviewers on a change."""
  # This is tricky, because we want to work on the "current revision", but
  # there's always the risk that "current revision" will change in between
  # API calls.  So, we check "current revision" at the beginning and end; if
  # it has changed, raise an exception.
  jmsg = GetChangeCurrentRevision(host, change)
  if not jmsg:
    raise GerritError(
        200, 'Could not get review information for change "%s"' % change)
  value = str(value)
  revision = jmsg[0]['current_revision']
  path = 'changes/%s/revisions/%s/review' % (change, revision)
  message = message or (
      '%s label set to %s programmatically.' % (label, value))
  jmsg = GetReview(host, change, revision)
  if not jmsg:
    raise GerritError(200, 'Could not get review information for revision %s '
                   'of change %s' % (revision, change))
  for review in jmsg.get('labels', {}).get(label, {}).get('all', []):
    if str(review.get('value', value)) != value:
      body = {
          'drafts': 'KEEP',
          'message': message,
          'labels': {label: value},
          'on_behalf_of': review['_account_id'],
      }
      if notify:
        body['notify'] = notify
      conn = CreateHttpConn(
          host, path, reqtype='POST', body=body)
      response = ReadHttpJsonResponse(conn)
      if str(response['labels'][label]) != value:
        username = review.get('email', jmsg.get('name', ''))
        raise GerritError(200, 'Unable to set %s label for user "%s"'
                       ' on change %s.' % (label, username, change))
  jmsg = GetChangeCurrentRevision(host, change)
  if not jmsg:
    raise GerritError(
        200, 'Could not get review information for change "%s"' % change)

  if jmsg[0]['current_revision'] != revision:
    raise GerritError(200, 'While resetting labels on change "%s", '
                   'a new patchset was uploaded.' % change)


def CreateChange(host, project, branch='main', subject='', params=()):
  """
  Creates a new change.

  Args:
    params: A list of additional ChangeInput specifiers, as documented here:
        (e.g. ('is_private', 'true') to mark the change private.
        https://gerrit-review.googlesource.com/Documentation/rest-api-changes.html#change-input

  Returns:
    ChangeInfo for the new change.
  """
  path = 'changes/'
  body = {'project': project, 'branch': branch, 'subject': subject}
  body.update(dict(params))
  for key in 'project', 'branch', 'subject':
    if not body[key]:
      raise GerritError(200, '%s is required' % key.title())

  conn = CreateHttpConn(host, path, reqtype='POST', body=body)
  return ReadHttpJsonResponse(conn, accept_statuses=[201])


def CreateGerritBranch(host, project, branch, commit):
  """Creates a new branch from given project and commit

  https://gerrit-review.googlesource.com/Documentation/rest-api-projects.html#create-branch

  Returns:
    A JSON object with 'ref' key.
  """
  path = 'projects/%s/branches/%s' % (project, branch)
  body = {'revision': commit}
  conn = CreateHttpConn(host, path, reqtype='PUT', body=body)
  response = ReadHttpJsonResponse(conn, accept_statuses=[201])
  if response:
    return response
  raise GerritError(200, 'Unable to create gerrit branch')


def CreateGerritTag(host, project, tag, commit):
  """Creates a new tag at the given commit.

  https://gerrit-review.googlesource.com/Documentation/rest-api-projects.html#create-tag

  Returns:
    A JSON object with 'ref' key.
  """
  path = 'projects/%s/tags/%s' % (project, tag)
  body = {'revision': commit}
  conn = CreateHttpConn(host, path, reqtype='PUT', body=body)
  response = ReadHttpJsonResponse(conn, accept_statuses=[201])
  if response:
    return response
  raise GerritError(200, 'Unable to create gerrit tag')


def GetHead(host, project):
  """Retrieves current HEAD of Gerrit project

  https://gerrit-review.googlesource.com/Documentation/rest-api-projects.html#get-head

  Returns:
    A JSON object with 'ref' key.
  """
  path = 'projects/%s/HEAD' % (project)
  conn = CreateHttpConn(host, path, reqtype='GET')
  response = ReadHttpJsonResponse(conn, accept_statuses=[200])
  if response:
    return response
  raise GerritError(200, 'Unable to update gerrit HEAD')


def UpdateHead(host, project, branch):
  """Updates Gerrit HEAD to point to branch

  https://gerrit-review.googlesource.com/Documentation/rest-api-projects.html#set-head

  Returns:
    A JSON object with 'ref' key.
  """
  path = 'projects/%s/HEAD' % (project)
  body = {'ref': branch}
  conn = CreateHttpConn(host, path, reqtype='PUT', body=body)
  response = ReadHttpJsonResponse(conn, accept_statuses=[200])
  if response:
    return response
  raise GerritError(200, 'Unable to update gerrit HEAD')


def GetGerritBranch(host, project, branch):
  """Gets a branch info from given project and branch name.

  See:
  https://gerrit-review.googlesource.com/Documentation/rest-api-projects.html#get-branch

  Returns:
    A JSON object with 'revision' key if the branch exists, otherwise None.
  """
  path = 'projects/%s/branches/%s' % (project, branch)
  conn = CreateHttpConn(host, path, reqtype='GET')
  return ReadHttpJsonResponse(conn, accept_statuses=[200, 404])


def GetProjectHead(host, project):
  conn = CreateHttpConn(host,
                        '/projects/%s/HEAD' % urllib.parse.quote(project, ''))
  return ReadHttpJsonResponse(conn, accept_statuses=[200])


def GetAccountDetails(host, account_id='self'):
  """Returns details of the account.

  If account_id is not given, uses magic value 'self' which corresponds to
  whichever account user is authenticating as.

  Documentation:
  https://gerrit-review.googlesource.com/Documentation/rest-api-accounts.html#get-account

  Returns None if account is not found (i.e., Gerrit returned 404).
  """
  conn = CreateHttpConn(host, '/accounts/%s' % account_id)
  return ReadHttpJsonResponse(conn, accept_statuses=[200, 404])


def ValidAccounts(host, accounts, max_threads=10):
  """Returns a mapping from valid account to its details.

  Invalid accounts, either not existing or without unique match,
  are not present as returned dictionary keys.
  """
  assert not isinstance(accounts, str), type(accounts)
  accounts = list(set(accounts))
  if not accounts:
    return {}

  def get_one(account):
    try:
      return account, GetAccountDetails(host, account)
    except GerritError:
      return None, None

  valid = {}
  with contextlib.closing(ThreadPool(min(max_threads, len(accounts)))) as pool:
    for account, details in pool.map(get_one, accounts):
      if account and details:
        valid[account] = details
  return valid


def PercentEncodeForGitRef(original):
  """Applies percent-encoding for strings sent to Gerrit via git ref metadata.

  The encoding used is based on but stricter than URL encoding (Section 2.1 of
  RFC 3986). The only non-escaped characters are alphanumerics, and 'SPACE'
  (U+0020) can be represented as 'LOW LINE' (U+005F) or 'PLUS SIGN' (U+002B).

  For more information, see the Gerrit docs here:

  https://gerrit-review.googlesource.com/Documentation/user-upload.html#message
  """
  safe = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 '
  encoded = ''.join(c if c in safe else '%%%02X' % ord(c) for c in original)

  # Spaces are not allowed in git refs; gerrit will interpret either '_' or
  # '+' (or '%20') as space. Use '_' since that has been supported the longest.
  return encoded.replace(' ', '_')


@contextlib.contextmanager
def tempdir():
  tdir = None
  try:
    tdir = tempfile.mkdtemp(suffix='gerrit_util')
    yield tdir
  finally:
    if tdir:
      gclient_utils.rmtree(tdir)


def ChangeIdentifier(project, change_number):
  """Returns change identifier "project~number" suitable for |change| arg of
  this module API.

  Such format is allows for more efficient Gerrit routing of HTTP requests,
  comparing to specifying just change_number.
  """
  assert int(change_number)
  return '%s~%s' % (urllib.parse.quote(project, ''), change_number)
