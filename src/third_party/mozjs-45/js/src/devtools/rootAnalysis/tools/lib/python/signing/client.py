import base64
import urllib2
import os
import hashlib
import time
import socket
import httplib
import urllib

# TODO: Use util.command
from subprocess import check_call

from poster.encode import multipart_encode

from util.file import sha1sum, copyfile

import logging
log = logging.getLogger(__name__)


def getfile(baseurl, filehash, format_):
    url = "%s/sign/%s/%s" % (baseurl, format_, filehash)
    log.debug("%s: GET %s", filehash, url)
    r = urllib2.Request(url)
    return urllib2.urlopen(r)


def get_token(baseurl, username, password, slave_ip, duration):
    auth = base64.encodestring('%s:%s' % (username, password)).rstrip('\n')
    url = '%s/token' % baseurl
    data = urllib.urlencode({
        'slave_ip': slave_ip,
        'duration': duration,
    })
    headers = {
        'Authorization': 'Basic %s' % auth,
        'Content-Length': str(len(data)),
    }
    r = urllib2.Request(url, data, headers)
    return urllib2.urlopen(r).read()


def remote_signfile(options, urls, filename, fmt, token, dest=None):
    filehash = sha1sum(filename)
    if dest is None:
        dest = filename

    if fmt == 'gpg':
        dest += '.asc'

    parent_dir = os.path.dirname(os.path.abspath(dest))
    if not os.path.exists(parent_dir):
        os.makedirs(parent_dir)

    # Check the cache
    cached_fn = None
    if options.cachedir:
        log.debug("%s: checking cache", filehash)
        cached_fn = os.path.join(options.cachedir, fmt, filehash)
        if os.path.exists(cached_fn):
            log.info("%s: exists in the cache; copying to %s", filehash, dest)
            cached_fp = open(cached_fn, 'rb')
            tmpfile = dest + '.tmp'
            fp = open(tmpfile, 'wb')
            hsh = hashlib.new('sha1')
            while True:
                data = cached_fp.read(1024 ** 2)
                if not data:
                    break
                hsh.update(data)
                fp.write(data)
            fp.close()
            newhash = hsh.hexdigest()
            if os.path.exists(dest):
                os.unlink(dest)
            os.rename(tmpfile, dest)
            log.info("%s: OK", filehash)
            # See if we should re-sign NSS
            if options.nsscmd and filehash != newhash and os.path.exists(os.path.splitext(filename)[0] + ".chk"):
                cmd = '%s "%s"' % (options.nsscmd, dest)
                log.info("Regenerating .chk file")
                log.debug("Running %s", cmd)
                check_call(cmd, shell=True)
            return True

    errors = 0
    pendings = 0
    max_errors = 20
    max_pending_tries = 300
    while True:
        if pendings >= max_pending_tries:
            log.error("%s: giving up after %i tries", filehash, pendings)
            return False
        if errors >= max_errors:
            log.error("%s: giving up after %i tries", filehash, errors)
            return False
        # Try to get a previously signed copy of this file
        try:
            url = urls[0]
            log.info("%s: processing %s on %s", filehash, filename, url)
            req = getfile(url, filehash, fmt)
            headers = req.info()
            responsehash = headers['X-SHA1-Digest']
            tmpfile = dest + '.tmp'
            fp = open(tmpfile, 'wb')
            while True:
                data = req.read(1024 ** 2)
                if not data:
                    break
                fp.write(data)
            fp.close()
            newhash = sha1sum(tmpfile)
            if newhash != responsehash:
                log.warn(
                    "%s: hash mismatch; trying to download again", filehash)
                os.unlink(tmpfile)
                errors += 1
                continue
            if os.path.exists(dest):
                os.unlink(dest)
            os.rename(tmpfile, dest)
            log.info("%s: OK", filehash)
            # See if we should re-sign NSS
            if options.nsscmd and filehash != responsehash and os.path.exists(os.path.splitext(filename)[0] + ".chk"):
                cmd = '%s "%s"' % (options.nsscmd, dest)
                log.info("Regenerating .chk file")
                log.debug("Running %s", cmd)
                check_call(cmd, shell=True)

            # Possibly write to our cache
            if cached_fn:
                cached_dir = os.path.dirname(cached_fn)
                if not os.path.exists(cached_dir):
                    log.debug("Creating %s", cached_dir)
                    os.makedirs(cached_dir)
                log.info("Copying %s to cache %s", dest, cached_fn)
                copyfile(dest, cached_fn)
            break
        except urllib2.HTTPError, e:
            try:
                if 'X-Pending' in e.headers:
                    log.debug("%s: pending; try again in a bit", filehash)
                    time.sleep(1)
                    pendings += 1
                    continue
            except:
                raise

            errors += 1

            # That didn't work...so let's upload it
            log.info("%s: uploading for signing", filehash)
            req = None
            try:
                try:
                    nonce = open(options.noncefile, 'rb').read()
                except IOError:
                    nonce = ""
                req = uploadfile(url, filename, fmt, token, nonce=nonce)
                nonce = req.info()['X-Nonce']
                open(options.noncefile, 'wb').write(nonce)
            except urllib2.HTTPError, e:
                # python2.5 doesn't think 202 is ok...but really it is!
                if 'X-Nonce' in e.headers:
                    log.debug("updating nonce")
                    nonce = e.headers['X-Nonce']
                    open(options.noncefile, 'wb').write(nonce)
                if e.code != 202:
                    log.exception("%s: error uploading file for signing: %s %s",
                                  filehash, e.code, e.msg)
                    urls.pop(0)
                    urls.append(url)
            except (urllib2.URLError, socket.error, httplib.BadStatusLine):
                # Try again in a little while
                log.exception("%s: connection error; trying again soon", filehash)
                # Move the current url to the back
                urls.pop(0)
                urls.append(url)
            time.sleep(1)
            continue
        except (urllib2.URLError, socket.error):
            # Try again in a little while
            log.exception("%s: connection error; trying again soon", filehash)
            # Move the current url to the back
            urls.pop(0)
            urls.append(url)
            time.sleep(1)
            errors += 1
            continue
    return True


