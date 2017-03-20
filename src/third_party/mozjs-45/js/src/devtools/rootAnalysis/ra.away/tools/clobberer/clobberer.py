#!/usr/bin/python
# vim:sts=4 sw=4
import sys
import shutil
import urllib2
import urllib
import json
import os
import time
import socket
import logging

log = logging.getLogger()

if os.name == 'nt':
    from win32file import RemoveDirectory, DeleteFile, \
        SetFileAttributesW, \
        FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_DIRECTORY
    from win32api import FindFiles

clobber_suffix = '.deleteme'


def get_hostname():
    return socket.gethostname().split('.')[0]


def ts_to_str(ts):
    if ts is None:
        return None
    return time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(ts))


def write_file(ts, fn):
    assert isinstance(ts, int)
    f = open(fn, "w")
    f.write(str(ts))
    f.close()


def read_file(fn):
    if not os.path.exists(fn):
        return None

    data = open(fn).read().strip()
    try:
        return int(data)
    except ValueError:
        return None


def safe_join(parent, path):
    """Returns $parent/$path
    Raises IOError in case the resulting path ends up outside of parent for
    some reason (e.g. by using ../../..)
    """
    while os.path.isabs(path):
        path = path.lstrip("/")
        drive, tail = os.path.splitdrive(path)
        path = tail
        path = os.path.normpath(path)
    if path.startswith("../"):
        raise IOError("unsafe join of %s/%s" % (parent, path))
    return os.path.join(parent, path)


def rmdir_recursive_windows(dir):
    """Windows-specific version of rmdir_recursive that handles
    path lengths longer than MAX_PATH.
    """

    dir = os.path.realpath(dir)
    # Make sure directory is writable
    SetFileAttributesW('\\\\?\\' + dir, FILE_ATTRIBUTE_NORMAL)

    for ffrec in FindFiles('\\\\?\\' + dir + '\\*.*'):
        file_attr = ffrec[0]
        name = ffrec[8].decode(sys.getfilesystemencoding())
        if name == '.' or name == '..':
            continue
        full_name = os.path.join(dir, name)

        if file_attr & FILE_ATTRIBUTE_DIRECTORY:
            rmdir_recursive_windows(full_name)
        else:
            SetFileAttributesW('\\\\?\\' + full_name, FILE_ATTRIBUTE_NORMAL)
            DeleteFile('\\\\?\\' + full_name)
    RemoveDirectory('\\\\?\\' + dir)


def rmdir_recursive(dir):
    """This is a replacement for shutil.rmtree that works better under
    windows. Thanks to Bear at the OSAF for the code.
    (Borrowed from buildbot.slave.commands)"""
    if os.name == 'nt':
        rmdir_recursive_windows(dir)
        return

    if not os.path.exists(dir):
        # This handles broken links
        if os.path.islink(dir):
            os.remove(dir)
        return

    if os.path.islink(dir):
        os.remove(dir)
        return

    # Verify the directory is read/write/execute for the current user
    os.chmod(dir, 0o700)

    for name in os.listdir(dir):
        full_name = os.path.join(dir, name)
        # on Windows, if we don't have write permission we can't remove
        # the file/directory either, so turn that on
        if os.name == 'nt':
            if not os.access(full_name, os.W_OK):
                # I think this is now redundant, but I don't have an NT
                # machine to test on, so I'm going to leave it in place
                # -warner
                os.chmod(full_name, 0o600)

        if os.path.isdir(full_name):
            rmdir_recursive(full_name)
        else:
            # Don't try to chmod links
            if not os.path.islink(full_name):
                os.chmod(full_name, 0o700)
            os.remove(full_name)
    os.rmdir(dir)


def do_clobber(dir, dryrun=False, skip=None):
    try:
        for f in os.listdir(dir):
            if skip is not None and f in skip:
                log.info("Skipping %s", f)
                continue
            clobber_path = f + clobber_suffix
            if os.path.isfile(f):
                log.info("Removing %s", f)
                if not dryrun:
                    if os.path.exists(clobber_path):
                        os.unlink(clobber_path)
                    # Prevent repeated moving.
                    if f.endswith(clobber_suffix):
                        os.unlink(f)
                    else:
                        shutil.move(f, clobber_path)
                        os.unlink(clobber_path)
            elif os.path.isdir(f):
                log.info("Removing %s/", f)
                if not dryrun:
                    if os.path.exists(clobber_path):
                        rmdir_recursive(clobber_path)
                    # Prevent repeated moving.
                    if f.endswith(clobber_suffix):
                        rmdir_recursive(f)
                    else:
                        shutil.move(f, clobber_path)
                        rmdir_recursive(clobber_path)
    except Exception as e:
        log.error("[clobber failed]: %s", e.message)
        sys.exit(1)


def get_clobber_times(clobber_url):
    log.info("Checking clobber URL: %s", clobber_url)
    data = urllib2.urlopen(clobber_url, timeout=30).read()
    return json.loads(data)


