#!/usr/bin/python
"""Script to clean up buildbot master directories"""
import os
from cPickle import load


def maybe_delete(filename, timestamp):
    """Delete filename if it's older than timestamp"""
    try:
        if os.path.getmtime(filename) < timestamp:
            os.unlink(filename)
    except OSError:
        # Ignore this error.  The file may have already been moved.
        # We'll get it next time!
        pass


def clean_dir(dirname, timestamp):
    """Delete old twisted log files, and old builder files from dirname"""
    # Clean up files older than timestamp
    files = os.listdir(dirname)
    # Look for twistd.log files
    for f in files:
        p = os.path.join(dirname, f)
        if os.path.isdir(p):
            builder_file = os.path.join(p, "builder")
            # Don't clean out non-builder directories
            if not os.path.exists(builder_file):
                continue

            try:
                builder = load(open(builder_file))
            except:
                continue

            # Don't clean out release builders
            if builder and builder.category and \
                    builder.category.find('release') > -1:
                continue

            for build in os.listdir(p):
                # Don't delete the 'builder' file
                if build == "builder":
                    continue
                build = os.path.join(p, build)
                if os.path.isfile(build):
                    maybe_delete(build, timestamp)

if __name__ == "__main__":
    import time
    from optparse import OptionParser
    parser = OptionParser()
    parser.add_option("-t", "--time", dest="time", type="int",
                      help="time, in days, for how old files have to be before being deleted",
                      default=4)

    options, args = parser.parse_args()

    if len(args) == 0:
        parser.error("Must specify at least one directory to clean up")

    for d in args:
        clean_dir(d, time.time() - options.time * 24 * 3600)