def buildValidatingOpener(ca_certs):
    """Build and register an HTTPS connection handler that validates that we're
    talking to a host matching ca_certs (a file containing a list of
    certificates we accept.

    Subsequent calls to HTTPS urls will validate that we're talking to an acceptable server.
    """
    try:
        from poster.streaminghttp import StreamingHTTPSHandler as HTTPSHandler, \
            StreamingHTTPSConnection as HTTPSConnection
        assert HTTPSHandler  # pyflakes
        assert HTTPSConnection  # pyflakes
    except ImportError:
        from httplib import HTTPSConnection
        from urllib2 import HTTPSHandler

    import ssl

    class VerifiedHTTPSConnection(HTTPSConnection):
        def connect(self):
            # overrides the version in httplib so that we do
            #    certificate verification
            # sock = socket.create_connection((self.host, self.port),
                                            # self.timeout)
            # if self._tunnel_host:
                # self.sock = sock
                # self._tunnel()

            sock = socket.socket()
            sock.connect((self.host, self.port))

            # wrap the socket using verification with the root
            #    certs in trusted_root_certs
            self.sock = ssl.wrap_socket(sock,
                                        self.key_file,
                                        self.cert_file,
                                        cert_reqs=ssl.CERT_REQUIRED,
                                        ca_certs=ca_certs,
                                        ssl_version=ssl.PROTOCOL_TLSv1,
                                        )

    # wraps https connections with ssl certificate verification
    class VerifiedHTTPSHandler(HTTPSHandler):
        def __init__(self, connection_class=VerifiedHTTPSConnection):
            self.specialized_conn_class = connection_class
            HTTPSHandler.__init__(self)

        def https_open(self, req):
            return self.do_open(self.specialized_conn_class, req)

    https_handler = VerifiedHTTPSHandler()
    opener = urllib2.build_opener(https_handler)
    urllib2.install_opener(opener)


def uploadfile(baseurl, filename, format_, token, nonce):
    """Uploads file (given by `filename`) to server at `baseurl`.

    `sesson_key` and `nonce` are string values that get passed as POST
    parameters.
    """
    from poster.encode import multipart_encode
    filehash = sha1sum(filename)

    try:
        fp = open(filename, 'rb')

        params = {
            'filedata': fp,
            'sha1': filehash,
            'filename': os.path.basename(filename),
            'token': token,
            'nonce': nonce,
        }

        datagen, headers = multipart_encode(params)
        r = urllib2.Request(
            "%s/sign/%s" % (baseurl, format_), datagen, headers)
        return urllib2.urlopen(r)
    finally:
        fp.close()
