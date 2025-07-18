# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Google OAuth2 related functions."""

from __future__ import print_function

import collections
import datetime
import functools
import httplib2
import json
import logging
import os

import subprocess2


# This is what most GAE apps require for authentication.
OAUTH_SCOPE_EMAIL = 'https://www.googleapis.com/auth/userinfo.email'
# Gerrit and Git on *.googlesource.com require this scope.
OAUTH_SCOPE_GERRIT = 'https://www.googleapis.com/auth/gerritcodereview'
# Deprecated. Use OAUTH_SCOPE_EMAIL instead.
OAUTH_SCOPES = OAUTH_SCOPE_EMAIL


# Mockable datetime.datetime.utcnow for testing.
def datetime_now():
  return datetime.datetime.utcnow()


# OAuth access token with its expiration time (UTC datetime or None if unknown).
class AccessToken(collections.namedtuple('AccessToken', [
    'token',
    'expires_at',
  ])):

  def needs_refresh(self):
    """True if this AccessToken should be refreshed."""
    if self.expires_at is not None:
      # Allow 30s of clock skew between client and backend.
      return datetime_now() + datetime.timedelta(seconds=30) >= self.expires_at
    # Token without expiration time never expires.
    return False


class LoginRequiredError(Exception):
  """Interaction with the user is required to authenticate."""

  def __init__(self, scopes=OAUTH_SCOPE_EMAIL):
    msg = (
        'You are not logged in. Please login first by running:\n'
        '  luci-auth login -scopes %s' % scopes)
    super(LoginRequiredError, self).__init__(msg)


def has_luci_context_local_auth():
  """Returns whether LUCI_CONTEXT should be used for ambient authentication."""
  ctx_path = os.environ.get('LUCI_CONTEXT')
  if not ctx_path:
    return False
  try:
    with open(ctx_path) as f:
      loaded = json.load(f)
  except (OSError, IOError, ValueError):
    return False
  return loaded.get('local_auth', {}).get('default_account_id') is not None


class Authenticator(object):
  """Object that knows how to refresh access tokens when needed.

  Args:
    scopes: space separated oauth scopes. Defaults to OAUTH_SCOPE_EMAIL.
  """

  def __init__(self, scopes=OAUTH_SCOPE_EMAIL):
    self._access_token = None
    self._scopes = scopes

  def has_cached_credentials(self):
    """Returns True if credentials can be obtained.

    If returns False, get_access_token() later will probably ask for interactive
    login by raising LoginRequiredError.

    If returns True, get_access_token() won't ask for interactive login.
    """
    return bool(self._get_luci_auth_token())

  def get_access_token(self):
    """Returns AccessToken, refreshing it if necessary.

    Raises:
      LoginRequiredError if user interaction is required.
    """
    if self._access_token and not self._access_token.needs_refresh():
      return self._access_token

    # Token expired or missing. Maybe some other process already updated it,
    # reload from the cache.
    self._access_token = self._get_luci_auth_token()
    if self._access_token and not self._access_token.needs_refresh():
      return self._access_token

    # Nope, still expired. Needs user interaction.
    logging.error('Failed to create access token')
    raise LoginRequiredError(self._scopes)

  def authorize(self, http):
    """Monkey patches authentication logic of httplib2.Http instance.

    The modified http.request method will add authentication headers to each
    request.

    Args:
       http: An instance of httplib2.Http.

    Returns:
       A modified instance of http that was passed in.
    """
    # Adapted from oauth2client.OAuth2Credentials.authorize.
    request_orig = http.request

    @functools.wraps(request_orig)
    def new_request(
        uri, method='GET', body=None, headers=None,
        redirections=httplib2.DEFAULT_MAX_REDIRECTS,
        connection_type=None):
      headers = (headers or {}).copy()
      headers['Authorization'] = 'Bearer %s' % self.get_access_token().token
      return request_orig(
          uri, method, body, headers, redirections, connection_type)

    http.request = new_request
    return http

  ## Private methods.

  def _run_luci_auth_login(self):
    """Run luci-auth login.

    Returns:
      AccessToken with credentials.
    """
    logging.debug('Running luci-auth login')
    subprocess2.check_call(['luci-auth', 'login', '-scopes', self._scopes])
    return self._get_luci_auth_token()

  def _get_luci_auth_token(self):
    logging.debug('Running luci-auth token')
    try:
      out, err = subprocess2.check_call_out(
          ['luci-auth', 'token', '-scopes', self._scopes, '-json-output', '-'],
          stdout=subprocess2.PIPE, stderr=subprocess2.PIPE)
      logging.debug('luci-auth token stderr:\n%s', err)
      token_info = json.loads(out)
      return AccessToken(
          token_info['token'],
          datetime.datetime.utcfromtimestamp(token_info['expiry']))
    except subprocess2.CalledProcessError as e:
      # subprocess2.CalledProcessError.__str__ nicely formats stdout/stderr.
      logging.error('luci-auth token failed: %s', e)
      return None
