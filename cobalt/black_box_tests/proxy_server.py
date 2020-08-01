# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
"""Starts a HTTP proxy server.

The HTTP proxy server is an improvement over SimpleHTTPServer as it can handle
more complex requests and responses including HTTP methods like POST, PUT,
OPTIONS, and others in addition to dealing with CORS and chunked messages. This
is a modified fork of proxy.py from https://pypi.org/project/proxy.py/ that
includes some bug fixes.
"""

import json
import os
import signal
import subprocess
import tempfile
import time

SRC_DIR = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir)


class ProxyServer(object):

  def __init__(self,
               hostname='0.0.0.0',
               port='8000',
               host_resolve_map=None,
               client_ips=None):
    self.command = [
        'python',
        os.path.join(SRC_DIR, 'third_party', 'proxy_py', 'proxy.py'),
        '--hostname', hostname, '--port', port, '--log-level', 'WARNING'
    ]
    self.host_resolver_path = None

    if host_resolve_map:
      with tempfile.NamedTemporaryFile(delete=False) as hosts:
        json.dump(host_resolve_map, hosts)
      self.host_resolver_path = hosts.name
      self.command += ['--host_resolver', self.host_resolver_path]

    if client_ips:
      self.command += ['--client_ips']
      self.command += client_ips

  def __enter__(self):
    self.proc = subprocess.Popen(self.command)
    time.sleep(1)
    return self

  def __exit__(self, exc_type, exc_value, traceback):
    self.proc.send_signal(signal.SIGINT)
    if self.host_resolver_path:
      os.unlink(self.host_resolver_path)
