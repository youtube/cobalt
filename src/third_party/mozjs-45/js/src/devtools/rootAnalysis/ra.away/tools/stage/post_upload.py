#!/usr/bin/python

### The canonical location for this file is
###   https://hg.mozilla.org/build/tools/file/default/stage/post_upload.py
###
### Please update the copy in puppet to deploy new changes to
### stage.mozilla.org, see
# https://wiki.mozilla.org/ReleaseEngineering/How_To/Modify_scripts_on_stage

# This script expects a directory as its first non-option argument,
# followed by a list of filenames.

import calendar
import sys
import os
import os.path
import pytz
import shutil
import re
import tempfile
from datetime import datetime
from optparse import OptionParser
from errno import EEXIST
from ConfigParser import RawConfigParser

# Lets read in the config files.  Because this application is
# run once for every upload, we just read the config file once at the beginning
# of execution.  If this was a longer running app, we'd need to do something
# to pick up changes in the config file.
config = RawConfigParser()
config.read(['post_upload.ini', os.path.expanduser('~/.post_upload.ini'),
            '/etc/post_upload.ini'])


# Read in paths that are valid on the stage server
CANDIDATES_PATH = config.get('paths', 'candidates_path')
NIGHTLY_PATH = config.get('paths', 'nightly')
TINDERBOX_BUILDS_PATH = config.get('paths', 'tinderbox_builds')
LONG_DATED_DIR = config.get('paths', 'long_dated')
SHORT_DATED_DIR = config.get('paths', 'short_dated')
CANDIDATES_BASE_DIR = config.get('paths', 'candidates_base')
CANDIDATES_DIR = CANDIDATES_BASE_DIR + config.get('paths', 'candidates')
LATEST_DIR = config.get('paths', 'latest')
TRY_DIR = config.get('paths', 'try')
PVT_BUILD_DIR = config.get('paths', 'pvt_builds')

# Read in the URLs that are served by this stage
TINDERBOX_URL_PATH = config.get('urls', 'tinderbox_builds')
LONG_DATED_URL_PATH = config.get('urls', 'long_dated')
CANDIDATES_URL_PATH = config.get('urls', 'candidates')
PVT_BUILD_URL_PATH = config.get('urls', 'pvt_builds')
TRY_URL_PATH = config.get('urls', 'try')

PARTIAL_MAR_RE = re.compile(config.get('patterns', 'partial_mar'))


# Cache of original_file to new location on disk
_linkCache = {}


def CopyFileToDir(original_file, source_dir, dest_dir, preserve_dirs=False):
    """ Atomically copy original_file from source_dir into dest_dir,
    overwriting old files and preserving directory hierarchy if preserve_dirs
    is True """
    if not original_file.startswith(source_dir):
        print "%s is not in %s!" % (original_file, source_dir)
        return
    relative_path = os.path.basename(original_file)
    if preserve_dirs:
        # Add any dirs below source_dir to the final destination
        filePath = original_file.replace(source_dir, "").lstrip("/")
        filePath = os.path.dirname(filePath)
        dest_dir = os.path.join(dest_dir, filePath)
    new_file = os.path.join(dest_dir, relative_path)
    full_dest_dir = os.path.dirname(new_file)
    if not os.path.isdir(full_dest_dir):
        try:
            os.makedirs(full_dest_dir, 0755)
        except OSError, e:
            if e.errno == EEXIST:
                print "%s already exists, continuing anyways" % full_dest_dir
            else:
                raise
    if os.path.exists(new_file):
        try:
            os.unlink(new_file)
        except OSError, e:
            # If the file gets deleted by another instance of post_upload
            # because there was a name collision this improves the situation
            # as to not abort the process but continue with the next file
            print "Warning: The file %s has already been unlinked by " % new_file + \
                  "another instance of post_upload.py"
            return

    # Try hard linking the file
    if original_file in _linkCache:
        for src in _linkCache[original_file]:
            try:
                os.link(src, new_file)
                os.chmod(new_file, 0644)
                return
            except OSError:
                pass

    tmp_fd, tmp_path = tempfile.mkstemp(dir=dest_dir)
    tmp_fp = os.fdopen(tmp_fd, 'wb')
    shutil.copyfileobj(open(original_file, 'rb'), tmp_fp)
    tmp_fp.close()
    os.chmod(tmp_path, 0644)
    os.rename(tmp_path, new_file)
    _linkCache.setdefault(original_file, []).append(new_file)


