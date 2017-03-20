import os
import hashlib
import hmac
import shlex
import time
import signal
import re
import tempfile
# TODO: use util.command
from subprocess import Popen, PIPE, STDOUT

import gevent
from gevent import queue
from gevent.event import Event
from gevent import pywsgi
from IPy import IP
import webob

from util import b64
from util.file import safe_unlink, sha1sum, safe_copyfile

import logging
log = logging.getLogger(__name__)


def make_token_data(slave_ip, valid_from, valid_to, chaff_bytes=16):
    """Return a string suitable for using as token data. This string will
    be signed, and the signature passed back to clients as the token
    key."""
    chaff = b64(os.urandom(chaff_bytes))
    block = "%s:%s:%s:%s" % (slave_ip, valid_from, valid_to,
            chaff)
    return block


def sign_data(data, secret, hsh=hashlib.sha256):
    """Returns b64(hmac(secret, data))"""
    h = hmac.new(secret, data, hsh)
    return b64(h.digest())


def verify_token(token_data, token, secret):
    """Returns True if token is the proper signature for
    token_data."""
    return sign_data(token_data, secret) == token


def unpack_token_data(token_data):
    """Reverse of make_token_data: takes an encoded string and returns a
    dictionary with token parameters as keys."""
    bits = token_data.split(":")
    return dict(
        slave_ip=bits[0],
        valid_from=int(bits[1]),
        valid_to=int(bits[2]),
    )


def run_signscript(cmd, inputfile, outputfile, filename, format_, passphrase=None, max_tries=5):
    """Run the signing script `cmd`, passing the inputfile, outputfile,
    original filename and format.

    If passphrase is set, it is passed on standard input.

    Returns 0 on success, non-zero otherwise.

    The command currently is allowed 10 seconds to complete before being killed
    and tried again.
    """
    if isinstance(cmd, basestring):
        cmd = shlex.split(cmd)
    else:
        cmd = cmd[:]

    cmd.extend((format_, inputfile, outputfile, filename))
    output = open(outputfile + '.out', 'wb')
    max_time = 120
    tries = 0
    while True:
        start = time.time()
        # Make sure to call os.setsid() after we fork so the spawned process is
        # its own session leader. This means that it will get its own process
        # group, and signals sent to its process group will also be sent to its
        # children. This allows us to kill of any grandchildren processes (e.g.
        # signmar) in cases where the child process is taking too long to
        # return.
        proc = Popen(cmd, stdout=output, stderr=STDOUT, stdin=PIPE,
                     close_fds=True, preexec_fn=lambda: os.setsid())
        if passphrase:
            proc.stdin.write(passphrase)
        proc.stdin.close()
        log.debug("%s: %s", proc.pid, cmd)
        siglist = [signal.SIGINT, signal.SIGTERM]
        timeout = False
        while True:
            if time.time() - start > max_time:
                timeout = True
                log.debug("%s: Exceeded timeout", proc.pid)
                # Kill off the process
                if siglist:
                    sig = siglist.pop(0)
                else:
                    sig = signal.SIGKILL
                try:
                    # Kill off the process group first, and then the process
                    # itself for good measure
                    os.kill(-proc.pid, sig)
                    os.kill(proc.pid, sig)
                    gevent.sleep(1)
                    os.kill(proc.pid, 0)
                except OSError:
                    # The process is gone now
                    rc = -1
                    break

            rc = proc.poll()
            if rc is not None:
                # if we killed the child, we want to break before we poll, since polling may not give
                # an error return code.
                if timeout:
                    rc = -1
                break

            # Check again in a bit
            gevent.sleep(0.25)

        if rc == 0:
            log.debug("%s: Success!", proc.pid)
            return 0
        # Try again it a bit
        log.info("%s: run_signscript: Failed with rc %i; retrying in a bit",
                 proc.pid, rc)
        tries += 1
        if tries >= max_tries:
            log.warning(
                "run_signscript: Exceeded maximum number of retries; exiting")
            return 1
        gevent.sleep(5)


