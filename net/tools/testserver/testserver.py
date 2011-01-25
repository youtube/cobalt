#!/usr/bin/python2.4
# Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This is a simple HTTP server used for testing Chrome.

It supports several test URLs, as specified by the handlers in TestPageHandler.
By default, it listens on an ephemeral port and sends the port number back to
the originating process over a pipe. The originating process can specify an
explicit port if necessary.
It can use https if you specify the flag --https=CERT where CERT is the path
to a pem file containing the certificate and private key that should be used.
"""

import asyncore
import base64
import BaseHTTPServer
import cgi
import errno
import optparse
import os
import re
import select
import simplejson
import SocketServer
import socket
import sys
import struct
import time
import urlparse
import warnings

# Ignore deprecation warnings, they make our output more cluttered.
warnings.filterwarnings("ignore", category=DeprecationWarning)

import pyftpdlib.ftpserver
import tlslite
import tlslite.api

try:
  import hashlib
  _new_md5 = hashlib.md5
except ImportError:
  import md5
  _new_md5 = md5.new

if sys.platform == 'win32':
  import msvcrt

SERVER_HTTP = 0
SERVER_FTP = 1
SERVER_SYNC = 2

# Using debug() seems to cause hangs on XP: see http://crbug.com/64515 .
debug_output = sys.stderr
def debug(str):
  debug_output.write(str + "\n")
  debug_output.flush()

class StoppableHTTPServer(BaseHTTPServer.HTTPServer):
  """This is a specialization of of BaseHTTPServer to allow it
  to be exited cleanly (by setting its "stop" member to True)."""

  def serve_forever(self):
    self.stop = False
    self.nonce_time = None
    while not self.stop:
      self.handle_request()
    self.socket.close()

class HTTPSServer(tlslite.api.TLSSocketServerMixIn, StoppableHTTPServer):
  """This is a specialization of StoppableHTTPerver that add https support."""

  def __init__(self, server_address, request_hander_class, cert_path,
               ssl_client_auth, ssl_client_cas, ssl_bulk_ciphers):
    s = open(cert_path).read()
    x509 = tlslite.api.X509()
    x509.parse(s)
    self.cert_chain = tlslite.api.X509CertChain([x509])
    s = open(cert_path).read()
    self.private_key = tlslite.api.parsePEMKey(s, private=True)
    self.ssl_client_auth = ssl_client_auth
    self.ssl_client_cas = []
    for ca_file in ssl_client_cas:
        s = open(ca_file).read()
        x509 = tlslite.api.X509()
        x509.parse(s)
        self.ssl_client_cas.append(x509.subject)
    self.ssl_handshake_settings = tlslite.api.HandshakeSettings()
    if ssl_bulk_ciphers is not None:
      self.ssl_handshake_settings.cipherNames = ssl_bulk_ciphers

    self.session_cache = tlslite.api.SessionCache()
    StoppableHTTPServer.__init__(self, server_address, request_hander_class)

  def handshake(self, tlsConnection):
    """Creates the SSL connection."""
    try:
      tlsConnection.handshakeServer(certChain=self.cert_chain,
                                    privateKey=self.private_key,
                                    sessionCache=self.session_cache,
                                    reqCert=self.ssl_client_auth,
                                    settings=self.ssl_handshake_settings,
                                    reqCAs=self.ssl_client_cas)
      tlsConnection.ignoreAbruptClose = True
      return True
    except tlslite.api.TLSAbruptCloseError:
      # Ignore abrupt close.
      return True
    except tlslite.api.TLSError, error:
      print "Handshake failure:", str(error)
      return False


class SyncHTTPServer(StoppableHTTPServer):
  """An HTTP server that handles sync commands."""

  def __init__(self, server_address, request_handler_class):
    # We import here to avoid pulling in chromiumsync's dependencies
    # unless strictly necessary.
    import chromiumsync
    import xmppserver
    StoppableHTTPServer.__init__(self, server_address, request_handler_class)
    self._sync_handler = chromiumsync.TestServer()
    self._xmpp_socket_map = {}
    self._xmpp_server = xmppserver.XmppServer(
      self._xmpp_socket_map, ('localhost', 0))
    self.xmpp_port = self._xmpp_server.getsockname()[1]

  def HandleCommand(self, query, raw_request):
    return self._sync_handler.HandleCommand(query, raw_request)

  def HandleRequestNoBlock(self):
    """Handles a single request.

    Copied from SocketServer._handle_request_noblock().
    """
    try:
      request, client_address = self.get_request()
    except socket.error:
      return
    if self.verify_request(request, client_address):
      try:
        self.process_request(request, client_address)
      except:
        self.handle_error(request, client_address)
        self.close_request(request)

  def serve_forever(self):
    """This is a merge of asyncore.loop() and SocketServer.serve_forever().
    """

    def HandleXmppSocket(fd, socket_map, handler):
      """Runs the handler for the xmpp connection for fd.

      Adapted from asyncore.read() et al.
      """
      xmpp_connection = socket_map.get(fd)
      # This could happen if a previous handler call caused fd to get
      # removed from socket_map.
      if xmpp_connection is None:
        return
      try:
        handler(xmpp_connection)
      except (asyncore.ExitNow, KeyboardInterrupt, SystemExit):
        raise
      except:
        xmpp_connection.handle_error()

    while True:
      read_fds = [ self.fileno() ]
      write_fds = []
      exceptional_fds = []

      for fd, xmpp_connection in self._xmpp_socket_map.items():
        is_r = xmpp_connection.readable()
        is_w = xmpp_connection.writable()
        if is_r:
          read_fds.append(fd)
        if is_w:
          write_fds.append(fd)
        if is_r or is_w:
          exceptional_fds.append(fd)

      try:
        read_fds, write_fds, exceptional_fds = (
          select.select(read_fds, write_fds, exceptional_fds))
      except select.error, err:
        if err.args[0] != errno.EINTR:
          raise
        else:
          continue

      for fd in read_fds:
        if fd == self.fileno():
          self.HandleRequestNoBlock()
          continue
        HandleXmppSocket(fd, self._xmpp_socket_map,
                         asyncore.dispatcher.handle_read_event)

      for fd in write_fds:
        HandleXmppSocket(fd, self._xmpp_socket_map,
                         asyncore.dispatcher.handle_write_event)

      for fd in exceptional_fds:
        HandleXmppSocket(fd, self._xmpp_socket_map,
                         asyncore.dispatcher.handle_expt_event)


class BasePageHandler(BaseHTTPServer.BaseHTTPRequestHandler):

  def __init__(self, request, client_address, socket_server,
               connect_handlers, get_handlers, post_handlers, put_handlers):
    self._connect_handlers = connect_handlers
    self._get_handlers = get_handlers
    self._post_handlers = post_handlers
    self._put_handlers = put_handlers
    BaseHTTPServer.BaseHTTPRequestHandler.__init__(
      self, request, client_address, socket_server)

  def log_request(self, *args, **kwargs):
    # Disable request logging to declutter test log output.
    pass

  def _ShouldHandleRequest(self, handler_name):
    """Determines if the path can be handled by the handler.

    We consider a handler valid if the path begins with the
    handler name. It can optionally be followed by "?*", "/*".
    """

    pattern = re.compile('%s($|\?|/).*' % handler_name)
    return pattern.match(self.path)

  def do_CONNECT(self):
    for handler in self._connect_handlers:
      if handler():
        return

  def do_GET(self):
    for handler in self._get_handlers:
      if handler():
        return

  def do_POST(self):
    for handler in self._post_handlers:
      if handler():
        return

  def do_PUT(self):
    for handler in self._put_handlers:
      if handler():
        return


class TestPageHandler(BasePageHandler):

  def __init__(self, request, client_address, socket_server):
    connect_handlers = [
      self.RedirectConnectHandler,
      self.ServerAuthConnectHandler,
      self.DefaultConnectResponseHandler]
    get_handlers = [
      self.NoCacheMaxAgeTimeHandler,
      self.NoCacheTimeHandler,
      self.CacheTimeHandler,
      self.CacheExpiresHandler,
      self.CacheProxyRevalidateHandler,
      self.CachePrivateHandler,
      self.CachePublicHandler,
      self.CacheSMaxAgeHandler,
      self.CacheMustRevalidateHandler,
      self.CacheMustRevalidateMaxAgeHandler,
      self.CacheNoStoreHandler,
      self.CacheNoStoreMaxAgeHandler,
      self.CacheNoTransformHandler,
      self.DownloadHandler,
      self.DownloadFinishHandler,
      self.EchoHeader,
      self.EchoHeaderOverride,
      self.EchoAllHandler,
      self.FileHandler,
      self.RealFileWithCommonHeaderHandler,
      self.RealBZ2FileWithCommonHeaderHandler,
      self.SetCookieHandler,
      self.AuthBasicHandler,
      self.AuthDigestHandler,
      self.SlowServerHandler,
      self.ContentTypeHandler,
      self.ServerRedirectHandler,
      self.ClientRedirectHandler,
      self.MultipartHandler,
      self.DefaultResponseHandler]
    post_handlers = [
      self.EchoTitleHandler,
      self.EchoAllHandler,
      self.EchoHandler,
      self.DeviceManagementHandler] + get_handlers
    put_handlers = [
      self.EchoTitleHandler,
      self.EchoAllHandler,
      self.EchoHandler] + get_handlers

    self._mime_types = {
      'crx' : 'application/x-chrome-extension',
      'exe' : 'application/octet-stream',
      'gif': 'image/gif',
      'jpeg' : 'image/jpeg',
      'jpg' : 'image/jpeg',
      'pdf' : 'application/pdf',
      'xml' : 'text/xml'
    }
    self._default_mime_type = 'text/html'

    BasePageHandler.__init__(self, request, client_address, socket_server,
                             connect_handlers, get_handlers, post_handlers,
                             put_handlers)

  def GetMIMETypeFromName(self, file_name):
    """Returns the mime type for the specified file_name. So far it only looks
    at the file extension."""

    (shortname, extension) = os.path.splitext(file_name.split("?")[0])
    if len(extension) == 0:
      # no extension.
      return self._default_mime_type

    # extension starts with a dot, so we need to remove it
    return self._mime_types.get(extension[1:], self._default_mime_type)

  def NoCacheMaxAgeTimeHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and no caching requested."""

    if not self._ShouldHandleRequest("/nocachetime/maxage"):
      return False

    self.send_response(200)
    self.send_header('Cache-Control', 'max-age=0')
    self.send_header('Content-type', 'text/html')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def NoCacheTimeHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and no caching requested."""

    if not self._ShouldHandleRequest("/nocachetime"):
      return False

    self.send_response(200)
    self.send_header('Cache-Control', 'no-cache')
    self.send_header('Content-type', 'text/html')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def CacheTimeHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and allows caching for one minute."""

    if not self._ShouldHandleRequest("/cachetime"):
      return False

    self.send_response(200)
    self.send_header('Cache-Control', 'max-age=60')
    self.send_header('Content-type', 'text/html')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def CacheExpiresHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and set the page to expire on 1 Jan 2099."""

    if not self._ShouldHandleRequest("/cache/expires"):
      return False

    self.send_response(200)
    self.send_header('Expires', 'Thu, 1 Jan 2099 00:00:00 GMT')
    self.send_header('Content-type', 'text/html')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def CacheProxyRevalidateHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and allows caching for 60 seconds"""

    if not self._ShouldHandleRequest("/cache/proxy-revalidate"):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'max-age=60, proxy-revalidate')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def CachePrivateHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and allows caching for 5 seconds."""

    if not self._ShouldHandleRequest("/cache/private"):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'max-age=3, private')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def CachePublicHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and allows caching for 5 seconds."""

    if not self._ShouldHandleRequest("/cache/public"):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'max-age=3, public')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def CacheSMaxAgeHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and does not allow for caching."""

    if not self._ShouldHandleRequest("/cache/s-maxage"):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'public, s-maxage = 60, max-age = 0')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def CacheMustRevalidateHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and does not allow caching."""

    if not self._ShouldHandleRequest("/cache/must-revalidate"):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'must-revalidate')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def CacheMustRevalidateMaxAgeHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and does not allow caching event though max-age of 60
    seconds is specified."""

    if not self._ShouldHandleRequest("/cache/must-revalidate/max-age"):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'max-age=60, must-revalidate')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def CacheNoStoreHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and does not allow the page to be stored."""

    if not self._ShouldHandleRequest("/cache/no-store"):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'no-store')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def CacheNoStoreMaxAgeHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and does not allow the page to be stored even though max-age
    of 60 seconds is specified."""

    if not self._ShouldHandleRequest("/cache/no-store/max-age"):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'max-age=60, no-store')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True


  def CacheNoTransformHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and does not allow the content to transformed during
    user-agent caching"""

    if not self._ShouldHandleRequest("/cache/no-transform"):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'no-transform')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def EchoHeader(self):
    """This handler echoes back the value of a specific request header."""
    """The only difference between this function and the EchoHeaderOverride"""
    """function is in the parameter being passed to the helper function"""
    return self.EchoHeaderHelper("/echoheader")

  def EchoHeaderOverride(self):
    """This handler echoes back the value of a specific request header."""
    """The UrlRequest unit tests also execute for ChromeFrame which uses"""
    """IE to issue HTTP requests using the host network stack."""
    """The Accept and Charset tests which expect the server to echo back"""
    """the corresponding headers fail here as IE returns cached responses"""
    """The EchoHeaderOverride parameter is an easy way to ensure that IE"""
    """treats this request as a new request and does not cache it."""
    return self.EchoHeaderHelper("/echoheaderoverride")

  def EchoHeaderHelper(self, echo_header):
    """This function echoes back the value of the request header passed in."""
    if not self._ShouldHandleRequest(echo_header):
      return False

    query_char = self.path.find('?')
    if query_char != -1:
      header_name = self.path[query_char+1:]

    self.send_response(200)
    self.send_header('Content-type', 'text/plain')
    self.send_header('Cache-control', 'max-age=60000')
    # insert a vary header to properly indicate that the cachability of this
    # request is subject to value of the request header being echoed.
    if len(header_name) > 0:
      self.send_header('Vary', header_name)
    self.end_headers()

    if len(header_name) > 0:
      self.wfile.write(self.headers.getheader(header_name))

    return True

  def ReadRequestBody(self):
    """This function reads the body of the current HTTP request, handling
    both plain and chunked transfer encoded requests."""

    if self.headers.getheader('transfer-encoding') != 'chunked':
      length = int(self.headers.getheader('content-length'))
      return self.rfile.read(length)

    # Read the request body as chunks.
    body = ""
    while True:
      line = self.rfile.readline()
      length = int(line, 16)
      if length == 0:
        self.rfile.readline()
        break
      body += self.rfile.read(length)
      self.rfile.read(2)
    return body

  def EchoHandler(self):
    """This handler just echoes back the payload of the request, for testing
    form submission."""

    if not self._ShouldHandleRequest("/echo"):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    self.wfile.write(self.ReadRequestBody())
    return True

  def EchoTitleHandler(self):
    """This handler is like Echo, but sets the page title to the request."""

    if not self._ShouldHandleRequest("/echotitle"):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    request = self.ReadRequestBody()
    self.wfile.write('<html><head><title>')
    self.wfile.write(request)
    self.wfile.write('</title></head></html>')
    return True

  def EchoAllHandler(self):
    """This handler yields a (more) human-readable page listing information
    about the request header & contents."""

    if not self._ShouldHandleRequest("/echoall"):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    self.wfile.write('<html><head><style>'
      'pre { border: 1px solid black; margin: 5px; padding: 5px }'
      '</style></head><body>'
      '<div style="float: right">'
      '<a href="/echo">back to referring page</a></div>'
      '<h1>Request Body:</h1><pre>')

    if self.command == 'POST' or self.command == 'PUT':
      qs = self.ReadRequestBody()
      params = cgi.parse_qs(qs, keep_blank_values=1)

      for param in params:
        self.wfile.write('%s=%s\n' % (param, params[param][0]))

    self.wfile.write('</pre>')

    self.wfile.write('<h1>Request Headers:</h1><pre>%s</pre>' % self.headers)

    self.wfile.write('</body></html>')
    return True

  def DownloadHandler(self):
    """This handler sends a downloadable file with or without reporting
    the size (6K)."""

    if self.path.startswith("/download-unknown-size"):
      send_length = False
    elif self.path.startswith("/download-known-size"):
      send_length = True
    else:
      return False

    #
    # The test which uses this functionality is attempting to send
    # small chunks of data to the client.  Use a fairly large buffer
    # so that we'll fill chrome's IO buffer enough to force it to
    # actually write the data.
    # See also the comments in the client-side of this test in
    # download_uitest.cc
    #
    size_chunk1 = 35*1024
    size_chunk2 = 10*1024

    self.send_response(200)
    self.send_header('Content-type', 'application/octet-stream')
    self.send_header('Cache-Control', 'max-age=0')
    if send_length:
      self.send_header('Content-Length', size_chunk1 + size_chunk2)
    self.end_headers()

    # First chunk of data:
    self.wfile.write("*" * size_chunk1)
    self.wfile.flush()

    # handle requests until one of them clears this flag.
    self.server.waitForDownload = True
    while self.server.waitForDownload:
      self.server.handle_request()

    # Second chunk of data:
    self.wfile.write("*" * size_chunk2)
    return True

  def DownloadFinishHandler(self):
    """This handler just tells the server to finish the current download."""

    if not self._ShouldHandleRequest("/download-finish"):
      return False

    self.server.waitForDownload = False
    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header('Cache-Control', 'max-age=0')
    self.end_headers()
    return True

  def _ReplaceFileData(self, data, query_parameters):
    """Replaces matching substrings in a file.

    If the 'replace_text' URL query parameter is present, it is expected to be
    of the form old_text:new_text, which indicates that any old_text strings in
    the file are replaced with new_text. Multiple 'replace_text' parameters may
    be specified.

    If the parameters are not present, |data| is returned.
    """
    query_dict = cgi.parse_qs(query_parameters)
    replace_text_values = query_dict.get('replace_text', [])
    for replace_text_value in replace_text_values:
      replace_text_args = replace_text_value.split(':')
      if len(replace_text_args) != 2:
        raise ValueError(
          'replace_text must be of form old_text:new_text. Actual value: %s' %
          replace_text_value)
      old_text_b64, new_text_b64 = replace_text_args
      old_text = base64.urlsafe_b64decode(old_text_b64)
      new_text = base64.urlsafe_b64decode(new_text_b64)
      data = data.replace(old_text, new_text)
    return data

  def FileHandler(self):
    """This handler sends the contents of the requested file.  Wow, it's like
    a real webserver!"""

    prefix = self.server.file_root_url
    if not self.path.startswith(prefix):
      return False

    # Consume a request body if present.
    if self.command == 'POST' or self.command == 'PUT' :
      self.ReadRequestBody()

    _, _, url_path, _, query, _ = urlparse.urlparse(self.path)
    sub_path = url_path[len(prefix):]
    entries = sub_path.split('/')
    file_path = os.path.join(self.server.data_dir, *entries)
    if os.path.isdir(file_path):
      file_path = os.path.join(file_path, 'index.html')

    if not os.path.isfile(file_path):
      print "File not found " + sub_path + " full path:" + file_path
      self.send_error(404)
      return True

    f = open(file_path, "rb")
    data = f.read()
    f.close()

    data = self._ReplaceFileData(data, query)

    # If file.mock-http-headers exists, it contains the headers we
    # should send.  Read them in and parse them.
    headers_path = file_path + '.mock-http-headers'
    if os.path.isfile(headers_path):
      f = open(headers_path, "r")

      # "HTTP/1.1 200 OK"
      response = f.readline()
      status_code = re.findall('HTTP/\d+.\d+ (\d+)', response)[0]
      self.send_response(int(status_code))

      for line in f:
        header_values = re.findall('(\S+):\s*(.*)', line)
        if len(header_values) > 0:
          # "name: value"
          name, value = header_values[0]
          self.send_header(name, value)
      f.close()
    else:
      # Could be more generic once we support mime-type sniffing, but for
      # now we need to set it explicitly.

      range = self.headers.get('Range')
      if range and range.startswith('bytes='):
        # Note this doesn't handle all valid byte range values (i.e. open ended
        # ones), just enough for what we needed so far.
        range = range[6:].split('-')
        start = int(range[0])
        end = int(range[1])

        self.send_response(206)
        content_range = 'bytes ' + str(start) + '-' + str(end) + '/' + \
                        str(len(data))
        self.send_header('Content-Range', content_range)
        data = data[start: end + 1]
      else:
        self.send_response(200)

      self.send_header('Content-type', self.GetMIMETypeFromName(file_path))
      self.send_header('Accept-Ranges', 'bytes')
      self.send_header('Content-Length', len(data))
      self.send_header('ETag', '\'' + file_path + '\'')
    self.end_headers()

    self.wfile.write(data)

    return True

  def RealFileWithCommonHeaderHandler(self):
    """This handler sends the contents of the requested file without the pseudo
    http head!"""

    prefix='/realfiles/'
    if not self.path.startswith(prefix):
      return False

    file = self.path[len(prefix):]
    path = os.path.join(self.server.data_dir, file)

    try:
      f = open(path, "rb")
      data = f.read()
      f.close()

      # just simply set the MIME as octal stream
      self.send_response(200)
      self.send_header('Content-type', 'application/octet-stream')
      self.end_headers()

      self.wfile.write(data)
    except:
      self.send_error(404)

    return True

  def RealBZ2FileWithCommonHeaderHandler(self):
    """This handler sends the bzip2 contents of the requested file with
     corresponding Content-Encoding field in http head!"""

    prefix='/realbz2files/'
    if not self.path.startswith(prefix):
      return False

    parts = self.path.split('?')
    file = parts[0][len(prefix):]
    path = os.path.join(self.server.data_dir, file) + '.bz2'

    if len(parts) > 1:
      options = parts[1]
    else:
      options = ''

    try:
      self.send_response(200)
      accept_encoding = self.headers.get("Accept-Encoding")
      if accept_encoding.find("bzip2") != -1:
        f = open(path, "rb")
        data = f.read()
        f.close()
        self.send_header('Content-Encoding', 'bzip2')
        self.send_header('Content-type', 'application/x-bzip2')
        self.end_headers()
        if options == 'incremental-header':
          self.wfile.write(data[:1])
          self.wfile.flush()
          time.sleep(1.0)
          self.wfile.write(data[1:])
        else:
          self.wfile.write(data)
      else:
        """client do not support bzip2 format, send pseudo content
        """
        self.send_header('Content-type', 'text/html; charset=ISO-8859-1')
        self.end_headers()
        self.wfile.write("you do not support bzip2 encoding")
    except:
      self.send_error(404)

    return True

  def SetCookieHandler(self):
    """This handler just sets a cookie, for testing cookie handling."""

    if not self._ShouldHandleRequest("/set-cookie"):
      return False

    query_char = self.path.find('?')
    if query_char != -1:
      cookie_values = self.path[query_char + 1:].split('&')
    else:
      cookie_values = ("",)
    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    for cookie_value in cookie_values:
      self.send_header('Set-Cookie', '%s' % cookie_value)
    self.end_headers()
    for cookie_value in cookie_values:
      self.wfile.write('%s' % cookie_value)
    return True

  def AuthBasicHandler(self):
    """This handler tests 'Basic' authentication.  It just sends a page with
    title 'user/pass' if you succeed."""

    if not self._ShouldHandleRequest("/auth-basic"):
      return False

    username = userpass = password = b64str = ""
    expected_password = 'secret'
    realm = 'testrealm'
    set_cookie_if_challenged = False

    _, _, url_path, _, query, _ = urlparse.urlparse(self.path)
    query_params = cgi.parse_qs(query, True)
    if 'set-cookie-if-challenged' in query_params:
      set_cookie_if_challenged = True
    if 'password' in query_params:
      expected_password = query_params['password'][0]
    if 'realm' in query_params:
      realm = query_params['realm'][0]

    auth = self.headers.getheader('authorization')
    try:
      if not auth:
        raise Exception('no auth')
      b64str = re.findall(r'Basic (\S+)', auth)[0]
      userpass = base64.b64decode(b64str)
      username, password = re.findall(r'([^:]+):(\S+)', userpass)[0]
      if password != expected_password:
        raise Exception('wrong password')
    except Exception, e:
      # Authentication failed.
      self.send_response(401)
      self.send_header('WWW-Authenticate', 'Basic realm="%s"' % realm)
      self.send_header('Content-type', 'text/html')
      if set_cookie_if_challenged:
        self.send_header('Set-Cookie', 'got_challenged=true')
      self.end_headers()
      self.wfile.write('<html><head>')
      self.wfile.write('<title>Denied: %s</title>' % e)
      self.wfile.write('</head><body>')
      self.wfile.write('auth=%s<p>' % auth)
      self.wfile.write('b64str=%s<p>' % b64str)
      self.wfile.write('username: %s<p>' % username)
      self.wfile.write('userpass: %s<p>' % userpass)
      self.wfile.write('password: %s<p>' % password)
      self.wfile.write('You sent:<br>%s<p>' % self.headers)
      self.wfile.write('</body></html>')
      return True

    # Authentication successful.  (Return a cachable response to allow for
    # testing cached pages that require authentication.)
    if_none_match = self.headers.getheader('if-none-match')
    if if_none_match == "abc":
      self.send_response(304)
      self.end_headers()
    elif url_path.endswith(".gif"):
      # Using chrome/test/data/google/logo.gif as the test image
      test_image_path = ['google', 'logo.gif']
      gif_path = os.path.join(self.server.data_dir, *test_image_path)
      if not os.path.isfile(gif_path):
        self.send_error(404)
        return True

      f = open(gif_path, "rb")
      data = f.read()
      f.close()

      self.send_response(200)
      self.send_header('Content-type', 'image/gif')
      self.send_header('Cache-control', 'max-age=60000')
      self.send_header('Etag', 'abc')
      self.end_headers()
      self.wfile.write(data)
    else:
      self.send_response(200)
      self.send_header('Content-type', 'text/html')
      self.send_header('Cache-control', 'max-age=60000')
      self.send_header('Etag', 'abc')
      self.end_headers()
      self.wfile.write('<html><head>')
      self.wfile.write('<title>%s/%s</title>' % (username, password))
      self.wfile.write('</head><body>')
      self.wfile.write('auth=%s<p>' % auth)
      self.wfile.write('You sent:<br>%s<p>' % self.headers)
      self.wfile.write('</body></html>')

    return True

  def GetNonce(self, force_reset=False):
   """Returns a nonce that's stable per request path for the server's lifetime.

   This is a fake implementation. A real implementation would only use a given
   nonce a single time (hence the name n-once). However, for the purposes of
   unittesting, we don't care about the security of the nonce.

   Args:
     force_reset: Iff set, the nonce will be changed. Useful for testing the
         "stale" response.
   """
   if force_reset or not self.server.nonce_time:
     self.server.nonce_time = time.time()
   return _new_md5('privatekey%s%d' %
                   (self.path, self.server.nonce_time)).hexdigest()

  def AuthDigestHandler(self):
    """This handler tests 'Digest' authentication.

    It just sends a page with title 'user/pass' if you succeed.

    A stale response is sent iff "stale" is present in the request path.
    """
    if not self._ShouldHandleRequest("/auth-digest"):
      return False

    stale = 'stale' in self.path
    nonce = self.GetNonce(force_reset=stale)
    opaque = _new_md5('opaque').hexdigest()
    password = 'secret'
    realm = 'testrealm'

    auth = self.headers.getheader('authorization')
    pairs = {}
    try:
      if not auth:
        raise Exception('no auth')
      if not auth.startswith('Digest'):
        raise Exception('not digest')
      # Pull out all the name="value" pairs as a dictionary.
      pairs = dict(re.findall(r'(\b[^ ,=]+)="?([^",]+)"?', auth))

      # Make sure it's all valid.
      if pairs['nonce'] != nonce:
        raise Exception('wrong nonce')
      if pairs['opaque'] != opaque:
        raise Exception('wrong opaque')

      # Check the 'response' value and make sure it matches our magic hash.
      # See http://www.ietf.org/rfc/rfc2617.txt
      hash_a1 = _new_md5(
          ':'.join([pairs['username'], realm, password])).hexdigest()
      hash_a2 = _new_md5(':'.join([self.command, pairs['uri']])).hexdigest()
      if 'qop' in pairs and 'nc' in pairs and 'cnonce' in pairs:
        response = _new_md5(':'.join([hash_a1, nonce, pairs['nc'],
            pairs['cnonce'], pairs['qop'], hash_a2])).hexdigest()
      else:
        response = _new_md5(':'.join([hash_a1, nonce, hash_a2])).hexdigest()

      if pairs['response'] != response:
        raise Exception('wrong password')
    except Exception, e:
      # Authentication failed.
      self.send_response(401)
      hdr = ('Digest '
             'realm="%s", '
             'domain="/", '
             'qop="auth", '
             'algorithm=MD5, '
             'nonce="%s", '
             'opaque="%s"') % (realm, nonce, opaque)
      if stale:
        hdr += ', stale="TRUE"'
      self.send_header('WWW-Authenticate', hdr)
      self.send_header('Content-type', 'text/html')
      self.end_headers()
      self.wfile.write('<html><head>')
      self.wfile.write('<title>Denied: %s</title>' % e)
      self.wfile.write('</head><body>')
      self.wfile.write('auth=%s<p>' % auth)
      self.wfile.write('pairs=%s<p>' % pairs)
      self.wfile.write('You sent:<br>%s<p>' % self.headers)
      self.wfile.write('We are replying:<br>%s<p>' % hdr)
      self.wfile.write('</body></html>')
      return True

    # Authentication successful.
    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    self.wfile.write('<html><head>')
    self.wfile.write('<title>%s/%s</title>' % (pairs['username'], password))
    self.wfile.write('</head><body>')
    self.wfile.write('auth=%s<p>' % auth)
    self.wfile.write('pairs=%s<p>' % pairs)
    self.wfile.write('</body></html>')

    return True

  def SlowServerHandler(self):
    """Wait for the user suggested time before responding. The syntax is
    /slow?0.5 to wait for half a second."""
    if not self._ShouldHandleRequest("/slow"):
      return False
    query_char = self.path.find('?')
    wait_sec = 1.0
    if query_char >= 0:
      try:
        wait_sec = int(self.path[query_char + 1:])
      except ValueError:
        pass
    time.sleep(wait_sec)
    self.send_response(200)
    self.send_header('Content-type', 'text/plain')
    self.end_headers()
    self.wfile.write("waited %d seconds" % wait_sec)
    return True

  def ContentTypeHandler(self):
    """Returns a string of html with the given content type.  E.g.,
    /contenttype?text/css returns an html file with the Content-Type
    header set to text/css."""
    if not self._ShouldHandleRequest("/contenttype"):
      return False
    query_char = self.path.find('?')
    content_type = self.path[query_char + 1:].strip()
    if not content_type:
      content_type = 'text/html'
    self.send_response(200)
    self.send_header('Content-Type', content_type)
    self.end_headers()
    self.wfile.write("<html>\n<body>\n<p>HTML text</p>\n</body>\n</html>\n");
    return True

  def ServerRedirectHandler(self):
    """Sends a server redirect to the given URL. The syntax is
    '/server-redirect?http://foo.bar/asdf' to redirect to
    'http://foo.bar/asdf'"""

    test_name = "/server-redirect"
    if not self._ShouldHandleRequest(test_name):
      return False

    query_char = self.path.find('?')
    if query_char < 0 or len(self.path) <= query_char + 1:
      self.sendRedirectHelp(test_name)
      return True
    dest = self.path[query_char + 1:]

    self.send_response(301)  # moved permanently
    self.send_header('Location', dest)
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    self.wfile.write('<html><head>')
    self.wfile.write('</head><body>Redirecting to %s</body></html>' % dest)

    return True

  def ClientRedirectHandler(self):
    """Sends a client redirect to the given URL. The syntax is
    '/client-redirect?http://foo.bar/asdf' to redirect to
    'http://foo.bar/asdf'"""

    test_name = "/client-redirect"
    if not self._ShouldHandleRequest(test_name):
      return False

    query_char = self.path.find('?');
    if query_char < 0 or len(self.path) <= query_char + 1:
      self.sendRedirectHelp(test_name)
      return True
    dest = self.path[query_char + 1:]

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    self.wfile.write('<html><head>')
    self.wfile.write('<meta http-equiv="refresh" content="0;url=%s">' % dest)
    self.wfile.write('</head><body>Redirecting to %s</body></html>' % dest)

    return True

  def MultipartHandler(self):
    """Send a multipart response (10 text/html pages)."""
    test_name = "/multipart"
    if not self._ShouldHandleRequest(test_name):
      return False

    num_frames = 10
    bound = '12345'
    self.send_response(200)
    self.send_header('Content-type',
                     'multipart/x-mixed-replace;boundary=' + bound)
    self.end_headers()

    for i in xrange(num_frames):
      self.wfile.write('--' + bound + '\r\n')
      self.wfile.write('Content-type: text/html\r\n\r\n')
      self.wfile.write('<title>page ' + str(i) + '</title>')
      self.wfile.write('page ' + str(i))

    self.wfile.write('--' + bound + '--')
    return True

  def DefaultResponseHandler(self):
    """This is the catch-all response handler for requests that aren't handled
    by one of the special handlers above.
    Note that we specify the content-length as without it the https connection
    is not closed properly (and the browser keeps expecting data)."""

    contents = "Default response given for path: " + self.path
    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.send_header("Content-Length", len(contents))
    self.end_headers()
    self.wfile.write(contents)
    return True

  def RedirectConnectHandler(self):
    """Sends a redirect to the CONNECT request for www.redirect.com. This
    response is not specified by the RFC, so the browser should not follow
    the redirect."""

    if (self.path.find("www.redirect.com") < 0):
      return False

    dest = "http://www.destination.com/foo.js"

    self.send_response(302)  # moved temporarily
    self.send_header('Location', dest)
    self.send_header('Connection', 'close')
    self.end_headers()
    return True

  def ServerAuthConnectHandler(self):
    """Sends a 401 to the CONNECT request for www.server-auth.com. This
    response doesn't make sense because the proxy server cannot request
    server authentication."""

    if (self.path.find("www.server-auth.com") < 0):
      return False

    challenge = 'Basic realm="WallyWorld"'

    self.send_response(401)  # unauthorized
    self.send_header('WWW-Authenticate', challenge)
    self.send_header('Connection', 'close')
    self.end_headers()
    return True

  def DefaultConnectResponseHandler(self):
    """This is the catch-all response handler for CONNECT requests that aren't
    handled by one of the special handlers above.  Real Web servers respond
    with 400 to CONNECT requests."""

    contents = "Your client has issued a malformed or illegal request."
    self.send_response(400)  # bad request
    self.send_header('Content-type', 'text/html')
    self.send_header("Content-Length", len(contents))
    self.end_headers()
    self.wfile.write(contents)
    return True

  def DeviceManagementHandler(self):
    """Delegates to the device management service used for cloud policy."""
    if not self._ShouldHandleRequest("/device_management"):
      return False

    raw_request = self.ReadRequestBody()

    if not self.server._device_management_handler:
      import device_management
      policy_path = os.path.join(self.server.data_dir, 'device_management')
      self.server._device_management_handler = (
          device_management.TestServer(policy_path))

    http_response, raw_reply = (
        self.server._device_management_handler.HandleRequest(self.path,
                                                             self.headers,
                                                             raw_request))
    self.send_response(http_response)
    self.end_headers()
    self.wfile.write(raw_reply)
    return True

  # called by the redirect handling function when there is no parameter
  def sendRedirectHelp(self, redirect_name):
    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    self.wfile.write('<html><body><h1>Error: no redirect destination</h1>')
    self.wfile.write('Use <pre>%s?http://dest...</pre>' % redirect_name)
    self.wfile.write('</body></html>')


