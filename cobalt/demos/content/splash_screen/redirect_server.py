#!/usr/bin/env python
# Copyright 2017 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http:#www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Tests the effect that HTTP redirects have on the Cobalt splash screen.

Run this script from the src directory, i.e.

src$ python cobalt/demos/content/splash_screen/redirect_server.py

Let $(hostname) be the local machine's hostname, and let ${port} be
the port printed out by the python script. Then in a separate tab, run
Cobalt and pass the initial URL

http://"$(hostname)":${port}/cobalt/demos/splash_screen/link_splash_screen.html?redirect=yes

(note the HTTP not HTTPS) which will be redirected to

http://"$(hostname)":${port}/cobalt/demos/splash_screen/redirected.html

The expected behavior is that the splash screen served by
redirected.html (beforeunload.html, a blue page with an animated rectangle in
the top left) will be cached separately from the splash screen served
by link_splash_screen.html (the Cobalt logo splash screen).

Specifically, starting with an empty cache, if we navigate to
http://"$(hostname)":${port}...link_splash_screen.html?redirect=yes ,
a new splash screen should be cached to the cache directory
(beforeunload.html). If we then navigate to
http://"$(hostname)":${port}...link_splash_screen.html (without the
query parameter), we should see no splash screen displayed at first
run, and cobalt_splash_screen.html should be cached to the cache
directory. On a second navigation to
http://"$(hostname)":${port}...link_splash_screen.html (without the
query parameter), we should see the cached Cobalt logo splash screen
displayed.

In short, although redirected.html serves a splash screen it should
not be seen when we redirect there via
link_splash_screen.html?redirect=yes and certainly not when we
navigate to link_splash_screen.html without query parameters. The
splash screen cached by redirected.html should only be seen when we
navigate directly to redirected.html.

It is OK if navigating to link_splash_screen.html first, then shows
the Cobalt logo splash screen on the next navigation to
link_splash_screen.html?redirect=yes.
"""
import SimpleHTTPServer
import SocketServer


class Handler(SimpleHTTPServer.SimpleHTTPRequestHandler):

  def do_GET(self):
    path = self.path
    print 'path = ' + path
    redirect_from = 'link_splash_screen.html?redirect=yes'
    redirect_to = 'redirected.html'
    if redirect_from in path:
      self.send_response(302)
      self.send_header('Location', path.replace(redirect_from, redirect_to))
      self.end_headers()
    else:
      return SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)


port = 8000
while True:
  try:
    handler = SocketServer.TCPServer(('', port), Handler)
    print 'seving port ' + str(port)
    handler.serve_forever()
  except SocketServer.socket.error as exc:
    port += 1
    print 'trying port ' + str(port)