def BuildIDToDict(buildid):
    """Returns an dict with the year, month, day, hour, minute, and second
       as keys, as parsed from the buildid"""
    buildidDict = {}
    try:
        # strptime is no good here because it strips leading zeros
        buildidDict['year'] = buildid[0:4]
        buildidDict['month'] = buildid[4:6]
        buildidDict['day'] = buildid[6:8]
        buildidDict['hour'] = buildid[8:10]
        buildidDict['minute'] = buildid[10:12]
        buildidDict['second'] = buildid[12:14]
    except:
        raise "Could not parse buildid!"
    return buildidDict


def BuildIDToUnixTime(buildid):
    """Returns the timestamp the buildid represents in unix time."""
    try:
        pt = pytz.timezone('US/Pacific')
        return calendar.timegm(pt.localize(datetime.strptime(buildid, "%Y%m%d%H%M%S")).utctimetuple())
    except:
        raise "Could not parse buildid!"


def ReleaseToDated(options, upload_dir, files):
    values = BuildIDToDict(options.buildid)
    values['branch'] = options.branch
    values['product'] = options.product
    values['nightly_dir'] = options.nightly_dir
    longDir = LONG_DATED_DIR % values
    shortDir = SHORT_DATED_DIR % values
    url = LONG_DATED_URL_PATH % values

    longDatedPath = os.path.join(NIGHTLY_PATH, longDir)
    shortDatedPath = os.path.join(NIGHTLY_PATH, shortDir)

    if options.builddir:
        longDatedPath += "/%s" % options.builddir
        shortDatedPath += "/%s" % options.builddir

    for f in files:
        if options.branch.endswith('l10n') and f.endswith('.xpi'):
            CopyFileToDir(f, upload_dir, longDatedPath, preserve_dirs=True)
            filePath = f.replace(upload_dir, "").lstrip("/")
            filePath = os.path.dirname(filePath)
            sys.stderr.write(
                "%s\n" % os.path.join(url, filePath, os.path.basename(f)))
        else:
            CopyFileToDir(f, upload_dir, longDatedPath)
            sys.stderr.write("%s\n" % os.path.join(url, os.path.basename(f)))
    os.utime(longDatedPath, None)

    if not options.noshort:
        try:
            cwd = os.getcwd()
            os.chdir(NIGHTLY_PATH)
            if not os.path.exists(shortDir):
                os.symlink(longDir, shortDir)
        finally:
            os.chdir(cwd)


def ReleaseToLatest(options, upload_dir, files):
    latestDir = LATEST_DIR % {'branch': options.branch}
    latestPath = os.path.join(NIGHTLY_PATH, latestDir)

    if options.builddir:
        latestDir += "/%s" % options.builddir
        latestPath += "/%s" % options.builddir

    marToolsPath = "%s/mar-tools" % latestPath

    for f in files:
        filename = os.path.basename(f)
        if f.endswith('crashreporter-symbols.zip'):
            continue
        if PARTIAL_MAR_RE.search(f):
            continue
        if options.branch.endswith('l10n') and f.endswith('.xpi'):
            CopyFileToDir(f, upload_dir, latestPath, preserve_dirs=True)
        elif filename in ('mar', 'mar.exe', 'mbsdiff', 'mbsdiff.exe'):
            if options.tinderbox_builds_dir:
                platform = options.tinderbox_builds_dir.split('-')[-1]
                if platform in ('win32', 'macosx64', 'linux', 'linux64', 'win64'):
                    CopyFileToDir(f, upload_dir, '%s/%s' % (marToolsPath, platform))
        else:
            CopyFileToDir(f, upload_dir, latestPath)
    os.utime(latestPath, None)