class SyncPageHandler(BasePageHandler):
  """Handler for the main HTTP sync server."""

  def __init__(self, request, client_address, sync_http_server):
    get_handlers = [self.ChromiumSyncTimeHandler]
    post_handlers = [self.ChromiumSyncCommandHandler]
    BasePageHandler.__init__(self, request, client_address,
                             sync_http_server, [], get_handlers,
                             post_handlers, [])

  def ChromiumSyncTimeHandler(self):
    """Handle Chromium sync .../time requests.

    The syncer sometimes checks server reachability by examining /time.
    """
    test_name = "/chromiumsync/time"
    if not self._ShouldHandleRequest(test_name):
      return False

    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    return True

  def ChromiumSyncCommandHandler(self):
    """Handle a chromiumsync command arriving via http.

    This covers all sync protocol commands: authentication, getupdates, and
    commit.
    """
    test_name = "/chromiumsync/command"
    if not self._ShouldHandleRequest(test_name):
      return False

    length = int(self.headers.getheader('content-length'))
    raw_request = self.rfile.read(length)

    http_response, raw_reply = self.server.HandleCommand(
        self.path, raw_request)
    self.send_response(http_response)
    self.end_headers()
    self.wfile.write(raw_reply)
    return True


def MakeDataDir():
  if options.data_dir:
    if not os.path.isdir(options.data_dir):
      print 'specified data dir not found: ' + options.data_dir + ' exiting...'
      return None
    my_data_dir = options.data_dir
  else:
    # Create the default path to our data dir, relative to the exe dir.
    my_data_dir = os.path.dirname(sys.argv[0])
    my_data_dir = os.path.join(my_data_dir, "..", "..", "..", "..",
                               "test", "data")

    #TODO(ibrar): Must use Find* funtion defined in google\tools
    #i.e my_data_dir = FindUpward(my_data_dir, "test", "data")

  return my_data_dir

