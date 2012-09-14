#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This is a simple HTTP/FTP/SYNC/TCP/UDP/ server used for testing Chrome.

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
import httplib
import minica
import optparse
import os
import random
import re
import select
import socket
import SocketServer
import struct
import sys
import threading
import time
import urllib
import urlparse
import warnings
import zlib

# Ignore deprecation warnings, they make our output more cluttered.
warnings.filterwarnings("ignore", category=DeprecationWarning)

import echo_message
import pyftpdlib.ftpserver
import tlslite
import tlslite.api

try:
  import hashlib
  _new_md5 = hashlib.md5
except ImportError:
  import md5
  _new_md5 = md5.new

try:
  import json
except ImportError:
  import simplejson as json

if sys.platform == 'win32':
  import msvcrt

SERVER_HTTP = 0
SERVER_FTP = 1
SERVER_SYNC = 2
SERVER_TCP_ECHO = 3
SERVER_UDP_ECHO = 4
SERVER_BASIC_AUTH_PROXY = 5

# Using debug() seems to cause hangs on XP: see http://crbug.com/64515 .
debug_output = sys.stderr
def debug(str):
  debug_output.write(str + "\n")
  debug_output.flush()

class RecordingSSLSessionCache(object):
  """RecordingSSLSessionCache acts as a TLS session cache and maintains a log of
  lookups and inserts in order to test session cache behaviours."""

  def __init__(self):
    self.log = []

  def __getitem__(self, sessionID):
    self.log.append(('lookup', sessionID))
    raise KeyError()

  def __setitem__(self, sessionID, session):
    self.log.append(('insert', sessionID))


class ClientRestrictingServerMixIn:
  """Implements verify_request to limit connections to our configured IP
  address."""

  def verify_request(self, request, client_address):
    return client_address[0] == self.server_address[0]


class StoppableHTTPServer(BaseHTTPServer.HTTPServer):
  """This is a specialization of BaseHTTPServer to allow it
  to be exited cleanly (by setting its "stop" member to True)."""

  def serve_forever(self):
    self.stop = False
    self.nonce_time = None
    while not self.stop:
      self.handle_request()
    self.socket.close()


class HTTPServer(ClientRestrictingServerMixIn, StoppableHTTPServer):
  """This is a specialization of StoppableHTTPServer that adds client
  verification."""

  pass

class OCSPServer(ClientRestrictingServerMixIn, BaseHTTPServer.HTTPServer):
  """This is a specialization of HTTPServer that serves an
  OCSP response"""

  def serve_forever_on_thread(self):
    self.thread = threading.Thread(target = self.serve_forever,
                                   name = "OCSPServerThread")
    self.thread.start()

  def stop_serving(self):
    self.shutdown()
    self.thread.join()

class HTTPSServer(tlslite.api.TLSSocketServerMixIn,
                  ClientRestrictingServerMixIn,
                  StoppableHTTPServer):
  """This is a specialization of StoppableHTTPServer that add https support and
  client verification."""

  def __init__(self, server_address, request_hander_class, pem_cert_and_key,
               ssl_client_auth, ssl_client_cas, ssl_bulk_ciphers,
               record_resume_info, tls_intolerant):
    self.cert_chain = tlslite.api.X509CertChain().parseChain(pem_cert_and_key)
    self.private_key = tlslite.api.parsePEMKey(pem_cert_and_key, private=True)
    self.ssl_client_auth = ssl_client_auth
    self.ssl_client_cas = []
    self.tls_intolerant = tls_intolerant

    for ca_file in ssl_client_cas:
        s = open(ca_file).read()
        x509 = tlslite.api.X509()
        x509.parse(s)
        self.ssl_client_cas.append(x509.subject)
    self.ssl_handshake_settings = tlslite.api.HandshakeSettings()
    if ssl_bulk_ciphers is not None:
      self.ssl_handshake_settings.cipherNames = ssl_bulk_ciphers

    if record_resume_info:
      # If record_resume_info is true then we'll replace the session cache with
      # an object that records the lookups and inserts that it sees.
      self.session_cache = RecordingSSLSessionCache()
    else:
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
                                    reqCAs=self.ssl_client_cas,
                                    tlsIntolerant=self.tls_intolerant)
      tlsConnection.ignoreAbruptClose = True
      return True
    except tlslite.api.TLSAbruptCloseError:
      # Ignore abrupt close.
      return True
    except tlslite.api.TLSError, error:
      print "Handshake failure:", str(error)
      return False


class SyncHTTPServer(ClientRestrictingServerMixIn, StoppableHTTPServer):
  """An HTTP server that handles sync commands."""

  def __init__(self, server_address, xmpp_port, request_handler_class):
    # We import here to avoid pulling in chromiumsync's dependencies
    # unless strictly necessary.
    import chromiumsync
    import xmppserver
    StoppableHTTPServer.__init__(self, server_address, request_handler_class)
    self._sync_handler = chromiumsync.TestServer()
    self._xmpp_socket_map = {}
    self._xmpp_server = xmppserver.XmppServer(
      self._xmpp_socket_map, ('localhost', xmpp_port))
    self.xmpp_port = self._xmpp_server.getsockname()[1]
    self.authenticated = True

  def GetXmppServer(self):
    return self._xmpp_server

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

  def SetAuthenticated(self, auth_valid):
    self.authenticated = auth_valid

  def GetAuthenticated(self):
    return self.authenticated

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


class FTPServer(ClientRestrictingServerMixIn, pyftpdlib.ftpserver.FTPServer):
  """This is a specialization of FTPServer that adds client verification."""

  pass


class TCPEchoServer(ClientRestrictingServerMixIn, SocketServer.TCPServer):
  """A TCP echo server that echoes back what it has received."""

  def server_bind(self):
    """Override server_bind to store the server name."""
    SocketServer.TCPServer.server_bind(self)
    host, port = self.socket.getsockname()[:2]
    self.server_name = socket.getfqdn(host)
    self.server_port = port

  def serve_forever(self):
    self.stop = False
    self.nonce_time = None
    while not self.stop:
      self.handle_request()
    self.socket.close()