def ReleaseToBuildDir(builds_dir, builds_url, options, upload_dir, files, dated):
    tinderboxBuildsPath = builds_dir % \
        {'product': options.product,
         'tinderbox_builds_dir': options.tinderbox_builds_dir}
    tinderboxUrl = builds_url % \
        {'product': options.product,
         'tinderbox_builds_dir': options.tinderbox_builds_dir}
    if dated:
        buildid = str(BuildIDToUnixTime(options.buildid))
        tinderboxBuildsPath = os.path.join(tinderboxBuildsPath, buildid)
        tinderboxUrl = os.path.join(tinderboxUrl, buildid)
        _to = os.path.basename(tinderboxBuildsPath)
        _from = os.path.join(os.path.dirname(tinderboxBuildsPath), "latest")
    if options.builddir:
        tinderboxBuildsPath = os.path.join(
            tinderboxBuildsPath, options.builddir)
        tinderboxUrl = os.path.join(tinderboxUrl, options.builddir)

    for f in files:
        # Reject MAR files. They don't belong here.
        if f.endswith('.mar'):
            continue
        if options.tinderbox_builds_dir.endswith('l10n') and f.endswith('.xpi'):
            CopyFileToDir(
                f, upload_dir, tinderboxBuildsPath, preserve_dirs=True)
            filePath = f.replace(upload_dir, "").lstrip("/")
            filePath = os.path.dirname(filePath)
            sys.stderr.write("%s\n" % os.path.join(
                tinderboxUrl, filePath, os.path.basename(f)))
        else:
            CopyFileToDir(f, upload_dir, tinderboxBuildsPath)
            sys.stderr.write(
                "%s\n" % os.path.join(tinderboxUrl, os.path.basename(f)))
    os.utime(tinderboxBuildsPath, None)
    # create latest softlink?
    if dated and options.release_to_latest_tinderbox_builds:
        # best effort softlink
        print >> sys.stderr, "ln -sfnv %s %s" % (_to, _from)
        os.system('ln -sfnv "%s" "%s"' % (_to, _from))


def ReleaseToTinderboxBuilds(options, upload_dir, files, dated=True):
    ReleaseToBuildDir(TINDERBOX_BUILDS_PATH, TINDERBOX_URL_PATH,
                      options, upload_dir, files, dated)


def ReleaseToTinderboxBuildsOverwrite(options, upload_dir, files):
    ReleaseToTinderboxBuilds(options, upload_dir, files, dated=False)


def rel_symlink(_to, _from):
    _to = os.path.realpath(_to)
    _from = os.path.realpath(_from)
    (_from_path, dummy) = os.path.split(_from)
    _to = os.path.relpath(_to, _from_path)
    os.symlink(_to, _from)


def symlink_nightly_to_candidates(nightly_path, candidates_full_path, version):
    _from = ("%(nightly_path)s/" + CANDIDATES_BASE_DIR) % {
        'nightly_path': nightly_path, 'version': version}
    _to = candidates_full_path
    if not os.path.isdir(_to):
        print >> sys.stderr, "mkdir %s" % _to
        try:
            os.mkdir(_to)
        except OSError, e:
            if e.errno == EEXIST:
                print "%s already exists, continuing anyways" % _to
            else:
                raise
    if not os.path.islink(_from):
        print >> sys.stderr, "ln -s %s %s" % (_to, _from)
        try:
            rel_symlink(_to, _from)
        except OSError, e:
            if e.errno == EEXIST:
                print "%s already exists, continuing anyways" % _from
                pass
            else:
                raise


def ReleaseToCandidatesDir(options, upload_dir, files):
    candidatesFullPath = CANDIDATES_BASE_DIR % {'version': options.version}
    candidatesFullPath = os.path.join(CANDIDATES_PATH, candidatesFullPath)
    candidatesDir = CANDIDATES_DIR % {'version': options.version,
                                      'buildnumber': options.build_number}
    candidatesPath = os.path.join(NIGHTLY_PATH, candidatesDir)
    candidatesUrl = CANDIDATES_URL_PATH % {
        'nightly_dir': options.nightly_dir,
        'version': options.version,
        'buildnumber': options.build_number,
        'product': options.product,
    }

    marToolsPath = "%s/mar-tools" % candidatesPath

    symlink_nightly_to_candidates(
        NIGHTLY_PATH, candidatesFullPath, options.version)

    for f in files:
        realCandidatesPath = candidatesPath
        filename = os.path.basename(f)
        if not options.signed and 'win32' in f and '/logs/' not in f:
            realCandidatesPath = os.path.join(realCandidatesPath, 'unsigned')
            url = os.path.join(candidatesUrl, 'unsigned')
        else:
            url = candidatesUrl
        if options.builddir:
            realCandidatesPath = os.path.join(
                realCandidatesPath, options.builddir)
            url = os.path.join(url, options.builddir)
        if filename in ('mar', 'mar.exe', 'mbsdiff', 'mbsdiff.exe'):
            if options.tinderbox_builds_dir:
                platform = options.tinderbox_builds_dir.split('-')[-1]
                if platform in ('win32', 'macosx64', 'linux', 'linux64', 'win64'):
                    CopyFileToDir(f, upload_dir, '%s/%s' % (marToolsPath, platform))
        else:
            CopyFileToDir(f, upload_dir, realCandidatesPath, preserve_dirs=True)
        # Output the URL to the candidate build
        if f.startswith(upload_dir):
            relpath = f[len(upload_dir):].lstrip("/")
        else:
            relpath = f.lstrip("/")

        sys.stderr.write("%s\n" % os.path.join(url, relpath))
        # We always want release files chmod'ed this way so other users in
        # the group cannot overwrite them.
        os.chmod(f, 0644)


