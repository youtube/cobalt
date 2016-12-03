#!/bin/env python
#
# Breakpad symbol cleanup
#
# When the Breakpad symbol store gets built as part of the
# Mozilla build process, it creates a file listing the symbols
# for that particular build.  The files are named:
# ${product}-${version}-${OS_ARCH}-${BUILD_ID}[-${BUILDID_EXTRA}]-symbols.txt, i.e.:
# firefox-3.0a5pre-WINNT-2007042804-symbols.txt
# $BUILDID_EXTRA is an optional identifier for feature branches, etc.
#
# This script, given a path to a symbol store, removes symbols
# for the oldest builds there.

import os
import os.path
import sys
import re
from datetime import datetime, timedelta
from optparse import OptionParser

# options, tweak as desired
# maximum number of nightlies to keep per branch
nightliesPerBin = 30
# maximum age permitted for a set of symbols, in days.
# used to clean up old feature branches, for example
maxNightlyAge = timedelta(45)
# end options

# RE to get the version number without alpha/beta designation
versionRE = re.compile("^(\d+\.\d+)")

parser = OptionParser(usage="usage: %prog [options] <symbol path> [symbol indexes to remove]")
parser.add_option("-d", "--dry-run",
                  action="store_true", dest="dry_run", default=False,
                  help="Don't delete anything, just print a list of actions")
parser.add_option("-r", "--remove-these-symbols",
                  action="store_true", dest="remove_symbols",
                  help="Remove specified symbol indexes and their contained symbols")
(options, args) = parser.parse_args()

if not args:
    print >>sys.stderr, "Must specify a symbol path!"
    sys.exit(1)
symbolPath = args[0]
if (options.remove_symbols and len(args) < 2) or (not options.remove_symbols and len(args) > 1):
    print >>sys.stderr, "Bad commandline"
    sys.exit(1)

symbols_to_remove = set()
if options.remove_symbols:
    symbols_to_remove = set(os.path.basename(a) for a in args[1:])

# Cheezy atom implementation, so we don't have to store symbol filenames
# multiple times.
atoms = []
atomdict = {}


def atomize(s):
    if s in atomdict:
        return atomdict[s]
    a = len(atoms)
    atoms.append(s)
    atomdict[s] = a
    return a


def sortByBuildID(x, y):
    "Sort two symbol index filenames by the Build IDs contained within"
    (a, b) = (os.path.basename(x).split('-')[3],
              os.path.basename(y).split('-')[3])
    return cmp(a, b)

buildidRE = re.compile("(\d\d\d\d)(\d\d)(\d\d)(\d\d)")


def datetimefrombuildid(f):
    """Given a symbol index filename, return a datetime representing the
    Build ID contained within it."""
    m = buildidRE.match(os.path.basename(f).split('-')[3])
    if m:
        return datetime(*[int(x) for x in m.groups()])
    # punt
    return datetime.now()


def adddefault(d, key, default):
    "If d[key] does not exist, set d[key] = default."
    if key not in d:
        d[key] = default


def addFiles(symbolindex, filesDict):
    """Return a list of atoms representing the symbols in this index file.
Also add 1 to filesDict[atom] for each symbol."""
    l = []
    try:
        sf = open(symbolindex, "r")
        for line in sf:
            a = atomize(line.rstrip())
            l.append(a)
            adddefault(filesDict, a, 0)
            filesDict[a] += 1
        sf.close()
    except IOError:
        pass
    return l


def markDeleteSymbols(symbols, filesDict):
    "Decrement reference count by one for each symbol in this symbol index."
    for a in symbols:
        filesDict[a] -= 1


def deletefile(f):
    if options.dry_run:
        print "rm ", f
    else:
        if os.path.isfile(f):
            try:
                os.unlink(f)
            except OSError:
                print >>sys.stderr, "Error removing file: ", f

builds = {}
allfiles = {}
buildfiles = {}
print "[1/4] Reading symbol index files..."
# get symbol index files, there's one per build
for f in os.listdir(symbolPath):
    if not (os.path.isfile(os.path.join(symbolPath, f)) and
            f.endswith("-symbols.txt")):
        continue
    # increment reference count of all symbol files listed in the index
    # and also keep a list of files from that index
    buildfiles[f] = addFiles(os.path.join(symbolPath, f), allfiles)
    # drop -symbols.txt
    parts = f.split("-")[:-1]
    (product, version, osName, buildId) = parts[:4]
    # extract branch
    # nightly build versions end with "pre" (older branches)
    # or "a1" (new mozilla-central) or "a2" (aurora)
    if version.endswith("a1"):
        branch = "nightly"
    elif version.endswith("a2"):
        branch = "aurora"
    elif version.endswith("pre"):
        m = versionRE.match(version)
        if m:
            branch = m.group(0)
        else:
            branch = version
    else:
	branch = "release"
    # group into bins by branch-product-os[-featurebranch]
    identifier = "%s-%s-%s" % (branch, product, osName)
    if len(parts) > 4:  # extra buildid, probably
        identifier += "-" + "-".join(parts[4:])
    adddefault(builds, identifier, [])
    builds[identifier].append(f)
    if f in symbols_to_remove:
        markDeleteSymbols(buildfiles[f], allfiles)
        deletefile(os.path.join(symbolPath,f))

print "[2/4] Looking for symbols to delete..."
if not symbols_to_remove:
    oldestdate = datetime.now() - maxNightlyAge
    for bin in builds:
	if bin.startswith("release"):
	    # Skip release builds for now
	    continue
        builds[bin].sort(sortByBuildID)
        if len(builds[bin]) > nightliesPerBin:
            # delete the oldest builds if there are too many
            for f in builds[bin][:-nightliesPerBin]:
                markDeleteSymbols(buildfiles[f], allfiles)
                deletefile(os.path.join(symbolPath,f))
            builds[bin] = builds[bin][-nightliesPerBin:]
        # now look for really old symbol files
        for f in builds[bin]:
            if datetimefrombuildid(f) < oldestdate:
                markDeleteSymbols(buildfiles[f], allfiles)
                deletefile(os.path.join(symbolPath,f))

print "[3/4] Deleting symbols..."
# now delete all files marked for deletion
for a, refcnt in allfiles.iteritems():
    if refcnt == 0:
        deletefile(os.path.join(symbolPath, atoms[a]))

print "[4/4] Pruning empty directories..."
sys.exit(0)
# now delete empty directories.
for root, dirs, files in os.walk(symbolPath, topdown=False):
    for d in dirs:
        fullpath = os.path.join(root, d)
        if len(os.listdir(fullpath)) == 0:
            if options.dry_run:
                print "rm -rf ", fullpath
            else:
                os.rmdir(fullpath)
print "Done!"