class FileMultiplexer:
  def __init__(self, fd1, fd2) :
    self.__fd1 = fd1
    self.__fd2 = fd2

  def __del__(self) :
    if self.__fd1 != sys.stdout and self.__fd1 != sys.stderr:
      self.__fd1.close()
    if self.__fd2 != sys.stdout and self.__fd2 != sys.stderr:
      self.__fd2.close()

  def write(self, text) :
    self.__fd1.write(text)
    self.__fd2.write(text)

  def flush(self) :
    self.__fd1.flush()
    self.__fd2.flush()

def main(options, args):
  logfile = open('testserver.log', 'w')
  sys.stdout = FileMultiplexer(sys.stdout, logfile)
  sys.stderr = FileMultiplexer(sys.stderr, logfile)

  port = options.port

  server_data = {}

  if options.server_type == SERVER_HTTP:
    if options.cert:
      # let's make sure the cert file exists.
      if not os.path.isfile(options.cert):
        print 'specified server cert file not found: ' + options.cert + \
              ' exiting...'
        return
      for ca_cert in options.ssl_client_ca:
        if not os.path.isfile(ca_cert):
          print 'specified trusted client CA file not found: ' + ca_cert + \
                ' exiting...'
          return
      server = HTTPSServer(('127.0.0.1', port), TestPageHandler, options.cert,
                           options.ssl_client_auth, options.ssl_client_ca,
                           options.ssl_bulk_cipher)
      print 'HTTPS server started on port %d...' % server.server_port
    else:
      server = StoppableHTTPServer(('127.0.0.1', port), TestPageHandler)
      print 'HTTP server started on port %d...' % server.server_port

    server.data_dir = MakeDataDir()
    server.file_root_url = options.file_root_url
    server_data['port'] = server.server_port
    server._device_management_handler = None
  elif options.server_type == SERVER_SYNC:
    server = SyncHTTPServer(('127.0.0.1', port), SyncPageHandler)
    print 'Sync HTTP server started on port %d...' % server.server_port
    print 'Sync XMPP server started on port %d...' % server.xmpp_port
    server_data['port'] = server.server_port
    server_data['xmpp_port'] = server.xmpp_port
  # means FTP Server
  else:
    my_data_dir = MakeDataDir()

    # Instantiate a dummy authorizer for managing 'virtual' users
    authorizer = pyftpdlib.ftpserver.DummyAuthorizer()

    # Define a new user having full r/w permissions and a read-only
    # anonymous user
    authorizer.add_user('chrome', 'chrome', my_data_dir, perm='elradfmw')

    authorizer.add_anonymous(my_data_dir)

    # Instantiate FTP handler class
    ftp_handler = pyftpdlib.ftpserver.FTPHandler
    ftp_handler.authorizer = authorizer

    # Define a customized banner (string returned when client connects)
    ftp_handler.banner = ("pyftpdlib %s based ftpd ready." %
                          pyftpdlib.ftpserver.__ver__)

    # Instantiate FTP server class and listen to 127.0.0.1:port
    address = ('127.0.0.1', port)
    server = pyftpdlib.ftpserver.FTPServer(address, ftp_handler)
    server_data['port'] = server.socket.getsockname()[1]
    print 'FTP server started on port %d...' % server_data['port']

  # Notify the parent that we've started. (BaseServer subclasses
  # bind their sockets on construction.)
  if options.startup_pipe is not None:
    server_data_json = simplejson.dumps(server_data)
    server_data_len = len(server_data_json)
    print 'sending server_data: %s (%d bytes)' % (
      server_data_json, server_data_len)
    if sys.platform == 'win32':
      fd = msvcrt.open_osfhandle(options.startup_pipe, 0)
    else:
      fd = options.startup_pipe
    startup_pipe = os.fdopen(fd, "w")
    # First write the data length as an unsigned 4-byte value.  This
    # is _not_ using network byte ordering since the other end of the
    # pipe is on the same machine.
    startup_pipe.write(struct.pack('=L', server_data_len))
    startup_pipe.write(server_data_json)
    startup_pipe.close()

  try:
    server.serve_forever()
  except KeyboardInterrupt:
    print 'shutting down server'
    server.stop = True