def legacy_get_clobber_times(clobber_url, branch, buildername, builddir, slave, master=None):
    params = dict(branch=branch, buildername=buildername,
                  builddir=builddir, slave=slave, master=master)
    url = "%s?%s" % (clobber_url, urllib.urlencode(params))
    # The timeout arg was added to urlopen() at Python 2.6
    # Deprecate this test when esr17 reaches EOL
    if sys.version_info[:2] < (2, 6):
        data = urllib2.urlopen(url).read().strip()
    else:
        data = urllib2.urlopen(url, timeout=30).read().strip()

    processed_data = data.strip().split(':')

    if len(processed_data) < 2:
        # We received an empty result, time to bail
        return {'result': []}

    builddir_result, lastclobber, who = processed_data
    lastclobber = int(lastclobber)
    # return a dictionary that mimicks the new endpoint
    # json format
    return {
        'result': [{
            'builddir': builddir_result,
            'lastclobber': lastclobber,
            'who': who,
            'slave': None
        }]
    }


def process_clobber_times(server_clobber_times, args):
    now = int(time.time())

    root_dir = os.path.abspath(args.dir)

    for clobber_time in server_clobber_times['result']:
        slave = clobber_time['slave']
        builddir = clobber_time['builddir']
        server_clobber_date = clobber_time['lastclobber']
        who = clobber_time['who']

        builder_dir = safe_join(root_dir, builddir)
        if not os.path.isdir(builder_dir):
            log.info("%s doesn't exist, skipping", builder_dir)
            continue
        os.chdir(builder_dir)

        our_clobber_date = read_file("last-clobber") or 0

        clobber = False
        clobber_type = None

        log.info("%s:Our last clobber date: %s", builddir, ts_to_str(our_clobber_date))
        log.info("%s:Server clobber date:   %s", builddir, ts_to_str(server_clobber_date))

        # If we don't have a last clobber date, then this is probably a fresh build.
        # We should only do a forced server clobber if we know when our last clobber
        # was, and if the server date is more recent than that.
        if server_clobber_date is not None and our_clobber_date is not None:
            # If the server is giving us a clobber date, compare the server's idea of
            # the clobber date to our last clobber date also skip slave specific
            # clobbers that aren't for the current host
            if (server_clobber_date > our_clobber_date) and (slave is None or slave == args.slave):
                # If the server's clobber date is greater than our last clobber date,
                # then we should clobber.
                clobber = True
                clobber_type = "forced"
                # We should also update our clobber date to match the server's
                our_clobber_date = server_clobber_date
                if who:
                    log.info("%s:Server is forcing a clobber, initiated by %s", builddir, who)
                else:
                    log.info("%s:Server is forcing a clobber", builddir)

        if not clobber:
            # Disable periodic clobbers for builders that aren't args.builddir
            if builddir != args.builddir:
                continue

            # Next, check if more than the periodic_clobber_time period has passed since
            # our last clobber
            if our_clobber_date is None:
                # We've never been clobbered
                # Set our last clobber time to now, so that we'll clobber
                # properly after periodic_clobber_time
                clobber_type = "purged"
                our_clobber_date = now
                write_file(our_clobber_date, "last-clobber")
            elif args.periodic_clobber_time and now > our_clobber_date + args.periodic_clobber_time:
                # periodic_clobber_time has passed since our last clobber
                clobber = True
                clobber_type = "periodic"
                # Update our clobber date to now
                our_clobber_date = now
                log.info("%s:More than %s seconds have passed since our last clobber",
                         (builddir, args.periodic_clobber_time))

        if clobber:
            # Finally, perform a clobber if we're supposed to
            log.info("%s:Clobbering...", builddir)
            do_clobber(builder_dir, args.dryrun, args.skip)
            write_file(our_clobber_date, "last-clobber")

        # If this is the build dir for the current job, display the clobber type in TBPL.
        # Note in the case of purged clobber, we output the clobber type even though no
        # clobber was performed this time.
        if clobber_type and builddir == args.builddir:
            log.info("Tinderboxlog.info: %s clobber" % clobber_type)


def make_argparser():
    import optparse
    parser = optparse.OptionParser(
        "%prog [options] clobberURL branch buildername builddir slave master"
    )
    parser.add_option("-n", "--dry-run", dest="dryrun", action="store_true",
                      default=False, help="don't actually delete anything")
    parser.add_option("-t", "--periodic", dest="period", type=float,
                      default=None, help="hours between periodic clobbers")
    parser.add_option('-s', '--skip', help='do not delete this file/directory',
                      action='append', dest='skip', default=['last-clobber'])
    parser.add_option('-d', '--dir', help='clobber this directory',
                      dest='dir', default='.')
    parser.add_option('--builddir', help='a specific builddir for periodic clobbers')
    parser.add_option('-v', '--verbose', help='be more verbose',
                      dest='loglevel', action='store_const',
                      default=logging.INFO, const=logging.DEBUG)
    parser.add_option('--url', dest='clobber_url', help='URL to clobberer service')
    parser.add_option('--slave', default=get_hostname(),
                      help='Name of the slave being clobbered (defaults to hostname)')
    return parser


def main(args, legacy=None):
    if args.period:
        args.periodic_clobber_time = args.period * 3600
    else:
        args.periodic_clobber_time = None
    # Legacy support
    if legacy and len(legacy) > 1:
        log.info("Entering legacy mode.")
        server_clobber_times = legacy_get_clobber_times(*legacy)
    elif not args.clobber_url:
        # we can't set required=True because of legacy support
        raise Exception("--url required")
    else:
        server_clobber_times = get_clobber_times(args.clobber_url)

    process_clobber_times(server_clobber_times, args)

if __name__ == "__main__":
    parser = make_argparser()
    parsed_args, legacy = parser.parse_args()
    logging.basicConfig(level=parsed_args.loglevel, format="%(asctime)s - %(message)s")
    main(parsed_args, legacy=legacy)