class UDPEchoServer(ClientRestrictingServerMixIn, SocketServer.UDPServer):
  """A UDP echo server that echoes back what it has received."""

  def server_bind(self):
    """Override server_bind to store the server name."""
    SocketServer.UDPServer.server_bind(self)
    host, port = self.socket.getsockname()[:2]
    self.server_name = socket.getfqdn(host)
    self.server_port = port

  def serve_forever(self):
    self.stop = False
    self.nonce_time = None
    while not self.stop:
      self.handle_request()
    self.socket.close()


class BasePageHandler(BaseHTTPServer.BaseHTTPRequestHandler):

  def __init__(self, request, client_address, socket_server,
               connect_handlers, get_handlers, head_handlers, post_handlers,
               put_handlers):
    self._connect_handlers = connect_handlers
    self._get_handlers = get_handlers
    self._head_handlers = head_handlers
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

  def do_HEAD(self):
    for handler in self._head_handlers:
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
      self.EchoHeaderCache,
      self.EchoAllHandler,
      self.ZipFileHandler,
      self.GDataAuthHandler,
      self.GDataDocumentsFeedQueryHandler,
      self.FileHandler,
      self.SetCookieHandler,
      self.SetManyCookiesHandler,
      self.ExpectAndSetCookieHandler,
      self.SetHeaderHandler,
      self.AuthBasicHandler,
      self.AuthDigestHandler,
      self.SlowServerHandler,
      self.ChunkedServerHandler,
      self.ContentTypeHandler,
      self.NoContentHandler,
      self.ServerRedirectHandler,
      self.ClientRedirectHandler,
      self.MultipartHandler,
      self.MultipartSlowHandler,
      self.GetSSLSessionCacheHandler,
      self.CloseSocketHandler,
      self.DefaultResponseHandler]
    post_handlers = [
      self.EchoTitleHandler,
      self.EchoHandler,
      self.DeviceManagementHandler,
      self.PostOnlyFileHandler] + get_handlers
    put_handlers = [
      self.EchoTitleHandler,
      self.EchoHandler] + get_handlers
    head_handlers = [
      self.FileHandler,
      self.DefaultResponseHandler]

    self._mime_types = {
      'crx' : 'application/x-chrome-extension',
      'exe' : 'application/octet-stream',
      'gif': 'image/gif',
      'jpeg' : 'image/jpeg',
      'jpg' : 'image/jpeg',
      'json': 'application/json',
      'pdf' : 'application/pdf',
      'xml' : 'text/xml'
    }
    self._default_mime_type = 'text/html'

    BasePageHandler.__init__(self, request, client_address, socket_server,
                             connect_handlers, get_handlers, head_handlers,
                             post_handlers, put_handlers)

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
    self.send_header('Content-Type', 'text/html')
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
    self.send_header('Content-Type', 'text/html')
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
    self.send_header('Content-Type', 'text/html')
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
    self.send_header('Content-Type', 'text/html')
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
    self.send_header('Content-Type', 'text/html')
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
    self.send_header('Content-Type', 'text/html')
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
    self.send_header('Content-Type', 'text/html')
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
    self.send_header('Content-Type', 'text/html')
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
    self.send_header('Content-Type', 'text/html')
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
    self.send_header('Content-Type', 'text/html')
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
    self.send_header('Content-Type', 'text/html')
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
    self.send_header('Content-Type', 'text/html')
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
    self.send_header('Content-Type', 'text/html')
    self.send_header('Cache-Control', 'no-transform')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def EchoHeader(self):
    """This handler echoes back the value of a specific request header."""
    return self.EchoHeaderHelper("/echoheader")

    """This function echoes back the value of a specific request header"""
    """while allowing caching for 16 hours."""
  def EchoHeaderCache(self):
    return self.EchoHeaderHelper("/echoheadercache")

  def EchoHeaderHelper(self, echo_header):
    """This function echoes back the value of the request header passed in."""
    if not self._ShouldHandleRequest(echo_header):
      return False

    query_char = self.path.find('?')
    if query_char != -1:
      header_name = self.path[query_char+1:]

    self.send_response(200)
    self.send_header('Content-Type', 'text/plain')
    if echo_header == '/echoheadercache':
      self.send_header('Cache-control', 'max-age=60000')
    else:
      self.send_header('Cache-control', 'no-cache')
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
    self.send_header('Content-Type', 'text/html')
    self.end_headers()
    self.wfile.write(self.ReadRequestBody())
    return True

  def EchoTitleHandler(self):
    """This handler is like Echo, but sets the page title to the request."""

    if not self._ShouldHandleRequest("/echotitle"):
      return False

    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
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
    self.send_header('Content-Type', 'text/html')
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
    self.send_header('Content-Type', 'application/octet-stream')
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
    self.send_header('Content-Type', 'text/html')
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

  def ZipFileHandler(self):
    """This handler sends the contents of the requested file in compressed form.
    Can pass in a parameter that specifies that the content length be
    C - the compressed size (OK),
    U - the uncompressed size (Non-standard, but handled),
    S - less than compressed (OK because we keep going),
    M - larger than compressed but less than uncompressed (an error),
    L - larger than uncompressed (an error)
    Example: compressedfiles/Picture_1.doc?C
    """

    prefix = "/compressedfiles/"
    if not self.path.startswith(prefix):
      return False

    # Consume a request body if present.
    if self.command == 'POST' or self.command == 'PUT' :
      self.ReadRequestBody()

    _, _, url_path, _, query, _ = urlparse.urlparse(self.path)

    if not query in ('C', 'U', 'S', 'M', 'L'):
      return False

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
    uncompressed_len = len(data)
    f.close()

    # Compress the data.
    data = zlib.compress(data)
    compressed_len = len(data)

    content_length = compressed_len
    if query == 'U':
      content_length = uncompressed_len
    elif query == 'S':
      content_length = compressed_len / 2
    elif query == 'M':
      content_length = (compressed_len + uncompressed_len) / 2
    elif query == 'L':
      content_length = compressed_len + uncompressed_len

    self.send_response(200)
    self.send_header('Content-Type', 'application/msword')
    self.send_header('Content-encoding', 'deflate')
    self.send_header('Connection', 'close')
    self.send_header('Content-Length', content_length)
    self.send_header('ETag', '\'' + file_path + '\'')
    self.end_headers()

    self.wfile.write(data)

    return True

  def FileHandler(self):
    """This handler sends the contents of the requested file.  Wow, it's like
    a real webserver!"""
    prefix = self.server.file_root_url
    if not self.path.startswith(prefix):
      return False
    return self._FileHandlerHelper(prefix)

  def PostOnlyFileHandler(self):
    """This handler sends the contents of the requested file on a POST."""
    prefix = urlparse.urljoin(self.server.file_root_url, 'post/')
    if not self.path.startswith(prefix):
      return False
    return self._FileHandlerHelper(prefix)

  def _FileHandlerHelper(self, prefix):
    request_body = ''
    if self.command == 'POST' or self.command == 'PUT':
      # Consume a request body if present.
      request_body = self.ReadRequestBody()

    _, _, url_path, _, query, _ = urlparse.urlparse(self.path)
    query_dict = cgi.parse_qs(query)

    expected_body = query_dict.get('expected_body', [])
    if expected_body and request_body not in expected_body:
      self.send_response(404)
      self.end_headers()
      self.wfile.write('')
      return True

    expected_headers = query_dict.get('expected_headers', [])
    for expected_header in expected_headers:
      header_name, expected_value = expected_header.split(':')
      if self.headers.getheader(header_name) != expected_value:
        self.send_response(404)
        self.end_headers()
        self.wfile.write('')
        return True

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

    old_protocol_version = self.protocol_version

    # If file.mock-http-headers exists, it contains the headers we
    # should send.  Read them in and parse them.
    headers_path = file_path + '.mock-http-headers'
    if os.path.isfile(headers_path):
      f = open(headers_path, "r")

      # "HTTP/1.1 200 OK"
      response = f.readline()
      http_major, http_minor, status_code = re.findall(
          'HTTP/(\d+).(\d+) (\d+)', response)[0]
      self.protocol_version = "HTTP/%s.%s" % (http_major, http_minor)
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
        # Note this doesn't handle all valid byte range values (i.e. left
        # open ended ones), just enough for what we needed so far.
        range = range[6:].split('-')
        start = int(range[0])
        if range[1]:
          end = int(range[1])
        else:
          end = len(data) - 1

        self.send_response(206)
        content_range = 'bytes ' + str(start) + '-' + str(end) + '/' + \
                        str(len(data))
        self.send_header('Content-Range', content_range)
        data = data[start: end + 1]
      else:
        self.send_response(200)

      self.send_header('Content-Type', self.GetMIMETypeFromName(file_path))
      self.send_header('Accept-Ranges', 'bytes')
      self.send_header('Content-Length', len(data))
      self.send_header('ETag', '\'' + file_path + '\'')
    self.end_headers()

    if (self.command != 'HEAD'):
      self.wfile.write(data)

    self.protocol_version = old_protocol_version
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
    self.send_header('Content-Type', 'text/html')
    for cookie_value in cookie_values:
      self.send_header('Set-Cookie', '%s' % cookie_value)
    self.end_headers()
    for cookie_value in cookie_values:
      self.wfile.write('%s' % cookie_value)
    return True

  def SetManyCookiesHandler(self):
    """This handler just sets a given number of cookies, for testing handling
       of large numbers of cookies."""

    if not self._ShouldHandleRequest("/set-many-cookies"):
      return False

    query_char = self.path.find('?')
    if query_char != -1:
      num_cookies = int(self.path[query_char + 1:])
    else:
      num_cookies = 0
    self.send_response(200)
    self.send_header('', 'text/html')
    for i in range(0, num_cookies):
      self.send_header('Set-Cookie', 'a=')
    self.end_headers()
    self.wfile.write('%d cookies were sent' % num_cookies)
    return True

  def ExpectAndSetCookieHandler(self):
    """Expects some cookies to be sent, and if they are, sets more cookies.

    The expect parameter specifies a required cookie.  May be specified multiple
    times.
    The set parameter specifies a cookie to set if all required cookies are
    preset.  May be specified multiple times.
    The data parameter specifies the response body data to be returned."""

    if not self._ShouldHandleRequest("/expect-and-set-cookie"):
      return False

    _, _, _, _, query, _ = urlparse.urlparse(self.path)
    query_dict = cgi.parse_qs(query)
    cookies = set()
    if 'Cookie' in self.headers:
      cookie_header = self.headers.getheader('Cookie')
      cookies.update([s.strip() for s in cookie_header.split(';')])
    got_all_expected_cookies = True
    for expected_cookie in query_dict.get('expect', []):
      if expected_cookie not in cookies:
        got_all_expected_cookies = False
    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    if got_all_expected_cookies:
      for cookie_value in query_dict.get('set', []):
        self.send_header('Set-Cookie', '%s' % cookie_value)
    self.end_headers()
    for data_value in query_dict.get('data', []):
      self.wfile.write(data_value)
    return True

  def SetHeaderHandler(self):
    """This handler sets a response header. Parameters are in the
    key%3A%20value&key2%3A%20value2 format."""

    if not self._ShouldHandleRequest("/set-header"):
      return False

    query_char = self.path.find('?')
    if query_char != -1:
      headers_values = self.path[query_char + 1:].split('&')
    else:
      headers_values = ("",)
    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    for header_value in headers_values:
      header_value = urllib.unquote(header_value)
      (key, value) = header_value.split(': ', 1)
      self.send_header(key, value)
    self.end_headers()
    for header_value in headers_values:
      self.wfile.write('%s' % header_value)
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
      self.send_header('Content-Type', 'text/html')
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
    old_protocol_version = self.protocol_version
    self.protocol_version = "HTTP/1.1"

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
        self.protocol_version = old_protocol_version
        return True

      f = open(gif_path, "rb")
      data = f.read()
      f.close()

      self.send_response(200)
      self.send_header('Content-Type', 'image/gif')
      self.send_header('Cache-control', 'max-age=60000')
      self.send_header('Etag', 'abc')
      self.end_headers()
      self.wfile.write(data)
    else:
      self.send_response(200)
      self.send_header('Content-Type', 'text/html')
      self.send_header('Cache-control', 'max-age=60000')
      self.send_header('Etag', 'abc')
      self.end_headers()
      self.wfile.write('<html><head>')
      self.wfile.write('<title>%s/%s</title>' % (username, password))
      self.wfile.write('</head><body>')
      self.wfile.write('auth=%s<p>' % auth)
      self.wfile.write('You sent:<br>%s<p>' % self.headers)
      self.wfile.write('</body></html>')

    self.protocol_version = old_protocol_version
    return True

  def GDataAuthHandler(self):
    """This handler verifies the Authentication header for GData requests."""
    if not self.server.gdata_auth_token:
      # --auth-token is not specified, not the test case for GData.
      return False

    if not self._ShouldHandleRequest('/files/chromeos/gdata'):
      return False

    if 'GData-Version' not in self.headers:
      self.send_error(httplib.BAD_REQUEST, 'GData-Version header is missing.')
      return True

    if 'Authorization' not in self.headers:
      self.send_error(httplib.UNAUTHORIZED)
      return True

    field_prefix = 'Bearer '
    authorization = self.headers['Authorization']
    if not authorization.startswith(field_prefix):
      self.send_error(httplib.UNAUTHORIZED)
      return True

    code = authorization[len(field_prefix):]
    if code != self.server.gdata_auth_token:
      self.send_error(httplib.UNAUTHORIZED)
      return True

    return False

  def GDataDocumentsFeedQueryHandler(self):
    """This handler verifies if required parameters are properly
    specified for the GData DocumentsFeed request."""
    if not self.server.gdata_auth_token:
      # --auth-token is not specified, not the test case for GData.
      return False

    if not self._ShouldHandleRequest('/files/chromeos/gdata/root_feed.json'):
      return False

    (path, question, query_params) = self.path.partition('?')
    self.query_params = urlparse.parse_qs(query_params)

    if 'v' not in self.query_params:
      self.send_error(httplib.BAD_REQUEST, 'v is not specified.')
      return True
    elif 'alt' not in self.query_params or self.query_params['alt'] != ['json']:
      # currently our GData client only uses JSON format.
      self.send_error(httplib.BAD_REQUEST, 'alt parameter is wrong.')
      return True

    return False

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
      self.send_header('Content-Type', 'text/html')
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
    self.send_header('Content-Type', 'text/html')
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
    self.send_header('Content-Type', 'text/plain')
    self.end_headers()
    self.wfile.write("waited %d seconds" % wait_sec)
    return True

  def ChunkedServerHandler(self):
    """Send chunked response. Allows to specify chunks parameters:
     - waitBeforeHeaders - ms to wait before sending headers
     - waitBetweenChunks - ms to wait between chunks
     - chunkSize - size of each chunk in bytes
     - chunksNumber - number of chunks
    Example: /chunked?waitBeforeHeaders=1000&chunkSize=5&chunksNumber=5
    waits one second, then sends headers and five chunks five bytes each."""
    if not self._ShouldHandleRequest("/chunked"):
      return False
    query_char = self.path.find('?')
    chunkedSettings = {'waitBeforeHeaders' : 0,
                       'waitBetweenChunks' : 0,
                       'chunkSize' : 5,
                       'chunksNumber' : 5}
    if query_char >= 0:
      params = self.path[query_char + 1:].split('&')
      for param in params:
        keyValue = param.split('=')
        if len(keyValue) == 2:
          try:
            chunkedSettings[keyValue[0]] = int(keyValue[1])
          except ValueError:
            pass
    time.sleep(0.001 * chunkedSettings['waitBeforeHeaders']);
    self.protocol_version = 'HTTP/1.1' # Needed for chunked encoding
    self.send_response(200)
    self.send_header('Content-Type', 'text/plain')
    self.send_header('Connection', 'close')
    self.send_header('Transfer-Encoding', 'chunked')
    self.end_headers()
    # Chunked encoding: sending all chunks, then final zero-length chunk and
    # then final CRLF.
    for i in range(0, chunkedSettings['chunksNumber']):
      if i > 0:
        time.sleep(0.001 * chunkedSettings['waitBetweenChunks'])
      self.sendChunkHelp('*' * chunkedSettings['chunkSize'])
      self.wfile.flush(); # Keep in mind that we start flushing only after 1kb.
    self.sendChunkHelp('')
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

  def NoContentHandler(self):
    """Returns a 204 No Content response."""
    if not self._ShouldHandleRequest("/nocontent"):
      return False
    self.send_response(204)
    self.end_headers()
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
    self.send_header('Content-Type', 'text/html')
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
    self.send_header('Content-Type', 'text/html')
    self.end_headers()
    self.wfile.write('<html><head>')
    self.wfile.write('<meta http-equiv="refresh" content="0;url=%s">' % dest)
    self.wfile.write('</head><body>Redirecting to %s</body></html>' % dest)

    return True

  def MultipartHandler(self):
    """Send a multipart response (10 text/html pages)."""
    test_name = '/multipart'
    if not self._ShouldHandleRequest(test_name):
      return False

    num_frames = 10
    bound = '12345'
    self.send_response(200)
    self.send_header('Content-Type',
                     'multipart/x-mixed-replace;boundary=' + bound)
    self.end_headers()

    for i in xrange(num_frames):
      self.wfile.write('--' + bound + '\r\n')
      self.wfile.write('Content-Type: text/html\r\n\r\n')
      self.wfile.write('<title>page ' + str(i) + '</title>')
      self.wfile.write('page ' + str(i))

    self.wfile.write('--' + bound + '--')
    return True

  def MultipartSlowHandler(self):
    """Send a multipart response (3 text/html pages) with a slight delay
    between each page.  This is similar to how some pages show status using
    multipart."""
    test_name = '/multipart-slow'
    if not self._ShouldHandleRequest(test_name):
      return False

    num_frames = 3
    bound = '12345'
    self.send_response(200)
    self.send_header('Content-Type',
                     'multipart/x-mixed-replace;boundary=' + bound)
    self.end_headers()

    for i in xrange(num_frames):
      self.wfile.write('--' + bound + '\r\n')
      self.wfile.write('Content-Type: text/html\r\n\r\n')
      time.sleep(0.25)
      if i == 2:
        self.wfile.write('<title>PASS</title>')
      else:
        self.wfile.write('<title>page ' + str(i) + '</title>')
      self.wfile.write('page ' + str(i) + '<!-- ' + ('x' * 2048) + '-->')

    self.wfile.write('--' + bound + '--')
    return True

  def GetSSLSessionCacheHandler(self):
    """Send a reply containing a log of the session cache operations."""

    if not self._ShouldHandleRequest('/ssl-session-cache'):
      return False

    self.send_response(200)
    self.send_header('Content-Type', 'text/plain')
    self.end_headers()
    try:
      for (action, sessionID) in self.server.session_cache.log:
        self.wfile.write('%s\t%s\n' % (action, sessionID.encode('hex')))
    except AttributeError, e:
      self.wfile.write('Pass --https-record-resume in order to use' +
                       ' this request')
    return True

  def CloseSocketHandler(self):
    """Closes the socket without sending anything."""

    if not self._ShouldHandleRequest('/close-socket'):
      return False

    self.wfile.close()
    return True

  def DefaultResponseHandler(self):
    """This is the catch-all response handler for requests that aren't handled
    by one of the special handlers above.
    Note that we specify the content-length as without it the https connection
    is not closed properly (and the browser keeps expecting data)."""

    contents = "Default response given for path: " + self.path
    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Content-Length', len(contents))
    self.end_headers()
    if (self.command != 'HEAD'):
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
    self.send_header('Content-Type', 'text/html')
    self.send_header('Content-Length', len(contents))
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
          device_management.TestServer(policy_path,
                                       self.server.policy_keys,
                                       self.server.policy_user))

    http_response, raw_reply = (
        self.server._device_management_handler.HandleRequest(self.path,
                                                             self.headers,
                                                             raw_request))
    self.send_response(http_response)
    if (http_response == 200):
      self.send_header('Content-Type', 'application/x-protobuffer')
    self.end_headers()
    self.wfile.write(raw_reply)
    return True

  # called by the redirect handling function when there is no parameter
  def sendRedirectHelp(self, redirect_name):
    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    self.end_headers()
    self.wfile.write('<html><body><h1>Error: no redirect destination</h1>')
    self.wfile.write('Use <pre>%s?http://dest...</pre>' % redirect_name)
    self.wfile.write('</body></html>')

  # called by chunked handling function
  def sendChunkHelp(self, chunk):
    # Each chunk consists of: chunk size (hex), CRLF, chunk body, CRLF
    self.wfile.write('%X\r\n' % len(chunk))
    self.wfile.write(chunk)
    self.wfile.write('\r\n')