if __name__ == '__main__':
  option_parser = optparse.OptionParser()
  option_parser.add_option("-f", '--ftp', action='store_const',
                           const=SERVER_FTP, default=SERVER_HTTP,
                           dest='server_type',
                           help='start up an FTP server.')
  option_parser.add_option('', '--sync', action='store_const',
                           const=SERVER_SYNC, default=SERVER_HTTP,
                           dest='server_type',
                           help='start up a sync server.')
  option_parser.add_option('', '--port', default='0', type='int',
                           help='Port used by the server. If unspecified, the '
                           'server will listen on an ephemeral port.')
  option_parser.add_option('', '--data-dir', dest='data_dir',
                           help='Directory from which to read the files.')
  option_parser.add_option('', '--https', dest='cert',
                           help='Specify that https should be used, specify '
                           'the path to the cert containing the private key '
                           'the server should use.')
  option_parser.add_option('', '--ssl-client-auth', action='store_true',
                           help='Require SSL client auth on every connection.')
  option_parser.add_option('', '--ssl-client-ca', action='append', default=[],
                           help='Specify that the client certificate request '
                           'should include the CA named in the subject of '
                           'the DER-encoded certificate contained in the '
                           'specified file. This option may appear multiple '
                           'times, indicating multiple CA names should be '
                           'sent in the request.')
  option_parser.add_option('', '--ssl-bulk-cipher', action='append',
                           help='Specify the bulk encryption algorithm(s)'
                           'that will be accepted by the SSL server. Valid '
                           'values are "aes256", "aes128", "3des", "rc4". If '
                           'omitted, all algorithms will be used. This '
                           'option may appear multiple times, indicating '
                           'multiple algorithms should be enabled.');
  option_parser.add_option('', '--file-root-url', default='/files/',
                           help='Specify a root URL for files served.')
  option_parser.add_option('', '--startup-pipe', type='int',
                           dest='startup_pipe',
                           help='File handle of pipe to parent process')
  options, args = option_parser.parse_args()

  sys.exit(main(options, args))
