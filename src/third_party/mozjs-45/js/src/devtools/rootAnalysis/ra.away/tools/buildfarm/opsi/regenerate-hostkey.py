#!/usr/bin/python

from ConfigParser import ConfigParser, NoOptionError, NoSectionError
from datetime import datetime
from md5 import md5
from os import path
import sys
from socket import gethostname

KEY_FILE = path.join("c:\\", "program files", "opsi.org", "preloginloader",
                     "cfg", "locked.cfg")
LOG_FILE = path.join("c:\\", "tmp", "key-generator.log")
REF_PLATFORMS = {'build': 'win2k3-ref-img',
                 'build-ix': 'win32-ix-ref',
                 'talos-xp': 'talos-r3-xp-ref',
                 'talos-win7': 'talos-r3-w7-ref'}


def generate_hash(str):
    return md5(str).hexdigest()


def get_ref_platform_key(hostname):
    if 'talos' in hostname:
        if 'xp' in hostname:
            return generate_hash(REF_PLATFORMS['talos-xp'])
        elif 'w7' in hostname:
            return generate_hash(REF_PLATFORMS['talos-win7'])
        else:
            return None
    else:
        if 'ix' in hostname:
            return generate_hash(REF_PLATFORMS['build-ix'])
        else:
            return generate_hash(REF_PLATFORMS['build'])


def write_new_key(host, cp, keyfile):
    newKey = generate_hash(host)
    if not cp.has_section('shareinfo'):
        cp.add_section('shareinfo')
    try:
        cp.set('shareinfo', 'pckey', newKey)
        f = open(keyfile, 'w')
        cp.write(f)
        f.close()
    except IOError, e:
        raise IOError("Error when writing key to %s: \n%s" % (keyfile, e))


def log(msg):
    print '%s - %s' % (datetime.strftime(datetime.now(), "%Y-%m-%d %H:%M"), msg)

if __name__ == '__main__':
    from optparse import OptionParser
    parser = OptionParser()
    parser.add_option("-k", "--key-file", action="store", dest="keyfile",
                      default=KEY_FILE)
    parser.add_option("-l", "--log-file", action="store", dest="logfile",
                      default=LOG_FILE)
    (options, args) = parser.parse_args()

    # Set up logging
    try:
        sys.stdout = open(options.logfile, "a")
    except IOError:
        log("WARN: Couldn't open %s, logging to STDOUT instead" %
            options.logfile)

    # Gather information before evaluating what to do
    hostname = gethostname()
    refKey = get_ref_platform_key(hostname)
    currentKey = None
    cp = ConfigParser()
    try:
        cp.read(options.keyfile)
        currentKey = cp.get('shareinfo', 'pckey')
    except (IOError, NoSectionError, NoOptionError):
        pass

    # Don't do anything for ref platforms
    if hostname in REF_PLATFORMS:
        log("WARN: %s is a ref platform, not doing anything..." % hostname)
        sys.exit(0)

    # If the currentKey doesn't match a ref platform key we assume the slave
    # isn't a ref platform and is already working correctly with OSPI.
    if currentKey and currentKey != refKey:
        # Don't log here because this process runs at every boot.
        sys.exit(0)

    # If we haven't hit any of the above conditions we assume a key needs to be
    # generated. Generally, this means the slave is freshly cloned from the
    # ref platform.
    log("Writing new key")
    try:
        write_new_key(hostname, cp, options.keyfile)
        log("New key written to %s" % options.keyfile)
    except IOError, e:
        log("Encountered error when writing new key: %s" % e)
