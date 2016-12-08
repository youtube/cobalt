# http://aspn.activestate.com/ASPN/Cookbook/Python/Recipe/146306
#   Submitter: Wade Leftwich
# Licensing:
#   according to http://aspn.activestate.com/ASPN/Cookbook/Python
#   "Except where otherwise noted, recipes in the Python Cookbook are published under the Python license ."
# This recipe is covered under the Python license:
# http://www.python.org/license

import httplib
import mimetypes
import urllib2
import socket
from socket import error, herror, gaierror, timeout
socket.setdefaulttimeout(None)
import urlparse


def post_multipart(host, selector, fields, files):
    """
    Post fields and files to an http host as multipart/form-data.
    fields is a sequence of (name, value) elements for regular form fields.
    files is a sequence of (name, filename, value) elements for data to be uploaded as files
    Return the server's response page.
    """
    try:
        host = host.replace('http://', '')
        index = host.find('/')
        if index > 0:
            selector = '/'.join([host[index:], selector.lstrip('/')])
            host = host[0:index]
        content_type, body = encode_multipart_formdata(fields, files)
        h = httplib.HTTP(host)
        h.putrequest('POST', selector)
        h.putheader('Host', host)
        h.putheader('Content-Length', str(len(body)))
        h.putheader('Content-Type', content_type)
        h.putheader('connection', 'close')
        h.endheaders()
        h.send(body)
        errcode, errmsg, headers = h.getreply()
        return h.file.read()
    except (httplib.HTTPException, error, herror, gaierror, timeout), e:
        print "FAIL: graph server unreachable"
        print "FAIL: " + str(e)
        raise
    except:
        print "FAIL: graph server unreachable"
        raise


def encode_multipart_formdata(fields, files):
    """
    fields is a sequence of (name, value) elements for regular form fields.
    files is a sequence of (name, filename, value) elements for data to be uploaded as files
    Return (content_type, body) ready for httplib.HTTP instance
    """
    BOUNDARY = '----------ThIs_Is_tHe_bouNdaRY_$'
    CRLF = '\r\n'
    L = []
    for (key, value) in fields:
        L.append('--' + BOUNDARY)
        L.append('Content-Disposition: form-data; name="%s"' % key)
        L.append('')
        L.append(value)
    for (key, filename, value) in files:
        L.append('--' + BOUNDARY)
        L.append('Content-Disposition: form-data; name="%s"; filename="%s"' %
                 (key, filename))
        L.append('Content-Type: %s' % get_content_type(filename))
        L.append('')
        L.append(value)
    L.append('--' + BOUNDARY + '--')
    L.append('')
    body = CRLF.join(L)
    content_type = 'multipart/form-data; boundary=%s' % BOUNDARY
    return content_type, body


def get_content_type(filename):
    return mimetypes.guess_type(filename)[0] or 'application/octet-stream'
