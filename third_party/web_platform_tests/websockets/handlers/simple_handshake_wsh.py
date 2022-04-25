#!/usr/bin/python

from mod_pywebsocket import common, msgutil, util
from mod_pywebsocket.handshake import hybi


def web_socket_do_extra_handshake(request):
    msg = (b'HTTP/1.1 101 Switching Protocols:\x0D\x0A'
           b'Connection: Upgrade\x0D\x0A'
           b'Upgrade: WebSocket\x0D\x0A'
           b'Set-Cookie: ws_test=test\x0D\x0A'
           b'Sec-WebSocket-Origin: %s\x0D\x0A'
           b'Sec-WebSocket-Accept: %s\x0D\x0A\x0D\x0A') % (request.ws_origin.encode(), hybi.compute_accept_from_unicode(request.headers_in.get(common.SEC_WEBSOCKET_KEY_HEADER)))
    request.connection.write(msg)
    return

def web_socket_transfer_data(request):
    while True:
        return