class SyncPageHandler(BasePageHandler):
  """Handler for the main HTTP sync server."""

  def __init__(self, request, client_address, sync_http_server):
    get_handlers = [self.ChromiumSyncTimeHandler,
                    self.ChromiumSyncMigrationOpHandler,
                    self.ChromiumSyncCredHandler,
                    self.ChromiumSyncDisableNotificationsOpHandler,
                    self.ChromiumSyncEnableNotificationsOpHandler,
                    self.ChromiumSyncSendNotificationOpHandler,
                    self.ChromiumSyncBirthdayErrorOpHandler,
                    self.ChromiumSyncTransientErrorOpHandler,
                    self.ChromiumSyncErrorOpHandler,
                    self.ChromiumSyncSyncTabFaviconsOpHandler,
                    self.ChromiumSyncCreateSyncedBookmarksOpHandler]

    post_handlers = [self.ChromiumSyncCommandHandler,
                     self.ChromiumSyncTimeHandler]
    BasePageHandler.__init__(self, request, client_address,
                             sync_http_server, [], get_handlers, [],
                             post_handlers, [])


  def ChromiumSyncTimeHandler(self):
    """Handle Chromium sync .../time requests.

    The syncer sometimes checks server reachability by examining /time.
    """
    test_name = "/chromiumsync/time"
    if not self._ShouldHandleRequest(test_name):
      return False

    # Chrome hates it if we send a response before reading the request.
    if self.headers.getheader('content-length'):
      length = int(self.headers.getheader('content-length'))
      raw_request = self.rfile.read(length)

    self.send_response(200)
    self.send_header('Content-Type', 'text/plain')
    self.end_headers()
    self.wfile.write('0123456789')
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
    http_response = 200
    raw_reply = None
    if not self.server.GetAuthenticated():
      http_response = 401
      challenge = 'GoogleLogin realm="http://%s", service="chromiumsync"' % (
        self.server.server_address[0])
    else:
      http_response, raw_reply = self.server.HandleCommand(
          self.path, raw_request)

    ### Now send the response to the client. ###
    self.send_response(http_response)
    if http_response == 401:
      self.send_header('www-Authenticate', challenge)
    self.end_headers()
    self.wfile.write(raw_reply)
    return True

  def ChromiumSyncMigrationOpHandler(self):
    test_name = "/chromiumsync/migrate"
    if not self._ShouldHandleRequest(test_name):
      return False

    http_response, raw_reply = self.server._sync_handler.HandleMigrate(
        self.path)
    self.send_response(http_response)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Content-Length', len(raw_reply))
    self.end_headers()
    self.wfile.write(raw_reply)
    return True

  def ChromiumSyncCredHandler(self):
    test_name = "/chromiumsync/cred"
    if not self._ShouldHandleRequest(test_name):
      return False
    try:
      query = urlparse.urlparse(self.path)[4]
      cred_valid = urlparse.parse_qs(query)['valid']
      if cred_valid[0] == 'True':
        self.server.SetAuthenticated(True)
      else:
        self.server.SetAuthenticated(False)
    except:
      self.server.SetAuthenticated(False)

    http_response = 200
    raw_reply = 'Authenticated: %s ' % self.server.GetAuthenticated()
    self.send_response(http_response)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Content-Length', len(raw_reply))
    self.end_headers()
    self.wfile.write(raw_reply)
    return True

  def ChromiumSyncDisableNotificationsOpHandler(self):
    test_name = "/chromiumsync/disablenotifications"
    if not self._ShouldHandleRequest(test_name):
      return False
    self.server.GetXmppServer().DisableNotifications()
    result = 200
    raw_reply = ('<html><title>Notifications disabled</title>'
                 '<H1>Notifications disabled</H1></html>')
    self.send_response(result)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Content-Length', len(raw_reply))
    self.end_headers()
    self.wfile.write(raw_reply)
    return True;

  def ChromiumSyncEnableNotificationsOpHandler(self):
    test_name = "/chromiumsync/enablenotifications"
    if not self._ShouldHandleRequest(test_name):
      return False
    self.server.GetXmppServer().EnableNotifications()
    result = 200
    raw_reply = ('<html><title>Notifications enabled</title>'
                 '<H1>Notifications enabled</H1></html>')
    self.send_response(result)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Content-Length', len(raw_reply))
    self.end_headers()
    self.wfile.write(raw_reply)
    return True;

  def ChromiumSyncSendNotificationOpHandler(self):
    test_name = "/chromiumsync/sendnotification"
    if not self._ShouldHandleRequest(test_name):
      return False
    query = urlparse.urlparse(self.path)[4]
    query_params = urlparse.parse_qs(query)
    channel = ''
    data = ''
    if 'channel' in query_params:
      channel = query_params['channel'][0]
    if 'data' in query_params:
      data = query_params['data'][0]
    self.server.GetXmppServer().SendNotification(channel, data)
    result = 200
    raw_reply = ('<html><title>Notification sent</title>'
                 '<H1>Notification sent with channel "%s" '
                 'and data "%s"</H1></html>'
                 % (channel, data))
    self.send_response(result)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Content-Length', len(raw_reply))
    self.end_headers()
    self.wfile.write(raw_reply)
    return True;

  def ChromiumSyncBirthdayErrorOpHandler(self):
    test_name = "/chromiumsync/birthdayerror"
    if not self._ShouldHandleRequest(test_name):
      return False
    result, raw_reply = self.server._sync_handler.HandleCreateBirthdayError()
    self.send_response(result)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Content-Length', len(raw_reply))
    self.end_headers()
    self.wfile.write(raw_reply)
    return True;

  def ChromiumSyncTransientErrorOpHandler(self):
    test_name = "/chromiumsync/transienterror"
    if not self._ShouldHandleRequest(test_name):
      return False
    result, raw_reply = self.server._sync_handler.HandleSetTransientError()
    self.send_response(result)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Content-Length', len(raw_reply))
    self.end_headers()
    self.wfile.write(raw_reply)
    return True;

  def ChromiumSyncErrorOpHandler(self):
    test_name = "/chromiumsync/error"
    if not self._ShouldHandleRequest(test_name):
      return False
    result, raw_reply = self.server._sync_handler.HandleSetInducedError(
        self.path)
    self.send_response(result)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Content-Length', len(raw_reply))
    self.end_headers()
    self.wfile.write(raw_reply)
    return True;

  def ChromiumSyncSyncTabFaviconsOpHandler(self):
    test_name = "/chromiumsync/synctabfavicons"
    if not self._ShouldHandleRequest(test_name):
      return False
    result, raw_reply = self.server._sync_handler.HandleSetSyncTabFavicons()
    self.send_response(result)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Content-Length', len(raw_reply))
    self.end_headers()
    self.wfile.write(raw_reply)
    return True;

  def ChromiumSyncCreateSyncedBookmarksOpHandler(self):
    test_name = "/chromiumsync/createsyncedbookmarks"
    if not self._ShouldHandleRequest(test_name):
      return False
    result, raw_reply = self.server._sync_handler.HandleCreateSyncedBookmarks()
    self.send_response(result)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Content-Length', len(raw_reply))
    self.end_headers()
    self.wfile.write(raw_reply)
    return True;


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