class Signer(object):
    """
    Main signing object

    `signcmd` is a string representing which command to run to sign files
    `inputdir` and `outputdir` are where uploaded files and signed files will be stored
    `passphrases` is a dict of format => passphrase
    `concurrency` is how many workers to run
    """
    stopped = False

    def __init__(self, app, signcmd, inputdir, outputdir, concurrency, passphrases):
        self.app = app
        self.signcmd = signcmd
        self.concurrency = concurrency
        self.inputdir = inputdir
        self.outputdir = outputdir

        self.passphrases = passphrases

        self.workers = []
        self.queue = queue.Queue()

    def signfile(self, filehash, filename, format_):
        assert not self.stopped
        e = Event()
        item = (filehash, filename, format_, e)
        log.debug("Putting %s on the queue", item)
        self.queue.put(item)
        self._start_worker()
        log.debug("%i workers active", len(self.workers))
        return e

    def _start_worker(self):
        if len(self.workers) < self.concurrency:
            t = gevent.spawn(self._worker)
            t.link(self._worker_done)
            self.workers.append(t)

    def _worker_done(self, t):
        log.debug("Done worker")
        self.workers.remove(t)
        # If there's still work to do, start another worker
        if self.queue.qsize() and not self.stopped:
            self._start_worker()
        log.debug("%i workers left", len(self.workers))

    def _worker(self):
        # Main worker process
        # We pop items off the queue and process them

        # How many jobs to process before exiting
        max_jobs = 10
        jobs = 0
        while True:
            # Event to signal when we're done
            e = None
            try:
                jobs += 1
                # Fall on our sword if we're too old
                if jobs >= max_jobs:
                    break

                try:
                    item = self.queue.get(block=False)
                    if not item:
                        break
                except queue.Empty:
                    log.debug("no items, exiting")
                    break

                filehash, filename, format_, e = item
                log.info("Signing %s (%s - %s)", filename, format_, filehash)

                inputfile = os.path.join(self.inputdir, filehash)
                outputfile = os.path.join(self.outputdir, format_, filehash)
                logfile = outputfile + ".out"

                if not os.path.exists(os.path.join(self.outputdir, format_)):
                    os.makedirs(os.path.join(self.outputdir, format_))

                retval = run_signscript(self.signcmd, inputfile, outputfile,
                                        filename, format_, self.passphrases.get(format_))

                if retval != 0:
                    if os.path.exists(logfile):
                        logoutput = open(logfile).read()
                    else:
                        logoutput = None
                    log.warning("Signing failed %s (%s - %s)",
                                filename, format_, filehash)
                    log.warning("Signing log: %s", logoutput)
                    safe_unlink(outputfile)
                    self.app.messages.put(
                        ('errors', item, 'signing script returned non-zero'))
                    continue

                # Copy our signed result into unsigned and signed so if
                # somebody wants to get this file signed again, they get the
                # same results.
                outputhash = sha1sum(outputfile)
                log.debug("Copying result to %s", outputhash)
                copied_input = os.path.join(self.inputdir, outputhash)
                if not os.path.exists(copied_input):
                    safe_copyfile(outputfile, copied_input)
                copied_output = os.path.join(
                    self.outputdir, format_, outputhash)
                if not os.path.exists(copied_output):
                    safe_copyfile(outputfile, copied_output)
                self.app.messages.put(('done', item, outputhash))
            except:
                # Inconceivable! Something went wrong!
                # Remove our output, it might be corrupted
                safe_unlink(outputfile)
                if os.path.exists(logfile):
                    logoutput = open(logfile).read()
                else:
                    logoutput = None
                log.exception(
                    "Exception signing file %s; output: %s ", item, logoutput)
                self.app.messages.put((
                    'errors', item, 'worker hit an exception while signing'))
            finally:
                if e:
                    e.set()
        log.debug("Worker exiting")


