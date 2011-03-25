#!/usr/bin/python2.4
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This is a simple HTTP server for manually testing exponential
back-off functionality in Chrome.
"""


import BaseHTTPServer
import sys
import urlparse


class RequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  keep_running = True

  def do_GET(self):
    if self.path == '/quitquitquit':
      self.send_response(200)
      self.send_header('Content-Type', 'text/plain')
      self.end_headers()
      self.wfile.write('QUITTING')
      RequestHandler.keep_running = False
      return

    params = urlparse.parse_qs(urlparse.urlparse(self.path).query)

    if not params or not 'code' in params or params['code'][0] == '200':
      self.send_response(200)
      self.send_header('Content-Type', 'text/plain')
      self.end_headers()
      self.wfile.write('OK')
    else:
      self.send_error(int(params['code'][0]))


def main():
  if len(sys.argv) != 2:
    print "Usage: %s PORT" % sys.argv[0]
    sys.exit(1)
  port = int(sys.argv[1])
  print "To stop the server, go to http://localhost:%d/quitquitquit" % port
  httpd = BaseHTTPServer.HTTPServer(('', port), RequestHandler)
  while RequestHandler.keep_running:
    httpd.handle_request()


if __name__ == '__main__':
  main()