class OCSPHandler(BasePageHandler):
  def __init__(self, request, client_address, socket_server):
    handlers = [self.OCSPResponse]
    self.ocsp_response = socket_server.ocsp_response
    BasePageHandler.__init__(self, request, client_address, socket_server,
                             [], handlers, [], handlers, [])

  def OCSPResponse(self):
    self.send_response(200)
    self.send_header('Content-Type', 'application/ocsp-response')
    self.send_header('Content-Length', str(len(self.ocsp_response)))
    self.end_headers()

    self.wfile.write(self.ocsp_response)

class TCPEchoHandler(SocketServer.BaseRequestHandler):
  """The RequestHandler class for TCP echo server.

  It is instantiated once per connection to the server, and overrides the
  handle() method to implement communication to the client.
  """

  def handle(self):
    """Handles the request from the client and constructs a response."""

    data = self.request.recv(65536).strip()
    # Verify the "echo request" message received from the client. Send back
    # "echo response" message if "echo request" message is valid.
    try:
      return_data = echo_message.GetEchoResponseData(data)
      if not return_data:
        return
    except ValueError:
      return

    self.request.send(return_data)


class UDPEchoHandler(SocketServer.BaseRequestHandler):
  """The RequestHandler class for UDP echo server.

  It is instantiated once per connection to the server, and overrides the
  handle() method to implement communication to the client.
  """

  def handle(self):
    """Handles the request from the client and constructs a response."""

    data = self.request[0].strip()
    socket = self.request[1]
    # Verify the "echo request" message received from the client. Send back
    # "echo response" message if "echo request" message is valid.
    try:
      return_data = echo_message.GetEchoResponseData(data)
      if not return_data:
        return
    except ValueError:
      return
    socket.sendto(return_data, self.client_address)


class BasicAuthProxyRequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  """A request handler that behaves as a proxy server which requires
  basic authentication. Only CONNECT, GET and HEAD is supported for now.
  """

  _AUTH_CREDENTIAL = 'Basic Zm9vOmJhcg==' # foo:bar

  def parse_request(self):
    """Overrides parse_request to check credential."""

    if not BaseHTTPServer.BaseHTTPRequestHandler.parse_request(self):
      return False

    auth = self.headers.getheader('Proxy-Authorization')
    if auth != self._AUTH_CREDENTIAL:
      self.send_response(407)
      self.send_header('Proxy-Authenticate', 'Basic realm="MyRealm1"')
      self.end_headers()
      return False

    return True

  def _start_read_write(self, sock):
    sock.setblocking(0)
    self.request.setblocking(0)
    rlist = [self.request, sock]
    while True:
      ready_sockets, unused, errors = select.select(rlist, [], [])
      if errors:
        self.send_response(500)
        self.end_headers()
        return
      for s in ready_sockets:
        received = s.recv(1024)
        if len(received) == 0:
          return
        if s == self.request:
          other = sock
        else:
          other = self.request
        other.send(received)

  def _do_common_method(self):
    url = urlparse.urlparse(self.path)
    port = url.port
    if not port:
      if url.scheme == 'http':
        port = 80
      elif url.scheme == 'https':
        port = 443
    if not url.hostname or not port:
      self.send_response(400)
      self.end_headers()
      return

    if len(url.path) == 0:
      path = '/'
    else:
      path = url.path
    if len(url.query) > 0:
      path = '%s?%s' % (url.path, url.query)

    sock = None
    try:
      sock = socket.create_connection((url.hostname, port))
      sock.send('%s %s %s\r\n' % (
          self.command, path, self.protocol_version))
      for header in self.headers.headers:
        header = header.strip()
        if (header.lower().startswith('connection') or
            header.lower().startswith('proxy')):
          continue
        sock.send('%s\r\n' % header)
      sock.send('\r\n')
      self._start_read_write(sock)
    except:
      self.send_response(500)
      self.end_headers()
    finally:
      if sock is not None:
        sock.close()

  def do_CONNECT(self):
    try:
      pos = self.path.rfind(':')
      host = self.path[:pos]
      port = int(self.path[pos+1:])
    except:
      self.send_response(400)
      self.end_headers()

    try:
      sock = socket.create_connection((host, port))
      self.send_response(200, 'Connection established')
      self.end_headers()
      self._start_read_write(sock)
    except:
      self.send_response(500)
      self.end_headers()
    finally:
      sock.close()

  def do_GET(self):
    self._do_common_method()

  def do_HEAD(self):
    self._do_common_method()


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
  sys.stderr = FileMultiplexer(sys.stderr, logfile)
  if options.log_to_console:
    sys.stdout = FileMultiplexer(sys.stdout, logfile)
  else:
    sys.stdout = logfile

  port = options.port
  host = options.host

  server_data = {}
  server_data['host'] = host

  ocsp_server = None

  if options.server_type == SERVER_HTTP:
    if options.https:
      pem_cert_and_key = None
      if options.cert_and_key_file:
        if not os.path.isfile(options.cert_and_key_file):
          print ('specified server cert file not found: ' +
                 options.cert_and_key_file + ' exiting...')
          return
        pem_cert_and_key = file(options.cert_and_key_file, 'r').read()
      else:
        # generate a new certificate and run an OCSP server for it.
        ocsp_server = OCSPServer((host, 0), OCSPHandler)
        print ('OCSP server started on %s:%d...' %
            (host, ocsp_server.server_port))

        ocsp_der = None
        ocsp_state = None

        if options.ocsp == 'ok':
          ocsp_state = minica.OCSP_STATE_GOOD
        elif options.ocsp == 'revoked':
          ocsp_state = minica.OCSP_STATE_REVOKED
        elif options.ocsp == 'invalid':
          ocsp_state = minica.OCSP_STATE_INVALID
        elif options.ocsp == 'unauthorized':
          ocsp_state = minica.OCSP_STATE_UNAUTHORIZED
        elif options.ocsp == 'unknown':
          ocsp_state = minica.OCSP_STATE_UNKNOWN
        else:
          print 'unknown OCSP status: ' + options.ocsp_status
          return

        (pem_cert_and_key, ocsp_der) = \
            minica.GenerateCertKeyAndOCSP(
                subject = "127.0.0.1",
                ocsp_url = ("http://%s:%d/ocsp" %
                    (host, ocsp_server.server_port)),
                ocsp_state = ocsp_state)

        ocsp_server.ocsp_response = ocsp_der

      for ca_cert in options.ssl_client_ca:
        if not os.path.isfile(ca_cert):
          print 'specified trusted client CA file not found: ' + ca_cert + \
                ' exiting...'
          return
      server = HTTPSServer((host, port), TestPageHandler, pem_cert_and_key,
                           options.ssl_client_auth, options.ssl_client_ca,
                           options.ssl_bulk_cipher, options.record_resume,
                           options.tls_intolerant)
      print 'HTTPS server started on %s:%d...' % (host, server.server_port)
    else:
      server = HTTPServer((host, port), TestPageHandler)
      print 'HTTP server started on %s:%d...' % (host, server.server_port)

    server.data_dir = MakeDataDir()
    server.file_root_url = options.file_root_url
    server_data['port'] = server.server_port
    server._device_management_handler = None
    server.policy_keys = options.policy_keys
    server.policy_user = options.policy_user
    server.gdata_auth_token = options.auth_token
  elif options.server_type == SERVER_SYNC:
    xmpp_port = options.xmpp_port
    server = SyncHTTPServer((host, port), xmpp_port, SyncPageHandler)
    print 'Sync HTTP server started on port %d...' % server.server_port
    print 'Sync XMPP server started on port %d...' % server.xmpp_port
    server_data['port'] = server.server_port
    server_data['xmpp_port'] = server.xmpp_port
  elif options.server_type == SERVER_TCP_ECHO:
    # Used for generating the key (randomly) that encodes the "echo request"
    # message.
    random.seed()
    server = TCPEchoServer((host, port), TCPEchoHandler)
    print 'Echo TCP server started on port %d...' % server.server_port
    server_data['port'] = server.server_port
  elif options.server_type == SERVER_UDP_ECHO:
    # Used for generating the key (randomly) that encodes the "echo request"
    # message.
    random.seed()
    server = UDPEchoServer((host, port), UDPEchoHandler)
    print 'Echo UDP server started on port %d...' % server.server_port
    server_data['port'] = server.server_port
  elif options.server_type == SERVER_BASIC_AUTH_PROXY:
    server = HTTPServer((host, port), BasicAuthProxyRequestHandler)
    print 'BasicAuthProxy server started on port %d...' % server.server_port
    server_data['port'] = server.server_port
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

    # Instantiate FTP server class and listen to address:port
    server = pyftpdlib.ftpserver.FTPServer((host, port), ftp_handler)
    server_data['port'] = server.socket.getsockname()[1]
    print 'FTP server started on port %d...' % server_data['port']

  # Notify the parent that we've started. (BaseServer subclasses
  # bind their sockets on construction.)
  if options.startup_pipe is not None:
    server_data_json = json.dumps(server_data)
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

  if ocsp_server is not None:
    ocsp_server.serve_forever_on_thread()

  try:
    server.serve_forever()
  except KeyboardInterrupt:
    print 'shutting down server'
    if ocsp_server is not None:
      ocsp_server.stop_serving()
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
  option_parser.add_option('', '--tcp-echo', action='store_const',
                           const=SERVER_TCP_ECHO, default=SERVER_HTTP,
                           dest='server_type',
                           help='start up a tcp echo server.')
  option_parser.add_option('', '--udp-echo', action='store_const',
                           const=SERVER_UDP_ECHO, default=SERVER_HTTP,
                           dest='server_type',
                           help='start up a udp echo server.')
  option_parser.add_option('', '--basic-auth-proxy', action='store_const',
                           const=SERVER_BASIC_AUTH_PROXY, default=SERVER_HTTP,
                           dest='server_type',
                           help='start up a proxy server which requires basic '
                           'authentication.')
  option_parser.add_option('', '--log-to-console', action='store_const',
                           const=True, default=False,
                           dest='log_to_console',
                           help='Enables or disables sys.stdout logging to '
                           'the console.')
  option_parser.add_option('', '--port', default='0', type='int',
                           help='Port used by the server. If unspecified, the '
                           'server will listen on an ephemeral port.')
  option_parser.add_option('', '--xmpp-port', default='0', type='int',
                           help='Port used by the XMPP server. If unspecified, '
                           'the XMPP server will listen on an ephemeral port.')
  option_parser.add_option('', '--data-dir', dest='data_dir',
                           help='Directory from which to read the files.')
  option_parser.add_option('', '--https', action='store_true', dest='https',
                           help='Specify that https should be used.')
  option_parser.add_option('', '--cert-and-key-file', dest='cert_and_key_file',
                           help='specify the path to the file containing the '
                           'certificate and private key for the server in PEM '
                           'format')
  option_parser.add_option('', '--ocsp', dest='ocsp', default='ok',
                           help='The type of OCSP response generated for the '
                           'automatically generated certificate. One of '
                           '[ok,revoked,invalid]')
  option_parser.add_option('', '--tls-intolerant', dest='tls_intolerant',
                           default='0', type='int',
                           help='If nonzero, certain TLS connections will be'
                           ' aborted in order to test version fallback. 1'
                           ' means all TLS versions will be aborted. 2 means'
                           ' TLS 1.1 or higher will be aborted. 3 means TLS'
                           ' 1.2 or higher will be aborted.')
  option_parser.add_option('', '--https-record-resume', dest='record_resume',
                           const=True, default=False, action='store_const',
                           help='Record resumption cache events rather than'
                           ' resuming as normal. Allows the use of the'
                           ' /ssl-session-cache request')
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
  option_parser.add_option('', '--policy-key', action='append',
                           dest='policy_keys',
                           help='Specify a path to a PEM-encoded private key '
                           'to use for policy signing. May be specified '
                           'multiple times in order to load multipe keys into '
                           'the server. If ther server has multiple keys, it '
                           'will rotate through them in at each request a '
                           'round-robin fashion. The server will generate a '
                           'random key if none is specified on the command '
                           'line.')
  option_parser.add_option('', '--policy-user', default='user@example.com',
                           dest='policy_user',
                           help='Specify the user name the server should '
                           'report back to the client as the user owning the '
                           'token used for making the policy request.')
  option_parser.add_option('', '--host', default='127.0.0.1',
                           dest='host',
                           help='Hostname or IP upon which the server will '
                           'listen. Client connections will also only be '
                           'allowed from this address.')
  option_parser.add_option('', '--auth-token', dest='auth_token',
                           help='Specify the auth token which should be used'
                           'in the authorization header for GData.')
  options, args = option_parser.parse_args()

  sys.exit(main(options, args))