class SigningServer:
    signer = None

    def __init__(self, config, passphrases):
        self.passphrases = passphrases
        ##
        # Stats
        ##
        # How many successful GETs have we had?
        self.hits = 0
        # How many unsuccesful GETs have we had? (not counting pending jobs)
        self.misses = 0
        # How many uploads have we had?
        self.uploads = 0

        # Mapping of file hashes to gevent Events
        self.pending = {}

        self.load_config(config)

        self.messages = queue.Queue()

        # Start our message handling loop
        self._message_loop_thread = gevent.spawn(self.process_messages)

        # Start our cleanup loop
        self._cleanup_loop_thead = gevent.spawn(self.cleanup_loop)

    def stop(self):
        self._message_loop_thread.kill()
        self._cleanup_loop_thead.kill()

    def load_config(self, config):
        from ConfigParser import NoOptionError

        # For generating new tokens
        self.token_secret = config.get('security', 'token_secret')
        # For validating existing tokens
        self.token_secrets = []
        for option, value in config.items('security'):
            if option.startswith('token_secret'):
                self.token_secrets.append(value)

        self.signed_dir = config.get('paths', 'signed_dir')
        self.unsigned_dir = config.get('paths', 'unsigned_dir')
        self.allowed_ips = [IP(i) for i in
                            config.get('security', 'allowed_ips').split(',')]
        self.new_token_allowed_ips = [IP(i) for i in
                                      config.get('security', 'new_token_allowed_ips').split(',')]
        self.allowed_filenames = [re.compile(e) for e in
                                  config.get('security', 'allowed_filenames').split(',')]
        self.min_filesize = config.getint('security', 'min_filesize')
        self.formats = [f.strip() for f in config.get('signing',
                                                      'formats').split(',')]
        self.max_filesize = dict()
        for f in self.formats:
            try:
                self.max_filesize[f] = config.getint(
                    'security', 'max_filesize_%s' % f)
            except NoOptionError:
                self.max_filesize[f] = None
        self.max_token_age = config.getint('security', 'max_token_age')
        self.max_file_age = config.getint('server', 'max_file_age')
        self.token_auths = []
        for option, value in config.items('security'):
            if option.startswith('new_token_auth'):
                self.token_auths.append(value)
        self.cleanup_interval = config.getint('server', 'cleanup_interval')

        for d in self.signed_dir, self.unsigned_dir:
            if not os.path.exists(d):
                log.info("Creating %s directory", d)
                os.makedirs(d)

        self.signer = Signer(self,
                             config.get('signing', 'signscript'),
                             config.get('paths', 'unsigned_dir'),
                             config.get('paths', 'signed_dir'),
                             config.getint('signing', 'concurrency'),
                             self.passphrases)

    def verify_token(self, token, slave_ip):
        token_data, token_sig = token.split('!', 1)

        # validate the signature, so we know token_data is reliable
        for secret in self.token_secrets:
            if verify_token(token_data, token_sig, secret):
                break
        else:
            return False

        info = unpack_token_data(token_data)
        valid_from, valid_to = info['valid_from'], info['valid_to']
        now = time.time()
        if now < valid_from or now > valid_to:
            log.info("Invalid time window")
            return False

        if info['slave_ip'] != slave_ip:
            log.info("Invalid slave ip")
            return False

        return True

    def get_token(self, slave_ip, duration):
        duration = int(duration)
        if not 0 < duration <= self.max_token_age:
            log.debug("invalid duration")
            raise ValueError("Invalid duration: %s", duration)
        now = int(time.time())
        valid_from = now
        valid_to = now + duration
        log.info("request for token for slave %s for %i seconds",
                 slave_ip, duration)
        data = make_token_data(slave_ip, valid_from, valid_to)
        token = data + '!' + sign_data(data, self.token_secret)
        return token

    def cleanup_loop(self):
        try:
            self.cleanup()
        except:
            log.exception("Error cleaning up")
        finally:
            gevent.spawn_later(
                self.cleanup_interval,
                self.cleanup_loop,
            )

    def cleanup(self):
        log.info("Stats: %i hits; %i misses; %i uploads",
                 self.hits, self.misses, self.uploads)
        log.debug("Pending: %s", self.pending)
        # Find files in unsigned that have bad hashes and delete them
        log.debug("Cleaning up...")
        now = time.time()
        for f in os.listdir(self.unsigned_dir):
            unsigned = os.path.join(self.unsigned_dir, f)
            # Clean up old files
            if os.path.getmtime(unsigned) < now - self.max_file_age:
                log.info("Deleting %s (too old)", unsigned)
                safe_unlink(unsigned)
                continue

        # Find files in signed that don't have corresponding files in unsigned
        # and delete them
        for format_ in os.listdir(self.signed_dir):
            for f in os.listdir(os.path.join(self.signed_dir, format_)):
                signed = os.path.join(self.signed_dir, format_, f)
                unsigned = os.path.join(self.unsigned_dir, f)
                if not os.path.exists(unsigned):
                    log.info("Deleting %s with no unsigned file", signed)
                    safe_unlink(signed)

    def submit_file(self, filehash, filename, format_):
        assert (filehash, format_) not in self.pending
        e = self.signer.signfile(filehash, filename, format_)
        self.pending[(filehash, format_)] = e

    def process_messages(self):
        while True:
            msg = self.messages.get()
            log.debug("Got message: %s", msg)
            try:
                if msg[0] == 'errors':
                    item, txt = msg[1:]
                    filehash, filename, format_, e = item
                    del self.pending[filehash, format_]
                elif msg[0] == 'done':
                    item, outputhash = msg[1:]
                    filehash, filename, format_, e = item
                    del self.pending[filehash, format_]
                    # Remember the filename for the output file too
                    self.save_filename(outputhash, filename)
                else:
                    log.error("Unknown message type: %s", msg)
            except:
                log.exception("Error handling message: %s", msg)

    def get_path(self, filename, format_):
        # Return path of filename under signed-files
        return os.path.join(self.signed_dir, format_, os.path.basename(filename))

    def get_filename(self, filehash):
        try:
            filename_fn = os.path.join(self.unsigned_dir, filehash + ".fn")
            os.utime(filename_fn, None)
            return open(filename_fn, 'rb').read()
        except OSError:
            return None

    def save_filename(self, filehash, filename):
        filename_fn = os.path.join(self.unsigned_dir, filehash + ".fn")
        fd, tmpname = tempfile.mkstemp(dir=self.unsigned_dir)
        fp = os.fdopen(fd, 'wb')
        fp.write(filename)
        fp.close()
        os.rename(tmpname, filename_fn)

    def __call__(self, environ, start_response):
        """WSGI entry point."""

        # Validate the client's IP
        remote_addr = environ['REMOTE_ADDR']
        if not any(remote_addr in net for net in self.allowed_ips):
            log.info("%(REMOTE_ADDR)s forbidden based on IP address" % environ)
            start_response("403 Forbidden", [])
            return ""

        method = getattr(self, 'do_%s' % environ['REQUEST_METHOD'], None)
        if not method:
            start_response("405 Method Not Allowed", [])
            return ""

        log.info("%(REMOTE_ADDR)s %(REQUEST_METHOD)s %(PATH_INFO)s" % environ)
        return method(environ, start_response)

    def do_GET(self, environ, start_response):
        """
        GET /sign/<format>/<hash>
        """
        try:
            _, magic, format_, filehash = environ['PATH_INFO'].split('/')
            assert magic == 'sign'
            assert format_ in self.formats
        except:
            log.debug("bad request: %s", environ['PATH_INFO'])
            start_response("400 Bad Request", [])
            yield ""
            return

        filehash = os.path.basename(environ['PATH_INFO'])
        try:
            pending = self.pending.get((filehash, format_))
            if pending:
                log.debug("Waiting for pending job")
                # Wait up to a minute for this to finish
                pending.wait(timeout=60)
                log.debug("Pending job finished!")
            fn = self.get_path(filehash, format_)
            filename = self.get_filename(filehash)
            if filename:
                log.debug("Looking for %s (%s)", fn, filename)
            else:
                log.debug("Looking for %s", fn)
            checksum = sha1sum(fn)
            headers = [
                ('X-SHA1-Digest', checksum),
                ('Content-Length', os.path.getsize(fn)),
            ]
            fp = open(fn, 'rb')
            os.utime(fn, None)
            log.debug("%s is OK", fn)
            start_response("200 OK", headers)
            while True:
                data = fp.read(1024 ** 2)
                if not data:
                    break
                yield data
            self.hits += 1
        except IOError:
            log.debug("%s is missing", fn)
            headers = []
            fn = os.path.join(self.unsigned_dir, filehash)
            if (filehash, format_) in self.pending:
                log.info("File is pending, come back soon!")
                log.debug("Pending: %s", self.pending)
                headers.append(('X-Pending', 'True'))

            # Maybe we have the file, but not for this format
            # If so, queue it up and return a pending response
            # This prevents the client from having to upload the file again
            elif os.path.exists(fn):
                log.debug("GET for file we already have, but not for the right format")
                # Validate the file
                myhash = sha1sum(fn)
                if myhash != filehash:
                    log.warning("%s is corrupt; deleting (%s != %s)",
                                fn, filehash, myhash)
                    safe_unlink(fn)
                else:
                    filename = self.get_filename(filehash)
                    if filename:
                        self.submit_file(filehash, filename, format_)
                        log.info("File is pending, come back soon!")
                        headers.append(('X-Pending', 'True'))
                    else:
                        log.debug("I don't remember the filename; re-submit please!")
            else:
                self.misses += 1

            start_response("404 Not Found", headers)
            yield ""

    def handle_upload(self, environ, start_response, values, rest, next_nonce):
        format_ = rest[0]
        assert format_ in self.formats
        filehash = values['sha1']
        filename = values['filename']
        log.info("Request to %s sign %s (%s) from %s", format_,
                 filename, filehash, environ['REMOTE_ADDR'])
        fn = os.path.join(self.unsigned_dir, filehash)
        headers = [('X-Nonce', next_nonce)]
        if os.path.exists(fn):
            # Validate the file
            mydigest = sha1sum(fn)

            if mydigest != filehash:
                log.warning("%s is corrupt; deleting (%s != %s)",
                            fn, mydigest, filehash)
                safe_unlink(fn)

            elif os.path.exists(os.path.join(self.signed_dir, filehash)):
                # Everything looks ok
                log.info("File already exists")
                start_response("202 File already exists", headers)
                return ""

            elif (filehash, format_) in self.pending:
                log.info("File is pending")
                start_response("202 File is pending", headers)
                return ""

            log.info("Not pending or already signed, re-queue")

        # Validate filename
        if not any(exp.match(filename) for exp in self.allowed_filenames):
            log.info("%s forbidden due to invalid filename: %s",
                     environ['REMOTE_ADDR'], filename)
            start_response("403 Unacceptable filename", headers)
            return ""

        try:
            fd, tmpname = tempfile.mkstemp(dir=self.unsigned_dir)
            fp = os.fdopen(fd, 'wb')

            h = hashlib.new('sha1')
            s = 0
            while True:
                data = values['filedata'].file.read(1024 ** 2)
                if not data:
                    break
                s += len(data)
                h.update(data)
                fp.write(data)
            fp.close()
        except:
            log.exception("Error downloading data")
            if os.path.exists(tmpname):
                os.unlink(tmpname)

        if s < self.min_filesize:
            if os.path.exists(tmpname):
                os.unlink(tmpname)
            start_response("400 File too small", headers)
            return ""
        if self.max_filesize[format_] and s > self.max_filesize[format_]:
            if os.path.exists(tmpname):
                os.unlink(tmpname)
            start_response("400 File too large", headers)
            return ""

        if h.hexdigest() != filehash:
            if os.path.exists(tmpname):
                os.unlink(tmpname)
            log.warn("Hash mismatch. Bad upload?")
            start_response("400 Hash mismatch", headers)
            return ""

        # Good to go!  Rename the temporary filename to the real filename
        self.save_filename(filehash, filename)
        os.rename(tmpname, fn)
        self.submit_file(filehash, filename, format_)
        start_response("202 Accepted", headers)
        self.uploads += 1
        return ""

    def handle_token(self, environ, start_response, values):
        token = self.get_token(
            values['slave_ip'],
            values['duration'],
        )
        start_response("200 OK", [])
        return token

    def do_POST(self, environ, start_response):
        req = webob.Request(environ)
        values = req.POST
        headers = []

        try:
            path_bits = environ['PATH_INFO'].split('/')
            magic = path_bits[1]
            rest = path_bits[2:]
            if not magic in ('sign', 'token'):
                log.exception("bad request: %s", environ['PATH_INFO'])
                start_response("400 Bad Request", [])
                return ""

            if magic == 'token':
                remote_addr = environ['REMOTE_ADDR']
                if not any(remote_addr in net for net in self.new_token_allowed_ips):
                    log.info("%(REMOTE_ADDR)s forbidden based on IP address" %
                             environ)
                    start_response("403 Forbidden", [])
                    return ""
                try:
                    basic, auth = req.headers['Authorization'].split()
                    if basic != "Basic" or auth.decode("base64") not in self.token_auths:
                        start_response("401 Authorization Required", [])
                        return ""
                except:
                    start_response("401 Authorization Required", [])
                    return ""

                return self.handle_token(environ, start_response, values)
            elif magic == 'sign':
                # Validate token
                if 'token' not in values:
                    start_response("400 Missing token", [])
                    return ""

                slave_ip = environ['REMOTE_ADDR']
                if not self.verify_token(values['token'], slave_ip):
                    log.warn("Bad token")
                    start_response("400 Invalid token", [])
                    return ""

                # nonces are unused, but still part of the protocol
                next_nonce = 'UNUSED'
                headers.append(('X-Nonce', 'UNUSED'))

                return self.handle_upload(environ, start_response, values, rest, next_nonce)
        except:
            log.exception("ISE")
            start_response("500 Internal Server Error", headers)
            return ""


def create_server(app, listener, config):
    # Simple wrapper so pywsgi uses logging to log instead of writing to stderr
    class logger(object):
        def write(self, msg):
            log.info(msg)

    server = pywsgi.WSGIServer(
        listener,
        app,
        certfile=config.get('security', 'public_ssl_cert'),
        keyfile=config.get('security', 'private_ssl_cert'),
        log=logger(),
    )
    return server
