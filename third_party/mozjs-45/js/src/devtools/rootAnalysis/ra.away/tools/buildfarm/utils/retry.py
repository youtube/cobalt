#!/usr/bin/env python
"""retry.py [options] cmd [arg arg1 ...]

Retries cmd and args with configurable timeouts and delays between retries.

Return code is 0 if cmd succeeds, and is 1 if cmd fails after all the retries.
"""

import time
import subprocess
import logging
import os
import sys
import re
from tempfile import TemporaryFile
from operator import xor
log = logging.getLogger()

sys.path.append(os.path.join(os.path.dirname(__file__), "../../lib/python"))

from util.retry import retry

if sys.platform.startswith('win'):
    from win32_util import kill, which
else:
    from unix_util import kill


def read_file(f):
    f.seek(0)
    return f.read()


def search_output(f, regexp, fail_if_match):
    res = re.search(regexp, read_file(f))
    return xor(bool(res), fail_if_match)


class RunWithTimeoutException(Exception):
    def __init__(self, rc, **kwargs):
        Exception.__init__(self, **kwargs)
        self.rc = rc


def run_with_timeout(cmd, timeout, stdout_regexp=None, stderr_regexp=None,
                     fail_if_match=False, print_output=True):
    if stdout_regexp:
        stdout = TemporaryFile()
    else:
        stdout = None
    if stderr_regexp:
        stderr = TemporaryFile()
    else:
        stderr = None
    proc = subprocess.Popen(cmd, stdout=stdout, stderr=stderr)
    start_time = time.time()
    log.info("Executing: %s", cmd)
    while True:
        rc = proc.poll()
        if rc is not None:
            log.debug("Process returned %s", rc)
            if print_output and stdout:
                print "Process stdio:\n%s" % read_file(stdout)
            if print_output and stderr:
                print "Process stderr:\n%s" % read_file(stderr)
            if rc == 0:
                if stdout_regexp and not \
                        search_output(stdout, stdout_regexp, fail_if_match):
                    raise RunWithTimeoutException(
                        "%s found in stdout, failing" % fail_if_match, -1)
                if stderr_regexp and not \
                        search_output(stderr, stderr_regexp, fail_if_match):
                    raise RunWithTimeoutException(
                        "%s found in stderr, failing" % fail_if_match, -1)
                return rc
            else:
                raise RunWithTimeoutException("command exited with non-zero return code %d, failing" % rc, rc)

        if start_time + timeout < time.time():
            log.warn("WARNING: Timeout (%i) exceeded, killing process %i",
                     timeout, proc.pid)
            rc = kill(proc.pid)
            log.debug("Process returned %s", rc)
            raise RunWithTimeoutException(
                "Timeout (%i) exceeded" % timeout, rc)

        # Check again in a sec...
        time.sleep(0.25)

if __name__ == "__main__":
    from optparse import OptionParser

    parser = OptionParser(__doc__)
    parser.disable_interspersed_args()

    parser.add_option("-r", "--retries", type="int", dest="retries",
                      help="""retry this many times.  Set 0 to retry forever.
                      Defaults to 10""")
    parser.add_option("-t", "--timeout", type="int", dest="timeout",
                      help="""timeout each request after this many seconds.  Set
                      to 0 to have no timeout.  Defaults to 300""")
    parser.add_option("-s", "--sleeptime", type="int", dest="sleeptime",
                      help="""sleep this many seconds between tries, doubling
                      each retry iteration.  Defaults to 30""")
    parser.add_option("-m", "--maxsleeptime", type="int", dest="maxsleeptime",
                      help="""when doubling sleeptime, do not exceed this value.
                      Defaults to 300""")
    parser.add_option("--stdout-regexp", dest="stdout_regexp",
                      help="""Fail if the expected regexp is not found in
                      stdout""")
    parser.add_option("--stderr-regexp", dest="stderr_regexp",
                      help="""Fail if the expected regexp is not found in
                      stderr""")
    parser.add_option("--fail-if-match", dest="fail_if_match",
                      action="store_true", default=False, help="""Reverse the
                      meaning of stderr-regexp/stdout-regexp and fail if the
                      expected regexp is NOT found in the output""")
    parser.add_option("--no-output", dest="print_output",
                      action="store_false", default=True,
                      help="Don't print stdout/stderr output")

    parser.set_defaults(
        retries=10,
        timeout=300,
        sleeptime=30,
        maxsleeptime=300,
    )

    options, args = parser.parse_args()

    if len(args) == 0:
        parser.error("Command argument missing")

    logging.basicConfig(level=logging.INFO, format="%(message)s")
    # Another dirty hack... :(
    # Special care for programs w/o extensions on Windows
    if sys.platform.startswith('win') and not os.path.splitext(args[0])[1]:
        args[0] = which(args[0])

    try:
        rc = retry(run_with_timeout, attempts=options.retries,
                   sleeptime=options.sleeptime, max_sleeptime=options.maxsleeptime,
                   args=(args, options.timeout, options.stdout_regexp,
                         options.stderr_regexp, options.fail_if_match,
                         options.print_output))
        sys.exit(rc)
    except KeyboardInterrupt:
        sys.exit(1)
    except Exception, e:
        log.info("Unable to successfully run %s after %d attempts" %
                (args, options.retries))
        # If we caught a RunWithTimeoutException we can exit with the same
        # rc as the command. If something else was hit, just exit with 1
        rc = getattr(e, 'rc', 1)
        sys.exit(rc)
