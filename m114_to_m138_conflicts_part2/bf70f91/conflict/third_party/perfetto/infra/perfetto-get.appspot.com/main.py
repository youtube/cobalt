# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

<<<<<<< HEAD
import base64
import requests
import time

from collections import namedtuple
from flask import Flask, make_response, redirect

BASE = 'https://raw.githubusercontent.com/google/perfetto/main/%s'
=======
from google.appengine.api import memcache
from google.appengine.api import urlfetch
import webapp2

import base64

BASE = 'https://android.googlesource.com/platform/external/perfetto.git/' \
       '+/master/%s?format=TEXT'
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

RESOURCES = {
    'tracebox': 'tools/tracebox',
    'traceconv': 'tools/traceconv',
    'trace_processor': 'tools/trace_processor',
}

<<<<<<< HEAD
CACHE_TTL = 3600  # 1 h

CacheEntry = namedtuple('CacheEntry', ['contents', 'expiration'])
cache = {}

app = Flask(__name__)


def DeleteStaleCacheEntries():
  now = time.time()
  for url, entry in list(cache.items()):
    if now > entry.expiration:
      cache.pop(url, None)


@app.route('/')
def root():
  return redirect('https://www.perfetto.dev/', code=301)


@app.route('/<string:resource>')
def fetch_artifact(resource):
  hdrs = {'Content-Type': 'text/plain'}
  resource = resource.lower()
  if resource not in RESOURCES:
    return make_response('Resource "%s" not found' % resource, 404, hdrs)
  url = BASE % RESOURCES[resource]
  DeleteStaleCacheEntries()
  entry = cache.get(url)
  contents = entry.contents if entry is not None else None
  if not contents:
    req = requests.get(url)
    if req.status_code != 200:
      err_str = 'http error %d while fetching %s' % (req.status_code, url)
      return make_response(err_str, req.status_code, hdrs)
    contents = req.text
    cache[url] = CacheEntry(contents, time.time() + CACHE_TTL)
  hdrs = {'Content-Disposition': 'attachment; filename="%s"' % resource}
  return make_response(contents, 200, hdrs)
=======

class RedirectHandler(webapp2.RequestHandler):

  def get(self):
    self.error(301)
    self.response.headers['Location'] = 'https://www.perfetto.dev/'


class GitilesMirrorHandler(webapp2.RequestHandler):

  def get(self, resource):
    self.response.headers['Content-Type'] = 'text/plain'
    resource = resource.lower()
    if resource not in RESOURCES:
      self.error(404)
      self.response.out.write('Resource "%s" not found' % resource)
      return

    url = BASE % RESOURCES[resource]
    contents = memcache.get(url)
    if not contents or self.request.get('reload'):
      result = urlfetch.fetch(url)
      if result.status_code != 200:
        memcache.delete(url)
        self.response.set_status(result.status_code)
        self.response.write(
            'http error %d while fetching %s' % (result.status_code, url))
        return
      contents = base64.b64decode(result.content)
      memcache.set(url, contents, time=3600)  # 1h
    self.response.headers['Content-Disposition'] = \
        'attachment; filename="%s"' % resource
    self.response.write(contents)


app = webapp2.WSGIApplication([
    ('/', RedirectHandler),
    ('/([a-zA-Z0-9_.-]+)', GitilesMirrorHandler),
],
                              debug=True)
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