def ReleaseToMobileCandidatesDir(options, upload_dir, files):
    candidatesDir = CANDIDATES_DIR % {'version': options.version,
                                      'buildnumber': options.build_number}
    candidatesPath = os.path.join(NIGHTLY_PATH, candidatesDir)
    candidatesUrl = CANDIDATES_URL_PATH % {
        'nightly_dir': options.nightly_dir,
        'version': options.version,
        'buildnumber': options.build_number,
        'product': options.product,
    }

    for f in files:
        realCandidatesPath = candidatesPath
        realCandidatesPath = os.path.join(realCandidatesPath,
                                          options.builddir)
        url = os.path.join(candidatesUrl, options.builddir)
        CopyFileToDir(f, upload_dir, realCandidatesPath, preserve_dirs=True)
        # Output the URL to the candidate build
        if f.startswith(upload_dir):
            relpath = f[len(upload_dir):].lstrip("/")
        else:
            relpath = f.lstrip("/")

        sys.stderr.write("%s\n" % os.path.join(url, relpath))
        # We always want release files chmod'ed this way so other users in
        # the group cannot overwrite them.
        os.chmod(f, 0644)

def ReleaseToTryBuilds(options, upload_dir, files):
    tryBuildsPath = TRY_DIR % {'product': options.product,
                               'who': options.who,
                               'revision': options.revision,
                               'builddir': options.builddir}
    tryBuildsUrl = TRY_URL_PATH % {'product': options.product,
                                   'who': options.who,
                                   'revision': options.revision,
                                   'builddir': options.builddir}
    for f in files:
        CopyFileToDir(f, upload_dir, tryBuildsPath)
        sys.stderr.write(
            "%s\n" % os.path.join(tryBuildsUrl, os.path.basename(f)))

