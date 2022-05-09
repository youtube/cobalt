#!/usr/bin/python
from six.moves.urllib import parse as urlparse

from mod_pywebsocket import common, msgutil, util
from mod_pywebsocket.handshake import hybi


def web_socket_do_extra_handshake(request):
    url_parts = urlparse.urlsplit(request.uri)
    msg = b'HTTP/1.1 101 Switching Protocols:\x0D\x0AConnection: Upgrade\x0D\x0AUpgrade: WebSocket\x0D\x0ASet-Cookie: ws_test_'
    msg += url_parts.query.encode() or b''
    msg += b'=test; Path=/\x0D\x0ASec-WebSocket-Origin: '
    msg += request.ws_origin.encode()
    msg += b'\x0D\x0ASec-WebSocket-Accept: '
    msg += hybi.compute_accept_from_unicode(request.headers_in.get(common.SEC_WEBSOCKET_KEY_HEADER))
    msg += b'\x0D\x0A\x0D\x0A'
    request.connection.write(msg)
    return

def web_socket_transfer_data(request):
    while True:
        return
