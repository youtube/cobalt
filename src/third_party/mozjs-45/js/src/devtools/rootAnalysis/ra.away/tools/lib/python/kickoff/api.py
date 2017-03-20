import os
import requests
try:
    import simplejson as json
except ImportError:
    import json

from util.retry import retry

CA_BUNDLE = os.path.join(os.path.dirname(__file__),
                         '../../../../misc/certs/ca-bundle.crt')

import logging
log = logging.getLogger(__name__)


def is_csrf_token_expired(token):
    from datetime import datetime
    expiry = token.split('##')[0]
    # this comparison relies on ship-it running on UTC-based systems
    if expiry <= datetime.utcnow().strftime('%Y%m%d%H%M%S'):
        return True
    return False


class API(object):
    auth = None
    url_template = None

    def __init__(self, auth, api_root, ca_certs=CA_BUNDLE, timeout=60,
                 raise_exceptions=True, retry_attempts=5):
        self.api_root = api_root.rstrip('/')
        self.auth = auth
        self.verify = ca_certs
        self.timeout = timeout
        self.raise_exceptions = raise_exceptions
        self.session = requests.session()
        self.csrf_token = None
        self.retries = retry_attempts

    def request(self, params=None, data=None, method='GET', url_template_vars={}):
        url = self.api_root + self.url_template % url_template_vars
        if method != 'GET' and method != 'HEAD':
            if not self.csrf_token or is_csrf_token_expired(self.csrf_token):
                res = self.session.request(
                    method='HEAD', url=self.api_root + '/csrf_token',
                    timeout=self.timeout, auth=self.auth)
                if self.raise_exceptions:
                    res.raise_for_status()
                self.csrf_token = res.headers['X-CSRF-Token']
            data['csrf_token'] = self.csrf_token
        log.debug('Request to %s' % url)
        log.debug('Data sent: %s' % data)
        try:
            def _req():
                r = self.session.request(
                    method=method, url=url, data=data, timeout=self.timeout,
                    auth=self.auth, params=params)
                if self.raise_exceptions:
                    r.raise_for_status()
                return r

            return retry(_req, sleeptime=5, max_sleeptime=15,
                         retry_exceptions=(requests.HTTPError,
                                           requests.ConnectionError),
                         attempts=self.retries)
        except requests.HTTPError, e:
            log.error('Caught HTTPError: %d %s' % 
                     (e.response.status_code, e.response.content), 
                     exc_info=True)
            raise


class Releases(API):
    url_template = '/releases'

    def getReleases(self, ready=1, complete=0):
        resp = None
        try:
            resp = self.request(params={'ready': ready, 'complete': complete})
            return json.loads(resp.content)
        except:
            log.error('Caught error while retrieving releases.', exc_info=True)
            if resp:
                log.error(resp.content)
                log.error('Response code: %d' % resp.status_code)
            raise


class Release(API):
    url_template = '/releases/%(name)s'

    def getRelease(self, name):
        resp = None
        try:
            resp = self.request(url_template_vars={'name': name})
            return json.loads(resp.content)
        except:
            log.error('Caught error while getting release', exc_info=True)
            if resp:
                log.error(resp.content)
                log.error('Response code: %d' % resp.status_code)
            raise

    def update(self, name, **data):
        url_template_vars = {'name': name}
        return self.request(method='POST', data=data, 
                            url_template_vars=url_template_vars).content


class ReleaseL10n(API):
    url_template = '/releases/%(name)s/l10n'

    def getL10n(self, name):
        return self.request(url_template_vars={'name': name}).content


class Status(API):
    url_template = '/releases/%(name)s/status'

    def update(self, name, data):
        return self.request(method='POST', data=data,
                            url_template_vars={'name': name}).content