if __name__ == '__main__':
    releaseTo = []
    error = False

    print >> sys.stderr, "sys.argv: %s" % sys.argv

    parser = OptionParser(usage="usage: %prog [options] <directory> <files>")
    parser.add_option("-p", "--product",
                      action="store", dest="product",
                      help="Set product name to build paths properly.")
    parser.add_option("-v", "--version",
                      action="store", dest="version",
                      help="Set version number to build paths properly.")
    parser.add_option("--nightly-dir", default="nightly",
                      action="store", dest="nightly_dir",
                      help="Set the base directory for nightlies (ie $product/$nightly_dir/), and the parent directory for release candidates (default 'nightly').")
    parser.add_option("-b", "--branch",
                      action="store", dest="branch",
                      help="Set branch name to build paths properly.")
    parser.add_option("-i", "--buildid",
                      action="store", dest="buildid",
                      help="Set buildid to build paths properly.")
    parser.add_option("-n", "--build-number",
                      action="store", dest="build_number",
                      help="Set buildid to build paths properly.")
    parser.add_option("-r", "--revision",
                      action="store", dest="revision")
    parser.add_option("-w", "--who",
                      action="store", dest="who")
    parser.add_option("-S", "--no-shortdir",
                      action="store_true", dest="noshort",
                      help="Don't symlink the short dated directories.")
    parser.add_option("--builddir",
                      action="store", dest="builddir",
                      help="Subdir to arrange packaged unittest build paths properly.")
    parser.add_option("--subdir",
                      action="store", dest="builddir",
                      help="Subdir to arrange packaged unittest build paths properly.")
    parser.add_option("--tinderbox-builds-dir",
                      action="store", dest="tinderbox_builds_dir",
                      help="Set tinderbox builds dir to build paths properly.")
    parser.add_option("-l", "--release-to-latest",
                      action="store_true", dest="release_to_latest",
                      help="Copy files to $product/$nightly_dir/latest-$branch")
    parser.add_option("-d", "--release-to-dated",
                      action="store_true", dest="release_to_dated",
                      help="Copy files to $product/$nightly_dir/$datedir-$branch")
    parser.add_option("-c", "--release-to-candidates-dir",
                      action="store_true", dest="release_to_candidates_dir",
                      help="Copy files to $product/$nightly_dir/$version-candidates/build$build_number")
    parser.add_option("--release-to-mobile-candidates-dir",
                      action="store_true", dest="release_to_mobile_candidates_dir",
                      help="Copy mobile files to $product/$nightly_dir/$version-candidates/build$build_number/$platform")
    parser.add_option("-t", "--release-to-tinderbox-builds",
                      action="store_true", dest="release_to_tinderbox_builds",
                      help="Copy files to $product/tinderbox-builds/$tinderbox_builds_dir")
    parser.add_option("--release-to-latest-tinderbox-builds",
                      action="store_true", dest="release_to_latest_tinderbox_builds",
                      help="Softlink tinderbox_builds_dir to latest")
    parser.add_option("--release-to-tinderbox-dated-builds",
                      action="store_true", dest="release_to_dated_tinderbox_builds",
                      help="Copy files to $product/tinderbox-builds/$tinderbox_builds_dir/$timestamp")
    parser.add_option("--release-to-try-builds",
                      action="store_true", dest="release_to_try_builds",
                      help="Copy files to try-builds/$who-$revision")
    parser.add_option("--signed", action="store_true", dest="signed",
                      help="Don't use unsigned directory for uploaded files")
    (options, args) = parser.parse_args()

    if len(args) < 2:
        print "Error, you must specify a directory and at least one file."
        error = True

    if not options.product:
        print "Error, you must supply the product name."
        error = True

    if options.release_to_latest:
        releaseTo.append(ReleaseToLatest)
        if not options.branch:
            print "Error, you must supply the branch name."
            error = True
    if options.release_to_dated:
        releaseTo.append(ReleaseToDated)
        if not options.branch:
            print "Error, you must supply the branch name."
            error = True
        if not options.buildid:
            print "Error, you must supply the build id."
            error = True
    if options.release_to_candidates_dir:
        releaseTo.append(ReleaseToCandidatesDir)
        if not options.version:
            print "Error, you must supply the version number."
            error = True
        if not options.build_number:
            print "Error, you must supply the build number."
            error = True
    if options.release_to_mobile_candidates_dir:
        releaseTo.append(ReleaseToMobileCandidatesDir)
        if not options.version:
            print "Error, you must supply the version number."
            error = True
        if not options.build_number:
            print "Error, you must supply the build number."
            error = True
        if not options.builddir:
            print "Error, you must supply a builddir."
            error = True
    if options.release_to_tinderbox_builds:
        releaseTo.append(ReleaseToTinderboxBuildsOverwrite)
        if not options.tinderbox_builds_dir:
            print "Error, you must supply the tinderbox builds dir."
            error = True
    if options.release_to_dated_tinderbox_builds:
        releaseTo.append(ReleaseToTinderboxBuilds)
        if not options.tinderbox_builds_dir:
            print "Error, you must supply the tinderbox builds dir."
            error = True
        if not options.buildid:
            print "Error, you must supply the build id."
            error = True
    if options.release_to_try_builds:
        releaseTo.append(ReleaseToTryBuilds)
        if not options.who:
            print "Error, must supply who"
            error = True
        if not options.revision:
            print "Error, you must supply the revision"
            error = True
        if not options.builddir:
            print "Error, you must supply the builddir"
            error = True
    if len(releaseTo) == 0:
        print "Error, you must pass a --release-to option!"
        error = True

    # Use the short revision
    if options.revision is not None:
        options.revision = options.revision[:12]

    if error:
        sys.exit(1)

    NIGHTLY_PATH = NIGHTLY_PATH % {'product': options.product,
                                   'nightly_dir': options.nightly_dir}
    CANDIDATES_PATH = CANDIDATES_PATH % {'product': options.product}
    upload_dir = os.path.abspath(args[0])
    files = args[1:]
    if not os.path.isdir(upload_dir):
        print "Error, %s is not a directory!" % upload_dir
        sys.exit(1)
    for f in files:
        f = os.path.abspath(f)
        if not os.path.isfile(f):
            print "Error, %s is not a file!" % f
            sys.exit(1)

    for func in releaseTo:
        func(options, upload_dir, files)
